"""
JECHT ENGINE — Protocol v1.3. The 10-step loop. Pure rules engine:
every decision below is a column lookup or arithmetic. No AI anywhere.

    python jecht_engine.py cycle      run one full cycle
    python jecht_engine.py loop       smart loop: 30 min while hunting,
                                      twice-a-day heartbeat when all booked
    python jecht_engine.py dayoff T1  driver code 0 — sit out til next workday
    python jecht_engine.py grid       fleet status grid
    python jecht_engine.py approve    send everything waiting in outbox/
    python jecht_engine.py autosend on|off   flip the kill switch

Step order per cycle:
  1  TIMERS     30-min offer timeout, 30-min claim lock, 4h ghost-book
  2  INBOX      pull sealed blocks + ratecons, apply outcomes
  3  TRUCKS     scan ready trucks (EMPTY + PAID + fresh GPS)
  4  SCRAPE     pull boards for those origins (DAT + 123LB only)
  5  STAMP      contact codes + broker blacklist/credit
  6  GRADE      pure math -> grade code 1-13
  7  MATCH      truck x load -> top matches, claim locks
  8  ROUTE      contact code 1 -> email lane, 2 -> call sheet, 0 -> skip
  9  TRUST      fake-gone re-check, snipe re-check, dodge, decline ceiling
  10 CLOSE      delivered -> history + 5% fee + billing rollup, truck reset
"""

import os
import re
import sys
import time
import datetime

import jecht_db as db
import jecht_wire as wire
import jecht_mailers as mail

OFFER_TIMEOUT_MIN = 30
CLAIM_LOCK_MIN = 30
GHOST_BOOK_HOURS = 4
GPS_STALE_HOURS = 4
TOP_LOADS_PER_TRUCK = 5
NO_ANSWER_STRIKES_DEAD = 2
SNIPE_WINDOW_MIN = 60
SNIPE_WEEK_LIMIT = 3
DECLINE_CEILING = 0.60          # 60%+ declines for 2 weeks -> email Brad (no penalty)
INSTA_DECLINE_SECONDS = 5

BRAD_EMAIL = "rotstrucking1@gmail.com"


def now():
    return datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")


def next_work_day():
    """Tomorrow — unless tomorrow is Sat/Sun, then the coming Monday."""
    d = datetime.date.today() + datetime.timedelta(days=1)
    while d.weekday() >= 5:                      # 5=Sat, 6=Sun
        d += datetime.timedelta(days=1)
    return d.isoformat()


def day_off(con, truck_number):
    """Code 0 — driver passes on today. Truck sits out until the next
    workday; the engine stops hunting for it automatically until then."""
    row = con.execute("SELECT id FROM trucks WHERE truck_number=?",
                      (str(truck_number),)).fetchone()
    if not row:
        return f"NO SUCH TRUCK: {truck_number}"
    until = next_work_day()
    con.execute("UPDATE trucks SET no_load_until=? WHERE id=?",
                (until, row[0]))
    con.commit()
    db.log(con, "DAY_OFF", truck_id=row[0], detail=f"until {until}")
    return (f"TRUCK {truck_number}: DAY OFF confirmed. "
            f"Engine resumes hunting for this truck on {until}.")


def minutes_ago(ts):
    """Tolerant timestamp age. Accepts 'YYYY-MM-DD HH:MM:SS', ISO 'T'
    separators, fractional seconds, and trailing 'Z'. Unparseable -> stale.
    GPS stamps may be local time while SQLite stamps are UTC, so we take
    whichever clock makes the stamp look freshest — a stamp can never be
    'from the future', and a fresh GPS ping is never wrongly called stale."""
    if not ts:
        return 1e9
    raw = str(ts).strip().replace("T", " ").rstrip("Z")
    t = None
    try:
        t = datetime.datetime.fromisoformat(raw)
    except ValueError:
        for fmt in ("%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M"):
            try:
                t = datetime.datetime.strptime(raw, fmt)
                break
            except ValueError:
                continue
    if t is None:
        return 1e9
    if t.tzinfo is not None:
        t = t.astimezone(datetime.timezone.utc).replace(tzinfo=None)
    age_utc = (datetime.datetime.utcnow() - t).total_seconds() / 60.0
    age_loc = (datetime.datetime.now() - t).total_seconds() / 60.0
    candidates = [a for a in (age_utc, age_loc) if a >= -1]
    return max(min(candidates), 0.0) if candidates else 0.0


# ============================================================
# STEP 1 — TIMERS
# ============================================================

def step1_timers(con):
    # offer out > 30 min, no confirmation -> truck back to EMPTY, match DEAD
    for mid, tid in con.execute("""
            SELECT m.id, m.truck_id FROM matches m
            JOIN trucks t ON t.id=m.truck_id
            WHERE m.state='SENT' AND t.status_code=1
              AND m.offer_sent_at IS NOT NULL""").fetchall():
        row = con.execute("SELECT offer_sent_at FROM matches WHERE id=?", (mid,)).fetchone()
        if minutes_ago(row[0]) > OFFER_TIMEOUT_MIN:
            con.execute("UPDATE matches SET state='DEAD' WHERE id=?", (mid,))
            db.set_status(con, tid, 0, "offer timeout 30min")
            db.log(con, "OFFER_TIMEOUT", truck_id=tid, match_id=mid)

    # claim lock expiry: claimed > 30 min with no outcome -> release
    for (mid,) in con.execute("""
            SELECT id FROM matches WHERE state='CLAIMED'
              AND claimed_at IS NOT NULL""").fetchall():
        row = con.execute("SELECT claimed_at FROM matches WHERE id=?", (mid,)).fetchone()
        if minutes_ago(row[0]) > CLAIM_LOCK_MIN:
            con.execute("UPDATE matches SET state='PROPOSED', claimed_at=NULL,"
                        " dispatcher_id=NULL WHERE id=?", (mid,))
            db.log(con, "CLAIM_EXPIRED", match_id=mid)

    # ghost book: BOOKED > 4h, no ratecon -> GHOST_BOOK flag, stays UNVERIFIED
    for mid, did, booked_at in con.execute("""
            SELECT id, dispatcher_id, booked_at FROM matches
            WHERE state='BOOKED' AND ratecon_verified=0
              AND booked_at IS NOT NULL""").fetchall():
        if minutes_ago(booked_at) > GHOST_BOOK_HOURS * 60 and did:
            already = con.execute(
                "SELECT 1 FROM log WHERE rule='GHOST_BOOK' AND match_id=?",
                (mid,)).fetchone()
            if not already:
                apply_trust_flag(con, did, 1, match_id=mid)
    con.commit()


# ============================================================
# STEP 2 — INBOX (sealed blocks + ratecons)
# ============================================================

def step2_inbox(con):
    try:
        blocks, ratecons, texts = mail.poll_inbox(con)
    except Exception as e:
        db.log(con, "INBOX_ERROR", detail=str(e)[:200])
        return

    # Driver texts relayed through Google Voice: match phone -> truck,
    # run the reply through the parser, and TEXT THE ANSWER BACK by
    # replying to the relay address. Fully hands-off SMS loop.
    for phone, sms, reply_addr in texts:
        row = con.execute(
            "SELECT t.truck_number FROM trucks t JOIN drivers d "
            "ON t.driver_id = d.id WHERE replace(replace(replace(replace("
            "COALESCE(d.phone,''),'-',''),'(',''),')',''),' ','') LIKE ?",
            (f"%{phone}",)).fetchone()
        if not row:
            db.log(con, "SMS_UNKNOWN_PHONE", detail=f"{phone} | {sms[:80]}")
            continue
        result = apply_driver_reply(con, row["truck_number"], sms)
        if result and reply_addr:
            mail.send_sms_via_gvoice(con, reply_addr, result)

    for btype, f, from_addr in blocks:
        if btype == "OUTCOME":
            apply_outcome(con, f)
        elif btype == "NEWCOMPANY":
            db.add_company(con, name=f["name"], mc_number=f["mc_number"],
                           dot_number=f.get("dot_number"), ein=f.get("ein"),
                           owner_name=f["owner_name"], owner_phone=f["owner_phone"],
                           owner_email=f.get("owner_email"),
                           factoring_company=f.get("factoring_company"))
            db.log(con, "NEWCOMPANY", detail=f["name"])
        elif btype == "NEWTRUCK":
            did = db.add_driver(con, f["company_id"], f["driver_name"],
                                f.get("driver_phone"), f.get("role_code", 2))
            db.add_truck(con, f["company_id"], f["truck_number"],
                         f["equipment_code"], driver_id=did,
                         plate=f.get("plate"), trailer_number=f.get("trailer_number"),
                         trailer_plate=f.get("trailer_plate"),
                         max_weight=f.get("max_weight", 45000),
                         hazmat=f.get("hazmat", 0), min_rpm=f.get("min_rpm", 2.0),
                         dest_pref=f.get("dest_pref"))
            db.log(con, "NEWTRUCK", detail=f["truck_number"])
        elif btype == "COMPLETED":
            apply_completed(con, f)

    for match_num, path, from_addr in ratecons:
        apply_ratecon(con, match_num, path)
    con.commit()


def apply_outcome(con, f):
    mid = int(str(f["match_id"]).lstrip("M-").lstrip("0") or 0)
    code = f["outcome_code"]
    did = f["dispatcher_id"]
    row = con.execute("SELECT truck_id, load_id, state FROM matches WHERE id=?",
                      (mid,)).fetchone()
    if not row:
        db.log(con, "OUTCOME_UNKNOWN_MATCH", detail=str(f))
        return
    tid, lid, state = row
    secs = f.get("seconds_open")
    con.execute("UPDATE matches SET seconds_open=? WHERE id=?", (secs, mid))

    if code in (1, 2):  # BOOKED
        rate = f.get("final_rate")
        if code == 1 and rate is None:
            rate = con.execute("SELECT rate FROM loads WHERE id=?", (lid,)).fetchone()[0]
        con.execute("""UPDATE matches SET state='BOOKED', reported_rate=?,
                       dispatcher_id=?, booked_at=datetime('now') WHERE id=?""",
                    (rate, did, mid))
        db.set_status(con, tid, 2, f"booked by dispatcher {did}, awaiting ratecon")
        con.execute("UPDATE trucks SET has_load=1 WHERE id=?", (tid,))
        # release this truck's other live matches back to the pool
        con.execute("""UPDATE matches SET state='DEAD'
                       WHERE truck_id=? AND id != ? AND state IN ('PROPOSED','SENT','CLAIMED')""",
                    (tid, mid))
        db.log(con, "BOOKED_UNVERIFIED", truck_id=tid, match_id=mid,
               detail=f"reported ${rate}")
    elif code == 3:  # NO ANSWER
        con.execute("UPDATE loads SET no_answer_strikes=no_answer_strikes+1 WHERE id=?", (lid,))
        strikes = con.execute("SELECT no_answer_strikes FROM loads WHERE id=?", (lid,)).fetchone()[0]
        if strikes >= NO_ANSWER_STRIKES_DEAD:
            con.execute("UPDATE loads SET alive=0 WHERE id=?", (lid,))
        con.execute("UPDATE matches SET state='DEAD' WHERE id=?", (mid,))
        db.log(con, "NO_ANSWER", load_id=lid, match_id=mid, detail=f"strike {strikes}")
    elif code == 4:  # LOAD GONE — verify next scrape (FAKE_GONE check)
        con.execute("UPDATE loads SET alive=0 WHERE id=?", (lid,))
        con.execute("UPDATE matches SET state='DEAD', dispatcher_id=? WHERE id=?", (did, mid))
        db.log(con, "LOAD_GONE_CLAIMED", load_id=lid, match_id=mid,
               detail=f"dispatcher {did}; verify on next scrape")
    elif code == 5:  # BAD BROKER
        bid = con.execute("SELECT broker_id FROM loads WHERE id=?", (lid,)).fetchone()[0]
        if bid:
            con.execute("UPDATE brokers SET blacklist=1 WHERE id=?", (bid,))
        con.execute("UPDATE matches SET state='DEAD' WHERE id=?", (mid,))
        db.log(con, "BROKER_BLACKLIST", load_id=lid, match_id=mid)
    elif code in (6, 7, 8):  # effort logged / untouched / emailed
        con.execute("UPDATE matches SET state='SENT' WHERE id=?", (mid,))
        db.log(con, db.OUTCOME_CODES[code].split(" ")[0], match_id=mid)
    elif code == 9:  # NOT INTERESTED — instant release, no penalty
        con.execute("""UPDATE matches SET state='DEAD', dispatcher_id=?,
                       declined_at=datetime('now') WHERE id=?""", (did, mid))
        if secs is not None and secs < INSTA_DECLINE_SECONDS:
            db.log(con, "INSTA_DECLINE", match_id=mid,
                   detail=f"dispatcher {did} declined in {secs}s")
        db.log(con, "NOT_INTERESTED", match_id=mid, detail=f"dispatcher {did}")


def apply_ratecon(con, match_num, path):
    row = con.execute("""SELECT id, truck_id, dispatcher_id, reported_rate
                         FROM matches WHERE id=?""", (match_num,)).fetchone()
    if not row:
        db.log(con, "RATECON_UNKNOWN_MATCH", detail=f"M-{match_num} {path}")
        return
    mid, tid, did, reported = row
    con.execute("UPDATE matches SET ratecon_path=? WHERE id=?", (path, mid))
    db.log(con, "RATECON_RECEIVED", truck_id=tid, match_id=mid, detail=path)
    # Rate extraction from the PDF is manual/agent-side; verification happens
    # when COMPLETED block or Brad's confirmation supplies ratecon_rate.


def apply_completed(con, f):
    mid = int(str(f["match_id"]).lstrip("M-").lstrip("0") or 0)
    row = con.execute("""SELECT truck_id, load_id, dispatcher_id, reported_rate
                         FROM matches WHERE id=?""", (mid,)).fetchone()
    if not row:
        db.log(con, "COMPLETED_UNKNOWN_MATCH", detail=str(f))
        return
    tid, lid, did, reported = row
    paper_rate = f["final_rate"]

    # RATE_LIE check: paper wins, always
    if did and reported is not None and abs(paper_rate - reported) > 0.01:
        apply_trust_flag(con, did, 5, match_id=mid,
                         detail=f"reported ${reported} vs ratecon ${paper_rate}")
    elif did:
        apply_trust_flag(con, did, 7, match_id=mid)  # CLEAN_BOOK +2

    con.execute("""UPDATE matches SET state='DELIVERED', final_rate=?,
                   ratecon_rate=?, ratecon_verified=? WHERE id=?""",
                (paper_rate, paper_rate, f.get("ratecon_verified", 1), mid))
    db.set_status(con, tid, 5, "delivered, closing out")
    db.log(con, "DELIVERED", truck_id=tid, match_id=mid, detail=f"${paper_rate}")


# ============================================================
# STEP 3 — READY TRUCKS
# ============================================================

def step3_trucks(con):
    """EMPTY + PAID + active + GPS fresh (<4h). Returns truck rows.
    Trucks skipped for missing/stale GPS are written to GPS_PINGS_NEEDED.txt
    so the operator knows exactly who to text from Google Voice."""
    rows = con.execute("""
        SELECT t.id, t.truck_number, t.equipment_code, t.gps_city, t.gps_state,
               t.gps_updated, t.max_weight, t.hazmat, t.min_rpm, t.dest_pref,
               t.level_code, t.company_id
        FROM trucks t JOIN companies c ON c.id=t.company_id
        WHERE t.active=1 AND t.has_load=0 AND t.status_code=0 AND c.paid=1
          AND (t.no_load_until IS NULL OR t.no_load_until=''
               OR date('now','localtime') >= t.no_load_until)
        """).fetchall()
    ready, need_ping = [], []
    for r in rows:
        if not r[3] or not r[4]:
            db.log(con, "NO_GPS", truck_id=r[0])
            need_ping.append((r[0], r[1], "no location on file"))
            continue
        if minutes_ago(r[5]) > GPS_STALE_HOURS * 60:
            db.log(con, "GPS_STALE", truck_id=r[0], detail=r[5] or "never")
            need_ping.append((r[0], r[1], f"stale since {r[5] or 'never'}"
                                          f" (last: {r[3]}, {r[4]})"))
            continue
        ready.append(r)
    _write_gps_pings(con, need_ping)
    return ready


def _write_gps_pings(con, need_ping):
    """GPS_PINGS_NEEDED.txt — who to text from the Google Voice site."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "GPS_PINGS_NEEDED.txt")
    try:
        if not need_ping:
            if os.path.exists(path):
                os.remove(path)          # all clear — no stale file lying around
            return
        lines = [f"JECHT // GPS PINGS NEEDED — {now()}",
                 f"{len(need_ping)} empty truck(s) blocked: no fresh location.",
                 "Text each driver from Google Voice (copy/paste):",
                 "-" * 52,
                 "JECHT: you're empty. Where to? Tap your answer:",
                 "  https://rotstrucking1-prog.github.io/Rotstrucking-app1/driver/",
                 "  (hit COPY RESPONSE, paste it back here)",
                 "Or reply ONE thing:",
                 "  1 = Best money (any direction)",
                 "  2 = Home",
                 "  0 = Day off (back tomorrow, Monday if weekend)",
                 "  or text a city & state, e.g. Tulsa OK",
                 "-" * 52,
                 "Feed each reply back with:",
                 '  python jecht_engine.py reply <truck#> "<their text>"',
                 ""]
        for tid, tnum, why in need_ping:
            row = con.execute(
                "SELECT d.name, d.phone FROM trucks t "
                "LEFT JOIN drivers d ON d.id=t.driver_id WHERE t.id=?",
                (tid,)).fetchone()
            name = (row[0] if row and row[0] else "driver?")
            phone = (row[1] if row and row[1] else "NO PHONE ON FILE")
            lines.append(f"  TRUCK {tnum:<8} {name:<20} {phone:<15} — {why}")
        open(path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
        print(f"[GPS] {len(need_ping)} truck(s) need a location text"
              f" -> {path}")
    except Exception as e:
        db.log(con, "GPS_PING_FILE_FAIL", detail=str(e)[:150])


# 50-state + DC lookup. Pure table — one answer, always. NOT guessing.
STATE_NAMES = {
    "alabama": "AL", "alaska": "AK", "arizona": "AZ", "arkansas": "AR",
    "california": "CA", "colorado": "CO", "connecticut": "CT",
    "delaware": "DE", "florida": "FL", "georgia": "GA", "hawaii": "HI",
    "idaho": "ID", "illinois": "IL", "indiana": "IN", "iowa": "IA",
    "kansas": "KS", "kentucky": "KY", "louisiana": "LA", "maine": "ME",
    "maryland": "MD", "massachusetts": "MA", "michigan": "MI",
    "minnesota": "MN", "mississippi": "MS", "missouri": "MO",
    "montana": "MT", "nebraska": "NE", "nevada": "NV",
    "new hampshire": "NH", "new jersey": "NJ", "new mexico": "NM",
    "new york": "NY", "north carolina": "NC", "north dakota": "ND",
    "ohio": "OH", "oklahoma": "OK", "oregon": "OR", "pennsylvania": "PA",
    "rhode island": "RI", "south carolina": "SC", "south dakota": "SD",
    "tennessee": "TN", "texas": "TX", "utah": "UT", "vermont": "VT",
    "virginia": "VA", "washington": "WA", "west virginia": "WV",
    "wisconsin": "WI", "wyoming": "WY", "district of columbia": "DC"}
STATE_CODES = set(STATE_NAMES.values())

DRIVER_LINK_URL = "https://rotstrucking1-prog.github.io/Rotstrucking-app1/driver/"

DRIVER_MENU = ("JECHT: Where to? Tap your answer here: "
               + DRIVER_LINK_URL +
               "  (hit COPY RESPONSE, paste it back)  "
               "Or reply ONE thing:  1 = Best money"
               "  2 = Home  0 = Day off  or text a city & state"
               " (e.g. Tulsa OK)")


def _parse_place(s):
    """Deterministic city+state extraction. Accepts 'Tulsa, OK',
    'tulsa ok', 'Burns Harbor Indiana', 'New Castle New Hampshire'.
    Stray digit tokens (menu numbers mixed in) are pulled out and
    returned separately. Returns (city, ST, stray_digit) or (None,None,d)."""
    toks = re.sub(r"[.,]", " ", str(s)).split()
    stray = None
    toks2 = []
    for t in toks:
        if t.isdigit():
            stray = t          # remember it, never mix into the place
        else:
            toks2.append(t)
    toks = toks2
    # state = last two words (e.g. 'new york'), last one word, or 2-letter code
    for n in (2, 1):
        if len(toks) > n:
            cand = " ".join(toks[-n:]).lower()
            if cand in STATE_NAMES:
                return " ".join(toks[:-n]).title(), STATE_NAMES[cand], stray
    if len(toks) >= 2 and len(toks[-1]) == 2 and toks[-1].upper() in STATE_CODES:
        return " ".join(toks[:-1]).title(), toks[-1].upper(), stray
    return None, None, stray


def apply_driver_reply(con, truck_number, text):
    """ONE-ANSWER driver protocol. GPS is automatic (set at close-out from
    the drop city) and level is dispatch's call — the driver only answers
    'where to?'. Deterministic ladder, no guessing:
      1. whole message is a single code -> 0=day off, 1=MONEY, 2=HOME
      2. otherwise parse as a place (full state names via lookup table;
         stray menu digits are ignored when a real place is found)
      3. place failed but a stray code 0/1/2 was present -> use the code
      4. nothing parsed -> refuse, write nothing, re-show the menu
    Legacy 'city, st / dest / level' slash format still accepted."""
    row = con.execute("SELECT id FROM trucks WHERE truck_number=?",
                      (str(truck_number),)).fetchone()
    if not row:
        return f"NO SUCH TRUCK: {truck_number}"
    tid = row[0]
    raw = str(text).strip()

    def _set(out, **sets):
        cols = ", ".join(f"{k}=?" for k in sets)
        con.execute(f"UPDATE trucks SET {cols} WHERE id=?",
                    (*sets.values(), tid))
        con.commit()
        db.log(con, "DRIVER_REPLY", truck_id=tid, detail=raw[:150])
        return f"TRUCK {truck_number}: {out}"

    def _code(c):
        if c == "0":
            return day_off(con, truck_number)
        if c == "1":
            return _set("DEST -> MONEY (best paying, any direction)",
                        dest_pref="MONEY")
        if c == "2":
            return _set("DEST -> HOME", dest_pref="HOME")
        return None

    # rung 0 — DRIVER LINK wire code: "JECHT D=<dest> [H=<hours>]"
    # Produced by JECHT_DRIVER.html (tap-only page). Machine-built, so it is
    # the most trusted format: exact tokens, zero interpretation.
    if raw.upper().startswith("JECHT"):
        toks = raw.upper().replace("JECHT", "", 1).strip()
        m = re.match(r"^D=(.+?)(?:\s+H=(\d{1,2}(?:\.\d+)?))?(?:\s+L=([1-4]))?$",
                     toks)
        if not m:
            db.log(con, "DRIVER_REPLY_REJECT", truck_id=tid, detail=raw[:150])
            return (f"TRUCK {truck_number}: BAD WIRE CODE '{raw}' — nothing "
                    "changed. Re-open your Jecht Driver Link and copy again.")
        d, h, lvl_req = m.group(1).strip(), m.group(2), m.group(3)
        hrs = min(float(h), 14.0) if h else None
        # LEVEL REQUEST: driver may ASK for a level, but the truck's
        # level_cap (set only by dispatch) is the law. Requests above the
        # cap are clamped — silently safe, loudly logged.
        lvl_note = ""
        if lvl_req:
            want = int(lvl_req)
            cap = con.execute("SELECT COALESCE(level_cap, level_code) AS c "
                              "FROM trucks WHERE id=?", (tid,)).fetchone()["c"]
            grant = min(want, int(cap or 1))
            con.execute("UPDATE trucks SET level_code=? WHERE id=?",
                        (grant, tid))
            names = {1: "SAFE", 2: "MODERATE", 3: "AGGRESSIVE", 4: "OUTLAW"}
            if grant < want:
                lvl_note = (f" | LEVEL {names[want]} DENIED — dispatch cap is "
                            f"{names[grant]}. Running {names[grant]}.")
                db.log(con, "LEVEL_DENIED", truck_id=tid,
                       detail=f"asked {want}, cap {cap}")
            else:
                lvl_note = f" | LEVEL -> {names[grant]}"
                db.log(con, "LEVEL_SET", truck_id=tid, detail=names[grant])
        if d == "DAYOFF":
            return day_off(con, truck_number)
        if d == "MONEY":
            return _set("DEST -> MONEY (best paying, any direction)"
                        + (f" | DRIVE TIME {hrs:g}h" if hrs is not None else "")
                        + lvl_note,
                        dest_pref="MONEY", drive_hours_left=hrs)
        if d == "HOME":
            return _set("DEST -> HOME"
                        + (f" | DRIVE TIME {hrs:g}h" if hrs is not None else "")
                        + lvl_note,
                        dest_pref="HOME", drive_hours_left=hrs)
        if "," in d:
            city, st = d.rsplit(",", 1)
            city, st = city.strip().title(), st.strip().upper()
            if len(st) == 2 and city:
                return _set(f"DEST -> {city}, {st}"
                            + (f" | DRIVE TIME {hrs:g}h" if hrs is not None else "")
                            + lvl_note,
                            dest_pref=f"{city}, {st}", drive_hours_left=hrs)
        db.log(con, "DRIVER_REPLY_REJECT", truck_id=tid, detail=raw[:150])
        return (f"TRUCK {truck_number}: BAD WIRE CODE '{raw}' — nothing "
                "changed. Re-open your Jecht Driver Link and copy again.")

    # legacy slash format: "city, st / dest / level"
    if "/" in raw:
        return _legacy_slash_reply(con, tid, truck_number, raw)

    # rung 1 — bare code or exact word synonym (fixed table, no guessing)
    SYNONYMS = {"zero": "0", "day off": "0", "dayoff": "0", "off": "0",
                "money": "1", "best money": "1",
                "home": "2", "going home": "2", "go home": "2",
                "take me home": "2", "house": "2"}
    bare = raw.rstrip(".!").strip().lower()
    if bare in ("0", "1", "2") or bare in SYNONYMS:
        r = _code(SYNONYMS.get(bare, bare))
        if r:
            return r

    # rung 2 — place (place wins over stray digits)
    city, st, stray = _parse_place(raw)
    if city and st:
        note = f" (ignored stray '{stray}')" if stray else ""
        return _set(f"DEST -> {city}, {st}{note}", dest_pref=f"{city}, {st}")

    # rung 3 — no place, but a clean code was buried in the text
    if stray in ("0", "1", "2"):
        r = _code(stray)
        if r:
            return r

    # rung 4 — refuse, write nothing, re-show menu
    db.log(con, "DRIVER_REPLY_REJECT", truck_id=tid, detail=raw[:150])
    return (f"TRUCK {truck_number}: COULD NOT READ '{raw}' — nothing changed. "
            + DRIVER_MENU)


def _legacy_slash_reply(con, tid, truck_number, raw):
    """Old three-part format, kept for power users:
    'city, st / where-to / level'. Same zero-guessing rules."""
    parts = [p.strip() for p in raw.split("/") if p.strip()]
    out, sets = [], {}
    if parts and parts[0].rstrip(".").lower() in ("0", "zero"):
        return day_off(con, truck_number)

    if parts:
        c, s, _ = _parse_place(parts[0])
        if c and s:
            sets["gps_city"], sets["gps_state"] = c, s
            sets["gps_updated"] = now()
            out.append(f"GPS -> {c}, {s}")
        else:
            out.append(f"COULD NOT READ LOCATION: '{parts[0]}'")

    if len(parts) > 1:
        p = parts[1].lower()
        if p in ("1", "money"):
            sets["dest_pref"] = "MONEY"
            out.append("DEST -> MONEY (best paying, any direction)")
        elif p in ("3", "home"):
            sets["dest_pref"] = "HOME"
            out.append("DEST -> HOME")
        else:
            c, s, _ = _parse_place(parts[1])
            if c and s:
                sets["dest_pref"] = f"{c}, {s}"
                out.append(f"DEST -> {c}, {s}")
            else:
                out.append(f"COULD NOT READ DESTINATION: '{parts[1]}'")

    if len(parts) > 2:
        p = parts[2].lower()
        names = {v[0].lower(): k for k, v in db.LEVEL_CODES.items()}
        lvl = int(p) if p in ("1", "2", "3", "4") else names.get(p)
        if lvl:
            sets["level_code"] = lvl
            out.append(f"LEVEL -> {lvl} {db.LEVEL_CODES[lvl][0]}")
        else:
            out.append(f"COULD NOT READ LEVEL: '{parts[2]}'")

    if sets:
        cols = ", ".join(f"{k}=?" for k in sets)
        con.execute(f"UPDATE trucks SET {cols} WHERE id=?",
                    (*sets.values(), tid))
        con.commit()
        db.log(con, "DRIVER_REPLY", truck_id=tid, detail=raw[:150])
    return f"TRUCK {truck_number}: " + " | ".join(out)


# ============================================================
# STEP 4 — SCRAPE (DAT + 123LB only; Trulos permanently banned)
# ============================================================

def _scrape_pass(con, lb, trucks):
    """One scrape sweep over every unique origin. Returns (loads, errors)."""
    results, errors = [], []
    seen_origins = set()
    for t in trucks:
        key = (t[3].lower(), t[4].lower(), t[2])
        if key in seen_origins:
            continue
        seen_origins.add(key)
        equip = db.EQUIPMENT_CODES[t[2]][0]
        try:
            loads = lb.search_loads_all(origin_city=t[3], origin_state=t[4],
                                        equipment=equip)
            results.extend(loads or [])
        except Exception as e:
            errors.append(f"{t[3]}, {t[4]} [{equip}]: {str(e)[:120]}")
            db.log(con, "SCRAPE_FAIL", truck_id=t[0], detail=str(e)[:200])
    return results, errors


def _diagnose_scraper(lb):
    """Self-diagnosis checklist. Pure checks, no guessing."""
    checks = []
    for label, fn in (("123LB credentials", "load_creds"),
                      ("DAT credentials", "_dat_creds")):
        try:
            got = getattr(lb, fn)() if hasattr(lb, fn) else None
            checks.append(f"{label}: {'OK' if got else 'MISSING'}")
        except Exception as e:
            checks.append(f"{label}: ERROR {str(e)[:80]}")
    try:
        b = None
        for fn in ("_find_chrome_binary", "_find_edge_binary"):
            if hasattr(lb, fn):
                b = getattr(lb, fn)()
                if b:
                    break
        checks.append(f"browser: {b or 'NOT FOUND'}")
    except Exception as e:
        checks.append(f"browser: ERROR {str(e)[:80]}")
    try:
        import socket
        socket.create_connection(("one.dat.com", 443), timeout=6).close()
        checks.append("network: OK")
    except Exception as e:
        checks.append(f"network: DOWN ({str(e)[:60]})")
    return checks


def _scraper_health_file(con, found, errors, checks, streak):
    """SCRAPER_HEALTH.txt — written when something's wrong, removed when not."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "SCRAPER_HEALTH.txt")
    try:
        if found and not errors:
            if os.path.exists(path):
                os.remove(path)
            return
        lines = [f"JECHT // SCRAPER HEALTH — {now()}",
                 f"loads found this cycle: {found}",
                 f"zero-load cycles in a row: {streak}",
                 "-" * 52]
        if checks:
            lines += ["SELF-DIAGNOSIS:"] + [f"  {c}" for c in checks]
        if errors:
            lines += ["ERRORS:"] + [f"  {e}" for e in errors]
        if streak >= 2:
            lines += ["-" * 52,
                      "ACTION NEEDED: two+ empty sweeps in a row.",
                      "  1. Open DAT and 123Loadboard in a browser, log in",
                      "  2. Re-save fresh debug dumps if the layout changed",
                      "  3. Run: python jecht_engine.py cycle  to re-test"]
        open(path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
        print(f"[SCRAPER] health report -> {path}")
    except Exception as e:
        db.log(con, "SCRAPER_HEALTH_FILE_FAIL", detail=str(e)[:150])


def step4_scrape(con, trucks):
    """Scrape with self-diagnosis: a zero-load sweep while empty trucks are
    waiting is never normal — diagnose, retry once, report. No silent fails."""
    try:
        import loadboard_scraper as lb
    except Exception as e:
        db.log(con, "SCRAPER_IMPORT_FAIL", detail=str(e)[:200])
        _scraper_health_file(con, 0, [f"scraper import failed: {e}"], [], 0)
        return []
    results, errors = _scrape_pass(con, lb, trucks)
    checks = []
    if trucks and not results:
        checks = _diagnose_scraper(lb)
        db.log(con, "SCRAPE_SELF_DIAG", detail="; ".join(checks)[:280])
        results, retry_errors = _scrape_pass(con, lb, trucks)   # one retry
        errors += retry_errors
    streak = int(db.meta_get(con, "scrape_zero_streak", "0") or 0)
    streak = streak + 1 if (trucks and not results) else 0
    db.meta_set(con, "scrape_zero_streak", str(streak))
    _scraper_health_file(con, len(results), errors, checks, streak)
    return results


def step4_intake(con, scraped):
    """Insert scraped loads. Returns count of new rows."""
    n = 0
    for L in scraped:
        try:
            con.execute("""INSERT OR IGNORE INTO loads
                (source, external_ref, origin_city, origin_state, dest_city,
                 dest_state, miles, rate, rpm, weight, equipment_code,
                 broker_name, broker_phone, broker_email)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)""",
                (L.get("source", "?"), L.get("ref") or L.get("external_ref"),
                 L.get("origin_city"), L.get("origin_state"),
                 L.get("dest_city"), L.get("dest_state"),
                 L.get("miles"), L.get("rate"), L.get("rpm"),
                 L.get("weight"), L.get("equipment_code"),
                 L.get("broker_name") or L.get("broker"),
                 L.get("broker_phone") or L.get("phone"),
                 L.get("broker_email") or L.get("email")))
            n += con.total_changes and 1
        except Exception as e:
            db.log(con, "INTAKE_FAIL", detail=str(e)[:150])
    con.commit()
    return n


# ============================================================
# STEP 5 — STAMP contact codes + broker check
# ============================================================

def step5_stamp(con):
    for lid, phone, mail_, bname in con.execute(
            "SELECT id, broker_phone, broker_email, broker_name FROM loads"
            " WHERE alive=1 AND contact_code=0").fetchall():
        # link/learn broker row by name
        bid = None
        if bname:
            row = con.execute("SELECT id, blacklist FROM brokers WHERE name=?",
                              (bname,)).fetchone()
            if row:
                bid, bl = row
                if bl:
                    con.execute("UPDATE loads SET broker_id=?, contact_code=0,"
                                " alive=0 WHERE id=?", (bid, lid))
                    db.log(con, "BLACKLIST_SKIP", load_id=lid, detail=bname)
                    continue
            else:
                cur = con.execute(
                    "INSERT INTO brokers(name, phone, email) VALUES (?,?,?)",
                    (bname, phone, mail_))
                bid = cur.lastrowid
        code = 1 if (mail_ and "@" in mail_) else (2 if phone else 0)
        con.execute("UPDATE loads SET contact_code=?, broker_id=? WHERE id=?",
                    (code, bid, lid))
        if code == 0:
            db.log(con, "SKIP_NO_CONTACT", load_id=lid)
    con.commit()


# ============================================================
# STEP 6 — GRADE (pure math)
# ============================================================

RPM_GRADE = [  # (min_rpm, grade_code) first hit wins
    (3.50, 1), (3.25, 2), (3.00, 3), (2.75, 4), (2.50, 5), (2.30, 6),
    (2.10, 7), (2.00, 8), (1.90, 9), (1.80, 10), (1.70, 11), (1.60, 12),
]


def grade_load(rpm, broker_credit, days_to_pay):
    g = 13
    for floor, code in RPM_GRADE:
        if rpm is not None and rpm >= floor:
            g = code
            break
    # unknown credit = +1 grade worse; slow pay (>45 days) = +2 worse
    if not broker_credit:
        g = min(13, g + 1)
    try:
        if days_to_pay and float(str(days_to_pay).strip("+ days")) > 45:
            g = min(13, g + 2)
    except ValueError:
        pass
    return g


def step6_grade(con):
    for lid, rpm, rate, miles, bid in con.execute(
            "SELECT id, rpm, rate, miles, broker_id FROM loads"
            " WHERE alive=1 AND grade_code IS NULL").fetchall():
        if rpm is None and rate and miles:
            rpm = rate / miles
            con.execute("UPDATE loads SET rpm=? WHERE id=?", (round(rpm, 2), lid))
        credit, dtp = None, None
        if bid:
            row = con.execute("SELECT credit_score, days_to_pay FROM brokers"
                              " WHERE id=?", (bid,)).fetchone()
            if row:
                credit, dtp = row
        con.execute("UPDATE loads SET grade_code=? WHERE id=?",
                    (grade_load(rpm, credit, dtp), lid))
    con.commit()


# ============================================================
# STEP 7 — MATCH
# ============================================================

FUEL_FLOOR_FACTOR = 0.74   # base floor = diesel $/gal x 0.74
                           # ($4.75 diesel -> $3.50/mi)
# Levels relax the RATIO, never underbid a posted rate:
#   SAFE/MODERATE 74% | AGGRESSIVE 64% (74-10) | OUTLAW 59% (74-15)
FUEL_LEVEL_CUT = {1: 0.00, 2: 0.00, 3: 0.10, 4: 0.15}


def fuel_price(con):
    row = con.execute(
        "SELECT value FROM meta WHERE key='fuel_price'").fetchone()
    try:
        return float(row[0]) if row else 0.0
    except (TypeError, ValueError):
        return 0.0


def fuel_floor(con, level_code=2):
    """Minimum RPM from current diesel price for a dispatch level.
    0 = price not set, floor inactive."""
    price = fuel_price(con)
    if price <= 0:
        return 0.0
    ratio = FUEL_FLOOR_FACTOR - FUEL_LEVEL_CUT.get(level_code or 2, 0.0)
    return round(price * ratio, 2)


def step7_match(con, trucks):
    made = 0
    for (tid, tnum, equip, gcity, gstate, _gu, maxw, hazmat, min_rpm,
         dest_pref, level_code, co_id) in trucks:
        name, max_grade, floor_mult, max_day_mi = db.LEVEL_CODES.get(
            level_code or 2, db.LEVEL_CODES[2])
        candidates = con.execute("""
            SELECT l.id, l.grade_code, l.rpm, l.dest_state, l.rate
            FROM loads l
            WHERE l.alive=1 AND l.contact_code != 0
              AND l.equipment_code=? AND lower(l.origin_state)=lower(?)
              AND (l.weight IS NULL OR l.weight <= ?)
              AND l.rpm >= ?
              AND l.grade_code <= ?
              AND (l.miles IS NULL OR l.miles <= ?)
              AND l.id NOT IN (SELECT load_id FROM matches
                               WHERE state IN ('PROPOSED','SENT','CLAIMED','BOOKED'))
            ORDER BY l.grade_code ASC, l.rpm DESC
            LIMIT ?""", (equip, gstate, maxw,
                         max(min_rpm * floor_mult,
                             fuel_floor(con, level_code)),
                         max_grade, max_day_mi,
                         TOP_LOADS_PER_TRUCK * 2)).fetchall()
        # destination preference: stable sort, preferred first
        pref = (dest_pref or "").strip()
        if pref.upper() == "MONEY":
            # chase the check — biggest total rate first
            candidates.sort(key=lambda c: -(c[4] or 0))
        elif pref.upper() == "HOME":
            hrow = con.execute("SELECT home_city, home_state FROM companies"
                               " WHERE id=?", (co_id,)).fetchone()
            hstate = (hrow[1] or "").lower() if hrow else ""
            if hstate:
                candidates.sort(
                    key=lambda c: 0 if (c[3] or "").lower() == hstate else 1)
        elif pref:
            prefs = [p.strip().lower() for p in pref.split(",")]
            candidates.sort(key=lambda c: 0 if (c[3] or "").lower() in prefs else 1)
        for lid, *_ in candidates[:TOP_LOADS_PER_TRUCK]:
            con.execute("INSERT INTO matches(truck_id, load_id) VALUES (?,?)",
                        (tid, lid))
            made += 1
        if candidates:
            db.log(con, "MATCHED", truck_id=tid,
                   detail=f"{min(len(candidates), TOP_LOADS_PER_TRUCK)} loads")
    con.commit()
    return made


# ============================================================
# STEP 8 — ROUTE by contact code, bundle, send
# ============================================================

def _match_block(con, mid, btype):
    r = con.execute("""
        SELECT m.id, t.id, t.truck_number, t.equipment_code, t.gps_city,
               t.gps_state, t.min_rpm, l.id, l.origin_city, l.origin_state,
               l.dest_city, l.dest_state, l.miles, l.rate, l.rpm,
               l.grade_code, l.broker_name, l.broker_phone, l.broker_email
        FROM matches m JOIN trucks t ON t.id=m.truck_id
        JOIN loads l ON l.id=m.load_id WHERE m.id=?""", (mid,)).fetchone()
    (mid, tid, tnum, eq, gc, gs, floor, lid, oc, os_, dc, ds, miles, rate,
     rpm, grade, bname, bphone, bemail) = r
    f = {"match_id": mail.match_tag(mid), "truck_id": tid, "truck_number": tnum,
         "equipment_code": eq, "gps_city": gc or "?", "gps_state": gs or "?",
         "load_id": lid, "origin": f"{oc}, {os_}", "dest": f"{dc}, {ds}",
         "miles": miles or 0, "rate": rate or 0, "rpm": rpm or 0,
         "grade_code": grade or 13, "broker_name": bname or "?",
         "floor_rpm": floor or 2.0}
    if btype == "DISPATCH":
        f["broker_email"] = bemail or "?"
    else:
        f["broker_phone"] = bphone or "?"
    return wire.build(btype, f)


def step8_route(con):
    # which dispatcher handles email loads? (handles_email=1, active, not kicked)
    email_dispatcher = con.execute(
        "SELECT id, name, email FROM dispatchers WHERE active=1 AND kicked=0"
        " AND handles_email=1 LIMIT 1").fetchone()
    call_dispatchers = con.execute(
        "SELECT id, name, email FROM dispatchers WHERE active=1 AND kicked=0"
        " ORDER BY trust DESC").fetchall()

    email_blocks, call_blocks = [], []
    for mid, lid, ccode in con.execute("""
            SELECT m.id, l.id, l.contact_code FROM matches m
            JOIN loads l ON l.id=m.load_id WHERE m.state='PROPOSED'""").fetchall():
        if ccode == 1:
            email_blocks.append(_match_block(con, mid, "DISPATCH"))
        elif ccode == 2:
            call_blocks.append(_match_block(con, mid, "CALLSHEET"))
        con.execute("UPDATE matches SET state='SENT',"
                    " offer_sent_at=datetime('now') WHERE id=?", (mid,))

    sent = []
    if email_blocks:
        if email_dispatcher:
            sent.append(mail.call_sheet_email(con, email_dispatcher,
                                              "\n\n".join(email_blocks),
                                              len(email_blocks)))
        else:
            sent.append(mail.need_dispatch_email(con, "\n\n".join(email_blocks),
                                                 len(email_blocks)))
    if call_blocks:
        if call_dispatchers:
            d = call_dispatchers[0]  # highest trust gets the sheet
            trust_line = wire.build("TRUST", {"dispatcher_id": d[0],
                "trust": con.execute("SELECT trust FROM dispatchers WHERE id=?",
                                     (d[0],)).fetchone()[0]})
            sent.append(mail.call_sheet_email(
                con, d, trust_line + "\n\n" + "\n\n".join(call_blocks),
                len(call_blocks)))
        else:
            db.log(con, "NO_DISPATCHER", detail=f"{len(call_blocks)} call loads waiting")
    con.commit()
    return sent


# ============================================================
# STEP 9 — TRUST (counters only, receipts in log)
# ============================================================

def apply_trust_flag(con, dispatcher_id, flag_code, match_id=None, detail=""):
    name, points, desc = db.TRUST_FLAG_CODES[flag_code]
    con.execute("UPDATE dispatchers SET trust=MAX(0,MIN(100,trust+?)) WHERE id=?",
                (points, dispatcher_id))
    if flag_code == 4:
        con.execute("UPDATE dispatchers SET stolen_flags=stolen_flags+1 WHERE id=?",
                    (dispatcher_id,))
    db.log(con, name, match_id=match_id,
           detail=f"dispatcher {dispatcher_id} {points:+d} | {detail or desc}")

    trust, stolen, kicked = con.execute(
        "SELECT trust, stolen_flags, kicked FROM dispatchers WHERE id=?",
        (dispatcher_id,)).fetchone()
    if not kicked and (trust <= db.TRUST_KICK or stolen >= 2):
        con.execute("UPDATE dispatchers SET active=0, kicked=1 WHERE id=?",
                    (dispatcher_id,))
        mail.kill_email(con, dispatcher_id,
                        reason_code=4 if stolen >= 2 else 1)
        db.log(con, "DISPATCHER_KICKED",
               detail=f"dispatcher {dispatcher_id} trust={trust} stolen={stolen}")
    con.commit()


def step9_trust(con):
    # FAKE_GONE: load claimed gone but seen alive in a scrape AFTER the claim
    for mid, lid, did, claim_ts in con.execute("""
            SELECT m.id, m.load_id, m.dispatcher_id, l.scraped FROM matches m
            JOIN loads l ON l.id=m.load_id
            WHERE m.dispatcher_id IS NOT NULL AND m.state='DEAD'""").fetchall():
        gone_claim = con.execute(
            "SELECT ts FROM log WHERE rule='LOAD_GONE_CLAIMED' AND match_id=?",
            (mid,)).fetchone()
        flagged = con.execute(
            "SELECT 1 FROM log WHERE rule='FAKE_GONE' AND match_id=?", (mid,)).fetchone()
        if gone_claim and not flagged and claim_ts and gone_claim[0] < claim_ts:
            apply_trust_flag(con, did, 2, match_id=mid)

    # SNIPE: 3+ NOT INTERESTED loads that vanished within 1h, in 7 days
    for (did,) in con.execute("SELECT id FROM dispatchers WHERE active=1").fetchall():
        snipes = con.execute("""
            SELECT COUNT(*) FROM matches m JOIN loads l ON l.id=m.load_id
            WHERE m.dispatcher_id=? AND m.declined_at IS NOT NULL
              AND m.declined_at > datetime('now','-7 days')
              AND l.alive=0
              AND (julianday(l.scraped) - julianday(m.declined_at)) * 1440
                  BETWEEN 0 AND ?""", (did, SNIPE_WINDOW_MIN)).fetchone()[0]
        flagged_this_week = con.execute(
            "SELECT 1 FROM log WHERE rule='SNIPE' AND ts > datetime('now','-7 days')"
            " AND detail LIKE ?", (f"dispatcher {did} %",)).fetchone()
        if snipes >= SNIPE_WEEK_LIMIT and not flagged_this_week:
            apply_trust_flag(con, did, 6, detail=f"dispatcher {did} {snipes} snipes/7d")

    # DECLINE CEILING: 60%+ declines for 14 days -> email Brad, no penalty
    for did, name in con.execute(
            "SELECT id, name FROM dispatchers WHERE active=1").fetchall():
        total, declined = con.execute("""
            SELECT COUNT(*),
                   SUM(CASE WHEN declined_at IS NOT NULL THEN 1 ELSE 0 END)
            FROM matches WHERE dispatcher_id=?
              AND created > datetime('now','-14 days')""", (did,)).fetchone()
        if total and total >= 10 and (declined or 0) / total >= DECLINE_CEILING:
            already = con.execute(
                "SELECT 1 FROM log WHERE rule='DECLINE_CEILING'"
                " AND ts > datetime('now','-14 days') AND detail LIKE ?",
                (f"%{name}%",)).fetchone()
            if not already:
                db.log(con, "DECLINE_CEILING", detail=f"{name} {declined}/{total}")
                mail.send_mail(con, BRAD_EMAIL,
                               f"JECHT: {name} declining {declined}/{total} loads",
                               f"{name} has declined {declined} of {total} offered"
                               f" loads in 14 days. No penalty applied —"
                               f" your call, Brad.")
    con.commit()


# ============================================================
# STEP 10 — CLOSE OUT + BILLING
# ============================================================

def step10_close(con):
    for mid, tid, lid, rate, did in con.execute("""
            SELECT m.id, m.truck_id, m.load_id, m.final_rate, m.dispatcher_id
            FROM matches m JOIN trucks t ON t.id=m.truck_id
            WHERE m.state='DELIVERED' AND t.status_code=5""").fetchall():
        co, = con.execute("SELECT company_id FROM trucks WHERE id=?", (tid,)).fetchone()
        o = con.execute("SELECT origin_city||', '||origin_state,"
                        " dest_city||', '||dest_state, miles FROM loads WHERE id=?",
                        (lid,)).fetchone()
        fee = round((rate or 0) * db.FEE_RATE, 2)
        con.execute("""INSERT INTO history(match_id, truck_id, company_id,
                       dispatcher_id, origin, dest, miles, final_rate, fee)
                       VALUES (?,?,?,?,?,?,?,?,?)""",
                    (mid, tid, co, did, o[0], o[1], o[2], rate, fee))
        # AUTO-GPS: the drop city IS the truck's new location. Driver never
        # types their position — Jecht already knows where the load ended.
        dc, _, ds = (o[1] or "").rpartition(", ")
        if dc and len(ds) == 2:
            con.execute("UPDATE trucks SET has_load=0, gps_city=?, gps_state=?,"
                        " gps_updated=? WHERE id=?", (dc, ds.upper(), now(), tid))
        else:
            con.execute("UPDATE trucks SET has_load=0 WHERE id=?", (tid,))
        db.set_status(con, tid, 0, "delivered + closed, ready to match")
        db.log(con, "CLOSED_OUT", truck_id=tid, match_id=mid,
               detail=f"${rate} fee ${fee}")
    con.commit()
    return mail.billing_rollup_email(con)


# ============================================================
# FLEET GRID
# ============================================================

def fleet_grid(con):
    rows = con.execute("""
        SELECT t.truck_number, c.name, t.gps_city, t.gps_state,
               t.status_code, t.status_changed, c.paid, t.active
        FROM trucks t JOIN companies c ON c.id=t.company_id
        ORDER BY c.name, t.truck_number""").fetchall()
    out = ["TRUCK  COMPANY               LOCATION             STATUS"]
    out.append("-" * 78)
    for tn, co, gc, gs, sc, ch, paid, act in rows:
        loc = f"{gc or '?'}, {gs or '?'}"
        status = db.STATUS_CODES.get(sc, "?")
        tag = "" if act else " [INACTIVE]"
        tag += "" if paid else " [UNPAID-NO DISPATCH]"
        out.append(f"{tn:<6} {co[:20]:<21} {loc[:20]:<20} {sc}: {status}{tag}")
    return "\n".join(out)


# ============================================================
# CYCLE + LOOP
# ============================================================

def cycle(con, scrape=True):
    db.log(con, "CYCLE_START")
    step1_timers(con)
    step2_inbox(con)
    trucks = step3_trucks(con)
    if scrape and trucks:
        step4_intake(con, step4_scrape(con, trucks))
    step5_stamp(con)
    step6_grade(con)
    step7_match(con, trucks)
    sent = step8_route(con)
    step9_trust(con)
    billed = step10_close(con)
    db.log(con, "CYCLE_END", detail=f"trucks={len(trucks)} sent={len(sent)}"
                                    f" billed={len(billed)}")
    return {"trucks": len(trucks), "sent": sent, "billed": billed}


HUNT_EVERY_MIN = 30          # while any truck needs a load
HEARTBEAT_HOURS = (6, 15)    # all booked -> just two check-ins a day


def _trucks_hunting(con):
    """Empty + active + paid trucks not on a day off = reasons to hunt."""
    return con.execute("""
        SELECT COUNT(*) FROM trucks t JOIN companies c ON c.id=t.company_id
        WHERE t.active=1 AND t.has_load=0 AND t.status_code=0 AND c.paid=1
          AND (t.no_load_until IS NULL OR t.no_load_until=''
               OR date('now','localtime') >= t.no_load_until)
        """).fetchone()[0]


def _next_heartbeat():
    """Next 06:00 or 15:00 local — the fully-booked idle cadence."""
    n = datetime.datetime.now()
    for h in sorted(HEARTBEAT_HOURS):
        cand = n.replace(hour=h, minute=0, second=0, microsecond=0)
        if cand > n:
            return cand
    return (n + datetime.timedelta(days=1)).replace(
        hour=min(HEARTBEAT_HOURS), minute=0, second=0, microsecond=0)


def loop(every_min=HUNT_EVERY_MIN):
    """Smart cadence. Trucks hunting -> full cycle every 30 minutes.
    Everything booked / off -> sleep until the next heartbeat (06:00, 15:00)
    so a fully-loaded fleet only runs twice a day. Day-off trucks rejoin
    automatically on their next workday — Monday after a weekend."""
    while True:
        con = db.connect()
        db.init(con)
        hunting = 0
        try:
            r = cycle(con)
            hunting = _trucks_hunting(con)
            print(f"[{now()}] cycle done: {r} | trucks hunting: {hunting}")
        except Exception as e:
            db.log(con, "CYCLE_ERROR", detail=str(e)[:300])
            print(f"[{now()}] CYCLE ERROR: {e}")
            hunting = 1                     # error -> stay on fast cadence
        finally:
            con.close()
        if hunting:
            print(f"[{now()}] {hunting} truck(s) need loads"
                  f" -> next sweep in {every_min} min")
            time.sleep(every_min * 60)
        else:
            nb = _next_heartbeat()
            secs = max(60, (nb - datetime.datetime.now()).total_seconds())
            print(f"[{now()}] fleet fully booked -> idle until"
                  f" {nb.strftime('%a %H:%M')} heartbeat")
            time.sleep(secs)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "cycle"
    con = db.connect()
    db.init(con)
    if cmd == "cycle":
        print(cycle(con))
    elif cmd == "loop":
        loop()
    elif cmd == "grid":
        print(fleet_grid(con))
    elif cmd == "approve":
        print(f"sent {mail.flush_outbox(con)} approved email(s)")
    elif cmd == "reply":
        if len(sys.argv) < 4:
            print('usage: python jecht_engine.py reply <truck#> "<text>"')
        else:
            print(apply_driver_reply(con, sys.argv[2],
                                     " ".join(sys.argv[3:])))
    elif cmd == "dayoff":
        if len(sys.argv) < 3:
            print("usage: python jecht_engine.py dayoff <truck#>")
        else:
            print(day_off(con, sys.argv[2]))
    elif cmd == "levelcap":
        if len(sys.argv) < 4:
            print("usage: python jecht_engine.py levelcap <truck#> <1-4>")
        else:
            cap = max(1, min(4, int(sys.argv[3])))
            con.execute("UPDATE trucks SET level_cap=?, "
                        "level_code=MIN(level_code, ?) "
                        "WHERE truck_number=?", (cap, cap, sys.argv[2]))
            con.commit()
            names = {1: "SAFE", 2: "MODERATE", 3: "AGGRESSIVE", 4: "OUTLAW"}
            print(f"TRUCK {sys.argv[2]}: level cap -> {names[cap]}")
    elif cmd == "fuel":
        if len(sys.argv) > 2:
            try:
                price = float(sys.argv[2].replace("$", ""))
            except ValueError:
                print("usage: jecht_engine.py fuel 4.75")
                sys.exit(1)
            con.execute("INSERT INTO meta(key,value) VALUES('fuel_price',?) "
                        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                        (str(price),))
            con.commit()
        price = fuel_price(con)
        if price > 0:
            print(f"diesel: ${price:.2f}/gal — fuel floors "
                  f"(posted rate is never negotiated down):")
            for lc, (lname, *_rest) in sorted(db.LEVEL_CODES.items()):
                print(f"  {lc} {lname:<10} >= ${fuel_floor(con, lc):.2f}/mi "
                      f"({int((FUEL_FLOOR_FACTOR - FUEL_LEVEL_CUT.get(lc, 0)) * 100)}%)")
        else:
            print("fuel price not set — fuel floor inactive "
                  "(set with: jecht_engine.py fuel 4.75)")
    elif cmd == "autosend":
        on = len(sys.argv) > 2 and sys.argv[2].lower() == "on"
        mail.set_auto_send(con, on)
        print("auto_send:", "ON — emails fire immediately" if on
              else "OFF — proposal mode, emails wait in outbox/")
    else:
        print(__doc__)
