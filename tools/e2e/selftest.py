#!/usr/bin/env python3
"""Checks ptyscreen.Screen against a child that asks the start-up queries and
prints what it is answered. Exits 77 (ctest: skipped) where pyte is missing."""
import os
import sys

try:
    import pyte  # noqa: F401
except ImportError:
    print("pyte is not installed; skipping")
    sys.exit(77)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ptyscreen  # noqa: E402

CHILD = r'''
import os, sys, termios, tty, select
tty.setraw(0)
def ask(q, end):
    os.write(1, q)
    got = b""
    while not got.endswith(end):
        if not select.select([0], [], [], 3)[0]:
            return b"TIMEOUT"
        got += os.read(0, 64)
    return got
for q, end in ((b"\x1b[6n", b"R"), (b"\x1b[?6n", b"R"), (b"\x1b[14t", b"t"), (b"\x1b[18t", b"t"), (b"\x1b[?u", b"u")):
    os.write(1, ("\r\n" + repr(ask(q, end))).encode())
os.write(1, b"\r\nDONE")
'''

pid, fd = ptyscreen.spawn([sys.executable, "-I", "-c", CHILD])
scr = ptyscreen.Screen(fd)
for _ in range(40):
    scr.pump(0.25)
    if "DONE" in scr.dump():
        break
text = scr.dump()
failed = False
for want in ("b'\\x1b[20;60R'", "b'\\x1b[?20;60R'", "b'\\x1b[4;1200;960t'", "b'\\x1b[8;40;120t'", "b'\\x1b[?0u'"):
    ok = want in text
    failed |= not ok
    print("[%s] %s" % ("PASS" if ok else "FAIL", want))
if failed:
    print(text)
sys.exit(1 if failed else 0)
