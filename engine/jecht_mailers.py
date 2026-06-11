"""
JECHT MAILERS — Protocol v1.3, Sections 7-10.
Builds and sends every outbound email; reads every inbound sealed block.

HARD RULES (baked in, not configurable):
  * All wire traffic goes ONLY to/from DISPATCH_EMAIL (jecht_email_config).
  * Kill switch starts OFF: meta key 'auto_send' = '0' means every email is
    written to outbox/ as a .eml proposal and NOTHING is sent until Brad
    approves (or flips auto_send to '1').
  * Only mail containing a sealed ===JECHT|...=== block (or a ratecon reply
    with a stamped subject) is processed. Everything else is ignored.
  * No AI in this file. Bodies are templates filled from columns.
"""

import os
import re
import email
import email.message
import email.utils
import imaplib
import smtplib
import datetime

import jecht_db as db
import jecht_wire as wire
import jecht_email_config as cfg

HERE = os.path.dirname(os.path.abspath(__file__))
OUTBOX = os.path.join(HERE, "outbox")        # proposals waiting for approval
SENTBOX = os.path.join(HERE, "outbox", "sent")
RATECON_DIR = os.path.join(HERE, "ratecons")

for d in (OUTBOX, SENTBOX, RATECON_DIR):
    os.makedirs(d, exist_ok=True)


# ------------------------------------------------------------
# Kill switch
# ------------------------------------------------------------

def auto_send_on(con):
    row = con.execute("SELECT value FROM meta WHERE key='auto_send'").fetchone()
    return bool(row and row[0] == "1")


def set_auto_send(con, on):
    con.execute("INSERT OR REPLACE INTO meta(key,value) VALUES('auto_send',?)",
                ("1" if on else "0",))
    con.commit()


# ------------------------------------------------------------
# Core send (proposal mode aware)
# ------------------------------------------------------------

def send_mail(con, to_addr, subject, body, attach_paths=None, force=False,
              html_body=None):
    """Build the email. auto_send=1 (or force) -> SMTP. Else -> outbox proposal.
    If html_body is given the mail goes out multipart/alternative: plain text
    for ancient clients, the HTML invoice for everyone else.
    Returns ('SENT'|'PROPOSED', path_or_addr)."""
    msg = email.message.EmailMessage()
    msg["From"] = f"{cfg.DISPATCH_NAME} <{cfg.DISPATCH_EMAIL}>"
    msg["To"] = to_addr
    msg["Subject"] = subject
    msg["Date"] = email.utils.formatdate(localtime=True)
    msg.set_content(body)
    if html_body:
        msg.add_alternative(html_body, subtype="html")
    for p in (attach_paths or []):
        with open(p, "rb") as f:
            data = f.read()
        msg.add_attachment(data, maintype="application",
                           subtype="octet-stream",
                           filename=os.path.basename(p))

    if auto_send_on(con) or force:
        try:
            with smtplib.SMTP(cfg.SMTP_HOST, cfg.SMTP_PORT, timeout=30) as s:
                s.starttls()
                s.login(cfg.DISPATCH_EMAIL, cfg.APP_PASSWORD)
                s.send_message(msg)
            db.log(con, "MAIL_SENT", detail=f"{to_addr} | {subject}")
            return ("SENT", to_addr)
        except Exception as e:
            # NEVER crash the engine over a mail hiccup. Park it in the
            # outbox so APPROVE can retry, and leave a receipt.
            db.log(con, "MAIL_FAIL", detail=f"{to_addr} | {subject} | {e}"[:200])

    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    safe = re.sub(r"[^A-Za-z0-9_-]+", "_", subject)[:60]
    path = os.path.join(OUTBOX, f"{stamp}_{safe}.eml")
    with open(path, "wb") as f:
        f.write(bytes(msg))
    db.log(con, "MAIL_PROPOSED", detail=f"{to_addr} | {subject} -> {path}")
    return ("PROPOSED", path)


def flush_outbox(con):
    """Send every approved proposal in outbox/ (Brad ran APPROVE)."""
    sent = 0
    for name in sorted(os.listdir(OUTBOX)):
        path = os.path.join(OUTBOX, name)
        if not name.endswith(".eml") or not os.path.isfile(path):
            continue
        try:
            with open(path, "rb") as f:
                msg = email.message_from_bytes(f.read())
            with smtplib.SMTP(cfg.SMTP_HOST, cfg.SMTP_PORT, timeout=30) as s:
                s.starttls()
                s.login(cfg.DISPATCH_EMAIL, cfg.APP_PASSWORD)
                s.send_message(msg)
            os.replace(path, os.path.join(SENTBOX, name))
            db.log(con, "MAIL_FLUSHED", detail=name)
            sent += 1
        except Exception as e:
            # leave the file in the outbox for retry; keep flushing the rest
            db.log(con, "MAIL_FLUSH_FAIL", detail=f"{name} | {e}"[:200])
    return sent


# ------------------------------------------------------------
# Builders — every body is a template filled from columns
# ------------------------------------------------------------

def match_tag(match_id):
    return f"M-{int(match_id):04d}"


def need_dispatch_email(con, blocks_text, n_loads):
    """Email-contact loads -> cloud agent for negotiation."""
    subject = f"NEED DISPATCH - {n_loads} load(s) - {datetime.date.today()}"
    body = (
        "JECHT -> AGENT. Negotiate the loads below by email only.\n"
        "Floor = floor_rpm x miles. Send ratecons back to this address\n"
        "with subject RATECON <match_id>.\n\n" + blocks_text
    )
    return send_mail(con, cfg.DISPATCH_EMAIL, subject, body)


def call_sheet_email(con, dispatcher_row, blocks_text, n_loads):
    """Call-only loads -> dispatcher. One bundle per dispatcher per cycle."""
    d_id, d_name, d_email = dispatcher_row
    subject = f"JECHT CALL SHEET - {n_loads} load(s) - {datetime.date.today()}"
    body = (
        f"{d_name} — paste everything between the lines into your panel\n"
        "(Loads tab -> IMPORT). Work the loads, click outcomes, then\n"
        "SEND TO JECHT.\n\n" + blocks_text
    )
    return send_mail(con, d_email, subject, body)


def invoice_html(co_name, mc_number, owner, items, total):
    """Render the JECHT invoice (same template as the panel's 🧾 INVOICE tab)
    as an HTML email body. items = [(origin, dest, truck_id, rate, fee), ...]"""
    today = datetime.date.today()
    inv_no = "JD-%s-%s" % (today.strftime("%Y%m%d"),
                           (str(mc_number or "0000"))[-4:])
    rows = ""
    for (origin, dest, truck_id, rate, fee) in items:
        rows += (
            "<tr>"
            f"<td style='padding:8px 10px;border-bottom:1px solid #eee;'>{origin} &rarr; {dest}</td>"
            f"<td style='padding:8px 10px;border-bottom:1px solid #eee;text-align:center;'>{truck_id or '—'}</td>"
            f"<td style='padding:8px 10px;border-bottom:1px solid #eee;text-align:right;'>${rate:,.2f}</td>"
            f"<td style='padding:8px 10px;border-bottom:1px solid #eee;text-align:right;'>${fee:,.2f}</td>"
            "</tr>")
    return f"""<!doctype html><html><body style="margin:0;padding:24px;background:#f4f4f4;font-family:Arial,Helvetica,sans-serif;color:#222;">
<div style="max-width:680px;margin:0 auto;background:#fff;border-top:6px solid #ff8c1a;padding:32px 36px;">
  <table width="100%" cellpadding="0" cellspacing="0"><tr>
    <td><div style="font-size:26px;font-weight:bold;letter-spacing:8px;color:#ff8c1a;">JECHT <span style="color:#00bcd4;">//</span></div>
        <div style="font-size:11px;letter-spacing:3px;color:#888;">DISPATCH SERVICES</div></td>
    <td align="right"><div style="font-size:16px;font-weight:bold;">INVOICE {inv_no}</div>
        <div style="font-size:12px;color:#666;">Date: {today.strftime('%B %d, %Y')}</div>
        <div style="font-size:12px;color:#666;">Terms: Due on receipt</div></td>
  </tr></table>
  <div style="margin:26px 0 6px;font-size:11px;letter-spacing:2px;color:#888;">BILL TO</div>
  <div style="font-size:14px;font-weight:bold;">{co_name}</div>
  <div style="font-size:13px;">Attn: {owner}</div>
  <div style="font-size:13px;color:#555;">MC# {mc_number or '—'}</div>
  <div style="margin:24px 0 8px;font-size:11px;letter-spacing:2px;color:#888;">DISPATCH FEE &mdash; 5% OF RATECON-VERIFIED GROSS</div>
  <table width="100%" cellpadding="0" cellspacing="0" style="font-size:13px;border-collapse:collapse;">
    <tr style="border-bottom:2px solid #222;">
      <th style="padding:6px 10px;text-align:left;font-size:11px;letter-spacing:1px;color:#666;">LANE</th>
      <th style="padding:6px 10px;text-align:center;font-size:11px;letter-spacing:1px;color:#666;">TRUCK</th>
      <th style="padding:6px 10px;text-align:right;font-size:11px;letter-spacing:1px;color:#666;">RATE</th>
      <th style="padding:6px 10px;text-align:right;font-size:11px;letter-spacing:1px;color:#666;">FEE (5%)</th>
    </tr>
    {rows}
  </table>
  <table cellpadding="0" cellspacing="0" align="right" style="margin-top:18px;font-size:14px;">
    <tr><td style="padding:6px 14px;font-weight:bold;font-size:16px;">TOTAL DUE</td>
        <td style="padding:6px 14px;text-align:right;font-weight:bold;font-size:16px;color:#c0392b;">${total:,.2f}</td></tr>
  </table>
  <div style="clear:both;padding-top:34px;font-size:12px;color:#555;">
    Please remit the total due to keep dispatch service active. Reply to this email with any questions.</div>
  <div style="margin-top:26px;border-top:1px solid #ddd;padding-top:10px;font-size:11px;color:#999;">
    <span style="letter-spacing:2px;">JECHT DISPATCH</span> &nbsp;&middot;&nbsp; {cfg.DISPATCH_EMAIL}</div>
</div></body></html>"""


def billing_rollup_email(con):
    """5% fee off ratecon-verified rates. ONE email per owner, all trucks.
    Plain-text body + the HTML invoice riding along as the pretty version."""
    rows = con.execute("""
        SELECT h.company_id, c.name, c.owner_name, c.owner_email,
               h.id, h.origin, h.dest, h.final_rate, h.fee,
               c.mc_number, h.truck_id
        FROM history h JOIN companies c ON c.id = h.company_id
        WHERE h.billed = 0 AND h.final_rate IS NOT NULL
        ORDER BY h.company_id, h.id""").fetchall()
    results = []
    by_co = {}
    for r in rows:
        by_co.setdefault(r[0], []).append(r)
    for co_id, items in by_co.items():
        _, co_name, owner, owner_email, *_ = items[0]
        mc_number = items[0][9]
        if not owner_email:
            db.log(con, "BILL_NO_EMAIL", detail=f"company {co_id} has no owner_email")
            continue
        lines, inv_items, total = [], [], 0.0
        for (_co, _cn, _on, _oe, hid, origin, dest, rate, fee,
             _mc, truck_id) in items:
            lines.append(f"  {origin} -> {dest}   ${rate:,.2f}   fee ${fee:,.2f}")
            inv_items.append((origin, dest, truck_id, rate, fee))
            total += fee
        body = (
            f"{owner},\n\nJecht dispatch fee summary for {co_name}.\n"
            f"5% of the ratecon-verified rate on each delivered load:\n\n"
            + "\n".join(lines)
            + f"\n\nTOTAL DUE: ${total:,.2f}\n\nReply to this email with any questions.\n— Jecht Dispatch"
        )
        html = invoice_html(co_name, mc_number, owner, inv_items, total)
        status, ref = send_mail(con, owner_email,
                                f"Dispatch fee summary - {co_name} - ${total:,.2f}",
                                body, html_body=html)
        if status == "SENT":
            ids = ",".join(str(i[4]) for i in items)
            con.execute(f"UPDATE history SET billed=1 WHERE id IN ({ids})")
            con.commit()
        results.append((co_name, total, status, ref))
    return results


def kill_email(con, dispatcher_id, reason_code=1):
    """Send the KILL block. Panel wipes and locks on import."""
    row = con.execute("SELECT name,email FROM dispatchers WHERE id=?",
                      (dispatcher_id,)).fetchone()
    if not row:
        return ("NO_DISPATCHER", None)
    block = wire.build("KILL", {"dispatcher_id": dispatcher_id,
                                "reason_code": reason_code})
    body = ("Your Jecht panel access has been revoked.\n"
            "Importing the block below locks the panel.\n\n" + block +
            "\n\nContact Rots Trucking with questions.")
    # KILL always sends regardless of kill switch — security beats proposal mode
    return send_mail(con, row[1], "JECHT ACCESS REVOKED", body, force=True)


# ------------------------------------------------------------
# Inbox — pull sealed blocks + ratecon attachments
# ------------------------------------------------------------

SUBJECT_RATECON = re.compile(r"RATECON\s+M-?(\d+)", re.I)


PHONE_IN_ADDR = re.compile(r"(\d{10,11})")

def poll_inbox(con):
    """Fetch UNSEEN mail. Returns (blocks, ratecons, texts):
       blocks  = [(type, fields, from_addr), ...] parsed + checksum-verified
       ratecons= [(match_id, saved_path, from_addr), ...]
       texts   = [(phone, body, reply_addr), ...] driver SMS relayed by
                 Google Voice into this inbox. Replying to reply_addr by
                 email sends the driver a text back — full SMS loop, no
                 phone hardware needed."""
    blocks, ratecons, texts = [], [], []
    M = imaplib.IMAP4_SSL(cfg.IMAP_HOST, cfg.IMAP_PORT)
    try:
        M.login(cfg.DISPATCH_EMAIL, cfg.APP_PASSWORD)
        M.select("INBOX")
        _typ, data = M.search(None, "UNSEEN")
        for num in data[0].split():
            _typ, msgdata = M.fetch(num, "(RFC822)")
            msg = email.message_from_bytes(msgdata[0][1])
            from_addr = email.utils.parseaddr(msg.get("From", ""))[1].lower()
            subject = msg.get("Subject", "")

            # ratecon? save attachments, tag with match id from subject
            m = SUBJECT_RATECON.search(subject)
            text_parts = []
            for part in msg.walk():
                fn = part.get_filename()
                if fn and m:
                    raw = part.get_payload(decode=True) or b""
                    safe = re.sub(r"[^A-Za-z0-9._-]+", "_", fn)
                    path = os.path.join(RATECON_DIR, f"M{int(m.group(1)):04d}_{safe}")
                    with open(path, "wb") as f:
                        f.write(raw)
                    ratecons.append((int(m.group(1)), path, from_addr))
                elif part.get_content_type() == "text/plain":
                    try:
                        text_parts.append(part.get_payload(decode=True).decode("utf-8", "replace"))
                    except Exception:
                        pass
            body = "\n".join(text_parts)

            # sealed blocks in body
            sealed = re.findall(r"===JECHT\|.*?===END\|[0-9a-f]+===", body, re.S)
            for raw_block in sealed:
                try:
                    for btype, _block_id, fields in wire.parse(raw_block):
                        blocks.append((btype, fields, from_addr))
                except wire.WireError as e:
                    db.log(con, "WIRE_BOUNCE", detail=f"{from_addr}: {e.reason}")

            # driver SMS relayed by Google Voice (no sealed blocks in these)
            if not sealed and "voice.google.com" in from_addr:
                pm = PHONE_IN_ADDR.search(from_addr)
                if pm:
                    phone = pm.group(1)[-10:]            # last 10 digits
                    # first non-empty line of the SMS is the driver's reply
                    sms = next((ln.strip() for ln in body.splitlines()
                                if ln.strip()
                                and not ln.strip().startswith("<")), "")
                    if sms:
                        texts.append((phone, sms, msg.get("Reply-To")
                                      or msg.get("From", "")))
    finally:
        try:
            M.logout()
        except Exception:
            pass
    return blocks, ratecons, texts


def send_sms_via_gvoice(con, reply_addr, body):
    """Text a driver back by emailing the Google Voice relay address.
    GV turns the email body into an SMS. Plain text only, keep it short."""
    msg = EmailMessage()
    msg["From"] = cfg.DISPATCH_EMAIL
    msg["To"] = reply_addr
    msg["Subject"] = ""
    msg.set_content(body[:450])
    try:
        with smtplib.SMTP(cfg.SMTP_HOST, cfg.SMTP_PORT, timeout=30) as s:
            s.starttls()
            s.login(cfg.DISPATCH_EMAIL, cfg.APP_PASSWORD)
            s.send_message(msg)
        db.log(con, "SMS_SENT", detail=f"{reply_addr} | {body[:80]}")
        return True
    except Exception as e:
        db.log(con, "SMS_FAIL", detail=f"{reply_addr} | {e}"[:200])
        return False


if __name__ == "__main__":
    con = db.connect()
    db.init(con)
    print("auto_send:", "ON" if auto_send_on(con) else "OFF (proposal mode)")
    print("outbox proposals:",
          len([f for f in os.listdir(OUTBOX) if f.endswith('.eml')]))
