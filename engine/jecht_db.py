"""
JECHT DATABASE — the foundation. Protocol v1.2.
Every decision is a column lookup. No AI touches this file's data.

Creates/maintains jecht.db (SQLite) next to this script.
Run directly to (re)build tables, seed codebooks, and load starter data:
    python jecht_db.py
Safe to re-run: CREATE IF NOT EXISTS + idempotent seeds. Never drops data.
"""

import os
import sqlite3
import datetime

PROTOCOL_VERSION = "1.2"
DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "jecht.db")

# ============================================================
# CODEBOOKS (Protocol section 1) — the only legal vocabulary
# ============================================================

EQUIPMENT_CODES = {
    1: ("53' Dry Van",        "Vans",     "Van"),
    2: ("53' Reefer",         "Reefers",  "Reefer"),
    3: ("Flatbed",            "Flatbeds", "Flatbed"),
    4: ("Step Deck",          "Flatbeds", "Step Deck"),
    5: ("Straight/Box Truck", "Vans",     "Box Truck"),
    6: ("Power Only",         "Power Only", "Power Only"),
    7: ("Hotshot",            "Flatbeds", "Hotshot"),
}  # code: (label, dat_term, lb123_term)

GRADE_CODES = {
    1: "AAA", 2: "AA+", 3: "AA", 4: "A+", 5: "A", 6: "B+", 7: "B",
    8: "C+", 9: "C", 10: "D+", 11: "D", 12: "E", 13: "F",
}

CONTACT_CODES = {0: "SKIP (no email, no phone)", 1: "EMAIL", 2: "CALL ONLY"}

DOC_CODES = {
    1: "MC Authority Letter",
    2: "Certificate of Insurance",
    3: "W-9",
    4: "Notice of Assignment (factoring)",
    5: "Carrier Profile Sheet",
    6: "Signed Broker-Carrier Agreement",
}

# Dispatch levels — how hard the matcher pushes for a truck.
# Feasibility = max realistic miles/day (avg 50 mph × drive hours).
# 4 OUTLAW is for exempt operations ONLY: ag/farm hauls, team drivers,
# FEMA/emergency declarations. It skips the standard-hours feasibility
# check and plans on physical drive time. Blacklisted brokers stay
# banned at every level. No exceptions.
LEVEL_CODES = {
    1: ("SAFE",       6,  1.00, 450),   # easy days, top brokers only
    2: ("MODERATE",   9,  1.00, 550),   # the default working pace
    3: ("AGGRESSIVE", 12, 0.90, 650),   # full single-driver clock, every drop
    4: ("OUTLAW",     13, 0.85, 1100),  # physical limits — exempt ops only
}
# tuple = (name, max broker grade, rate-floor multiplier, max miles/day)

DRIVER_DEST_CODES = {
    0: ("DAY OFF",  "no load today; engine resumes next workday (Mon if weekend)"),
    1: ("MONEY",    "best paying load, any direction"),
    2: ("CITY",     "driver types City, ST in this slot instead of a number"),
    3: ("HOME",     "head toward the truck's home base"),
}
# Driver text menu — slot 2 of the reply line. Numbers only (slot 2 also
# accepts a typed 'City, ST' which IS code 2). Code 0 may be sent alone.

STATUS_CODES = {
    0: "EMPTY - ready to match",
    1: "OFFER OUT - 30 min timer",
    2: "BOOKED - awaiting ratecon",
    3: "DISPATCHED - driver confirmed YES",
    4: "IN TRANSIT - GPS tracking",
    5: "DELIVERED - close out, bill, reset to 0",
}

ROLE_CODES = {1: "OWNER", 2: "DRIVER"}

OUTCOME_CODES = {  # dispatcher panel buttons (Protocol section 8)
    1: "BOOKED at posted rate",
    2: "BOOKED at typed final rate",
    3: "NO ANSWER - retry, 2 strikes dead",
    4: "LOAD GONE - re-match truck",
    5: "BAD BROKER - blacklist",
    6: "CALLED - not booked (effort logged, load stays live)",
    7: "NOT WORKED - untouched, re-offer next cycle",
    8: "EMAILED - sent via multi-email, awaiting broker reply",
    9: "NOT INTERESTED - declined, release back to pool immediately",
}

TRUST_FLAG_CODES = {  # Protocol section 10 — every flag is a counter, never an opinion
    1: ("GHOST_BOOK",  -10, "BOOKED claimed, no ratecon copy within 4 hours"),
    2: ("FAKE_GONE",   -10, "LOAD GONE claimed, load still posted next scrape"),
    3: ("DODGE",        -5, "no-answer rate 3x peer average on same brokers"),
    4: ("STOLEN_LOAD", -25, "BOOKED claimed, no delivery in history; ratecon MC mismatch"),
    5: ("RATE_LIE",    -15, "reported rate != ratecon rate (ratecon overrides ledger)"),
    6: ("SNIPE",       -10, "3+ declined loads vanished from board within 1h, same week"),
    7: ("CLEAN_BOOK",   +2, "verified booked load, ratecon matched"),
}
TRUST_START = 100
TRUST_WARN  = 70    # warning banner in dispatcher panel
TRUST_KICK  = 50    # active=0 + KILL block; also 2x STOLEN_LOAD = instant kick

# ============================================================
# SCHEMA — 11 tables
# ============================================================

SCHEMA = """
CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY, value TEXT
);

CREATE TABLE IF NOT EXISTS companies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    mc_number TEXT UNIQUE,
    dot_number TEXT,
    ein TEXT,
    owner_name TEXT,
    owner_phone TEXT,
    owner_email TEXT,
    factoring_company TEXT,
    paid INTEGER NOT NULL DEFAULT 0,            -- paywall: 0 = NO DISPATCH
    created TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS drivers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_id INTEGER NOT NULL REFERENCES companies(id),
    name TEXT NOT NULL,
    phone TEXT,
    role_code INTEGER NOT NULL DEFAULT 2,       -- 1=OWNER 2=DRIVER
    active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS trucks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_id INTEGER NOT NULL REFERENCES companies(id),
    driver_id INTEGER REFERENCES drivers(id),
    truck_number TEXT NOT NULL,
    plate TEXT,
    trailer_number TEXT,
    trailer_plate TEXT,
    equipment_code INTEGER NOT NULL,            -- EQUIPMENT_CODES
    max_weight INTEGER DEFAULT 45000,
    hazmat INTEGER NOT NULL DEFAULT 0,
    min_rpm REAL DEFAULT 2.00,                  -- negotiation floor $/mi
    gps_city TEXT, gps_state TEXT,
    gps_updated TEXT,                           -- stale > 4h = no matching
    dest_pref TEXT,                             -- MONEY | HOME | "City, ST" list
    level_code INTEGER NOT NULL DEFAULT 2,      -- LEVEL_CODES (dispatch style)
    level_cap INTEGER NOT NULL DEFAULT 2,       -- HARD CEILING: driver requests above this are clamped (dispatch only)
    status_code INTEGER NOT NULL DEFAULT 0,     -- STATUS_CODES
    status_changed TEXT DEFAULT (datetime('now')),
    has_load INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS documents (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_id INTEGER NOT NULL REFERENCES companies(id),
    doc_code INTEGER NOT NULL,                  -- DOC_CODES
    file_path TEXT,
    on_file INTEGER NOT NULL DEFAULT 0,
    expires TEXT,                               -- expired/30-day warn handled by engine
    UNIQUE(company_id, doc_code)
);

CREATE TABLE IF NOT EXISTS brokers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT,
    mc_number TEXT UNIQUE,
    phone TEXT, email TEXT,
    credit_score TEXT, days_to_pay TEXT,
    blacklist INTEGER NOT NULL DEFAULT 0        -- 1 = contact_code forced to 0
);

CREATE TABLE IF NOT EXISTS loads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source TEXT NOT NULL,                       -- DAT | 123LB
    external_ref TEXT,
    origin_city TEXT, origin_state TEXT,
    dest_city TEXT, dest_state TEXT,
    miles REAL, rate REAL, rpm REAL,
    weight INTEGER,
    equipment_code INTEGER,
    broker_id INTEGER REFERENCES brokers(id),
    broker_name TEXT, broker_phone TEXT, broker_email TEXT,
    contact_code INTEGER NOT NULL DEFAULT 0,    -- 0 skip / 1 email / 2 call
    grade_code INTEGER,                         -- GRADE_CODES
    alive INTEGER NOT NULL DEFAULT 1,
    no_answer_strikes INTEGER NOT NULL DEFAULT 0,
    scraped TEXT DEFAULT (datetime('now')),
    UNIQUE(source, external_ref)
);

CREATE TABLE IF NOT EXISTS matches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    truck_id INTEGER NOT NULL REFERENCES trucks(id),
    load_id INTEGER NOT NULL REFERENCES loads(id),
    state TEXT NOT NULL DEFAULT 'PROPOSED',     -- PROPOSED/SENT/CLAIMED/BOOKED/DEAD/DELIVERED
    final_rate REAL,
    dispatcher_id INTEGER REFERENCES dispatchers(id),
    claimed_at TEXT,                            -- claim lock: 30 min
    offer_sent_at TEXT,                         -- timeout: 30 min -> reset
    ratecon_path TEXT,
    ratecon_verified INTEGER NOT NULL DEFAULT 0,
    driver_confirmed INTEGER NOT NULL DEFAULT 0,
    reported_rate REAL,                         -- what the dispatcher typed (BOOKED prompt)
    ratecon_rate REAL,                          -- what the paper says; mismatch = RATE_LIE, paper wins
    booked_at TEXT,                             -- UNVERIFIED clock: no ratecon in 4h -> GHOST_BOOK
    declined_at TEXT,                           -- NOT INTERESTED stamp -> snipe re-check next scrape
    seconds_open INTEGER,                       -- panel time-to-decision (insta-decline tell)
    created TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS dispatchers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT, phone TEXT,
    sending_email TEXT,                         -- their own address for multi-email
    active INTEGER NOT NULL DEFAULT 1,
    handles_email INTEGER NOT NULL DEFAULT 0,   -- 1 = email loads go to their panel, not the agent
    trust INTEGER NOT NULL DEFAULT 100,         -- section 10; warn <=70, kick <=50
    stolen_flags INTEGER NOT NULL DEFAULT 0,    -- 2 = instant kick regardless of trust
    kicked INTEGER NOT NULL DEFAULT 0           -- 1 = KILL block sent, bundles stop forever
);

CREATE TABLE IF NOT EXISTS history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id INTEGER REFERENCES matches(id),
    truck_id INTEGER, company_id INTEGER, dispatcher_id INTEGER,
    origin TEXT, dest TEXT, miles REAL,
    final_rate REAL,
    fee REAL,                                   -- final_rate * 0.05
    delivered TEXT DEFAULT (datetime('now')),
    billed INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT DEFAULT (datetime('now')),
    truck_id INTEGER, load_id INTEGER, match_id INTEGER,
    rule TEXT NOT NULL,                         -- which rule fired
    detail TEXT
);
"""

FEE_RATE = 0.05  # 5%, billed to OWNER, one email per owner

# ============================================================


def connect():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    # Self-healing: if the schema isn't there yet, build it. Idempotent.
    has = con.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='companies'"
    ).fetchone()
    if not has:
        init(con)
    return con


def meta_get(con, key, default=""):
    row = con.execute("SELECT value FROM meta WHERE key=?", (key,)).fetchone()
    return row[0] if row else default


def meta_set(con, key, value):
    con.execute("INSERT INTO meta(key,value) VALUES(?,?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                (key, str(value)))
    con.commit()


def init(con=None):
    """Build tables, stamp version, seed codebook-driven defaults. Idempotent."""
    own = con is None
    con = con or connect()
    con.executescript(SCHEMA)
    # Migrations for databases created before these columns existed.
    for tbl, col, ddl in (
        ("trucks",    "level_code", "level_code INTEGER NOT NULL DEFAULT 2"),
        ("trucks",    "max_level_code",
         "max_level_code INTEGER NOT NULL DEFAULT 3"),
        ("trucks",    "no_load_until", "no_load_until TEXT NOT NULL DEFAULT ''"),
        ("trucks",    "level_cap", "level_cap INTEGER NOT NULL DEFAULT 2"),
        ("trucks",    "drive_hours_left", "drive_hours_left REAL"),
        ("companies", "home_city",  "home_city TEXT"),
        ("companies", "home_state", "home_state TEXT"),
    ):
        cols = [r[1] for r in con.execute(f"PRAGMA table_info({tbl})")]
        if col not in cols:
            con.execute(f"ALTER TABLE {tbl} ADD COLUMN {ddl}")
    con.execute(
        "INSERT INTO meta(key,value) VALUES('protocol_version',?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        (PROTOCOL_VERSION,),
    )
    con.execute(
        "INSERT INTO meta(key,value) VALUES('auto_send','0') "
        "ON CONFLICT(key) DO NOTHING"
    )
    con.execute(
        "INSERT INTO meta(key,value) VALUES('fuel_price','0') "
        "ON CONFLICT(key) DO NOTHING")
    con.execute(
        "INSERT INTO meta(key,value) VALUES('dispatch_email','') "
        "ON CONFLICT(key) DO NOTHING"   # TBD — Brad creating new address
    )
    con.commit()
    if own:
        con.close()


def version_check(con):
    """Rule 12: engine refuses to run on version mismatch."""
    row = con.execute(
        "SELECT value FROM meta WHERE key='protocol_version'"
    ).fetchone()
    if not row or row["value"] != PROTOCOL_VERSION:
        raise SystemExit(
            f"VERSION LOCK: database is protocol {row['value'] if row else 'NONE'}, "
            f"code is {PROTOCOL_VERSION}. Run: python jecht_db.py"
        )


def ensure_doc_slots(con, company_id):
    """New company -> 6 empty document slots, all flagged missing."""
    for code in DOC_CODES:
        con.execute(
            "INSERT OR IGNORE INTO documents(company_id, doc_code, on_file) "
            "VALUES(?,?,0)",
            (company_id, code),
        )


def add_company(con, **f):
    cur = con.execute(
        "INSERT INTO companies(name,mc_number,dot_number,ein,owner_name,"
        "owner_phone,owner_email,factoring_company,paid) "
        "VALUES(:name,:mc_number,:dot_number,:ein,:owner_name,"
        ":owner_phone,:owner_email,:factoring_company,:paid)",
        {**{k: None for k in ('mc_number','dot_number','ein','owner_name',
            'owner_phone','owner_email','factoring_company')}, "paid": 0, **f},
    )
    cid = cur.lastrowid
    ensure_doc_slots(con, cid)
    log(con, rule="NEW_COMPANY", detail=f.get("name"))
    return cid


def add_driver(con, company_id, name, phone=None, role_code=2):
    cur = con.execute(
        "INSERT INTO drivers(company_id,name,phone,role_code) VALUES(?,?,?,?)",
        (company_id, name, phone, role_code),
    )
    log(con, rule="NEW_DRIVER", detail=name)
    return cur.lastrowid


def add_truck(con, company_id, truck_number, equipment_code, **f):
    if equipment_code not in EQUIPMENT_CODES:
        raise ValueError(f"Illegal equipment code: {equipment_code}")
    cur = con.execute(
        "INSERT INTO trucks(company_id,truck_number,equipment_code,driver_id,"
        "plate,trailer_number,trailer_plate,max_weight,hazmat,min_rpm,dest_pref,"
        "level_code) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
        (company_id, truck_number, equipment_code,
         f.get("driver_id"), f.get("plate"), f.get("trailer_number"),
         f.get("trailer_plate"), f.get("max_weight", 45000),
         f.get("hazmat", 0), f.get("min_rpm", 2.00), f.get("dest_pref"),
         f.get("level_code", 2)),
    )
    log(con, rule="NEW_TRUCK", truck_id=cur.lastrowid, detail=truck_number)
    return cur.lastrowid


def set_status(con, truck_id, status_code, detail=""):
    if status_code not in STATUS_CODES:
        raise ValueError(f"Illegal status code: {status_code}")
    con.execute(
        "UPDATE trucks SET status_code=?, status_changed=datetime('now'), "
        "has_load=? WHERE id=?",
        (status_code, 1 if status_code in (2, 3, 4) else 0, truck_id),
    )
    log(con, rule=f"STATUS->{status_code}", truck_id=truck_id, detail=detail)


def log(con, rule, truck_id=None, load_id=None, match_id=None, detail=""):
    con.execute(
        "INSERT INTO log(truck_id,load_id,match_id,rule,detail) VALUES(?,?,?,?,?)",
        (truck_id, load_id, match_id, rule, str(detail)),
    )


def nightly_backup():
    """Rule: nightly copy of the DB file. Call from the engine loop."""
    if not os.path.exists(DB_PATH):
        return None
    stamp = datetime.date.today().isoformat()
    dst = DB_PATH.replace(".db", f".backup-{stamp}.db")
    if not os.path.exists(dst):
        import shutil
        shutil.copy2(DB_PATH, dst)
    return dst


# ============================================================
# SEED — Brad's starter rows (idempotent: keyed on MC#)
# ============================================================

def seed(con):
    row = con.execute(
        "SELECT id FROM companies WHERE mc_number='1559338'"
    ).fetchone()
    if row:
        return row["id"]
    cid = add_company(
        con,
        name="Rots Trucking LLC",
        mc_number="1559338",
        dot_number="4092433",
        owner_name="Brad Haga",
        owner_phone="+12175046883",
        owner_email="rotstrucking1@gmail.com",
        paid=1,
    )
    did = add_driver(con, cid, "Brad Haga", "+12175046883", role_code=1)
    add_truck(
        con, cid, "101", 1,  # 1 = 53' Dry Van
        driver_id=did, trailer_number="431006",
    )
    con.execute(
        "INSERT OR IGNORE INTO dispatchers(name,email,phone) VALUES(?,?,?)",
        ("Brad Haga", "rotstrucking1@gmail.com", "+12175046883"),
    )
    con.commit()
    return cid


if __name__ == "__main__":
    con = connect()
    init(con)
    seed(con)
    con.commit()
    n = {t: con.execute(f"SELECT COUNT(*) c FROM {t}").fetchone()["c"]
         for t in ("companies", "drivers", "trucks", "documents",
                   "brokers", "loads", "matches", "dispatchers",
                   "history", "log")}
    print(f"jecht.db ready — protocol v{PROTOCOL_VERSION}")
    for t, c in n.items():
        print(f"  {t:12} {c} rows")
    con.close()
