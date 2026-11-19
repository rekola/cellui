"""Runs a terminal program under a pty and feeds its output through `pyte`, so a
script can read the screen as a person would see it, without a real terminal.
Needs `pyte` (pip install pyte).

A program on cellui's terminal backend asks the terminal what it can do when it
starts (cursor position, pixel geometry, kitty keyboard and graphics support,
colours) and waits up to a second for replies that a plain pty never sends.
`Screen` answers each query with a canned reply as soon as it sees it, as a real
terminal would, so the program starts at once. It claims no kitty keyboard
protocol and no pictures, so the program runs as on a plain xterm.

    pid, fd = ptyscreen.spawn(["./app", "--ui", "terminal"])
    scr = ptyscreen.Screen(fd)
    scr.wait(1.0)
    print(scr.dump())

Wait with `scr.wait()` or `scr.pump()`, never `time.sleep()` once the program is
running: an unread pty fills up and blocks the program's UI thread.
"""
import fcntl
import os
import pty
import re
import select
import struct
import termios
import time

import pyte

ROWS, COLS = 40, 120
XPIX, YPIX = 960, 1200


def set_winsize(fd, rows, cols, xpix, ypix):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, xpix, ypix))


def spawn(argv, env=None, term="xterm-256color", rows=ROWS, cols=COLS, xpix=XPIX, ypix=YPIX):
    """Starts `argv` in a pty of the given size; `env` is added to the child's
    environment. Returns (pid, master_fd), the fd non-blocking."""
    pid, master_fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = term
        os.environ.update(env or {})
        os.execvp(argv[0], argv)
        os._exit(1)
    set_winsize(master_fd, rows, cols, xpix, ypix)
    os.set_blocking(master_fd, False)
    return pid, master_fd


class Screen:
    def __init__(self, fd, rows=ROWS, cols=COLS, xpix=XPIX, ypix=YPIX):
        self.fd = fd
        self.rows, self.cols, self.xpix, self.ypix = rows, cols, xpix, ypix
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.Stream(self.screen)
        self._respond_buf = b""

    def _respond(self, data):
        buf = self._respond_buf + data
        r, c = self.rows, self.cols
        # The frame fences (CSI ? 6 n): one reply each, as a terminal gives.
        fences = buf.count(b"\x1b[?6n")
        if fences:
            os.write(self.fd, f"\x1b[?{r // 2};{c // 2}R".encode() * fences)
            buf = buf.replace(b"\x1b[?6n", b"")
        if re.search(rb"\x1b\[6n", buf):
            os.write(self.fd, f"\x1b[{r // 2};{c // 2}R".encode())
            buf = re.sub(rb"\x1b\[6n", b"", buf)
        if re.search(rb"\x1b\[14t", buf):
            os.write(self.fd, f"\x1b[4;{self.ypix};{self.xpix}t".encode())
            buf = re.sub(rb"\x1b\[14t", b"", buf)
        if re.search(rb"\x1b\[18t", buf):
            os.write(self.fd, f"\x1b[8;{r};{c}t".encode())
            buf = re.sub(rb"\x1b\[18t", b"", buf)
        if re.search(rb"\x1b\[>c", buf):
            os.write(self.fd, b"\x1b[>1;10;0c")
            buf = re.sub(rb"\x1b\[>c", b"", buf)
        if re.search(rb"\x1b\[c", buf):
            os.write(self.fd, b"\x1b[?62;22c")
            buf = re.sub(rb"\x1b\[c", b"", buf)
        if re.search(rb"\x1b\[\?u", buf):
            os.write(self.fd, b"\x1b[?0u")
            buf = re.sub(rb"\x1b\[\?u", b"", buf)
        for mode in (b"2026", b"1016"):
            pat = rb"\x1b\[\?" + mode + rb"\$p"
            if re.search(pat, buf):
                os.write(self.fd, b"\x1b[?" + mode + b";0$y")
                buf = re.sub(pat, b"", buf)
        if re.search(rb"\x1bP\+q[0-9a-f;]*\x1b\\", buf):
            os.write(self.fd, b"\x1bP0+r\x1b\\")
            buf = re.sub(rb"\x1bP\+q[0-9a-f;]*\x1b\\", b"", buf)
        buf = re.sub(rb"\x1b\]4;\d+;\?\x1b\\", b"", buf)
        buf = re.sub(rb"\x1b\]1[01];\?\x1b\\", b"", buf)
        buf = re.sub(rb"\x1b_Gi=1,a=q;\x1b\\", b"", buf)
        self._respond_buf = buf

    def pump(self, timeout=0.3, settle=0.08):
        """Reads the pty for up to `timeout` seconds, returning early once it
        has been quiet for `settle`. The cap is on wall-clock time, since a
        program redrawing steadily would otherwise keep extending the wait."""
        hard_deadline = time.time() + timeout
        end = hard_deadline
        while time.time() < end and time.time() < hard_deadline:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if self.fd in r:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    break
                if not data:
                    break
                self._respond(data)
                # pyte has no private form of the status report and fails on it.
                self.stream.feed(data.replace(b"\x1b[?6n", b"").decode("utf-8", errors="ignore"))
                end = min(time.time() + settle, hard_deadline)

    def wait_for_exit(self, proc, timeout):
        """Reads the pty until `proc` (anything with poll(), such as a helper
        process) has finished, or `timeout` seconds."""
        deadline = time.time() + timeout
        while time.time() < deadline and proc.poll() is None:
            self.pump(0.2)

    def wait_for_log(self, path, text, timeout):
        """Reads the pty until the file at `path` contains `text`, or `timeout`
        seconds; returns whether it does."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                with open(path) as f:
                    if text in f.read():
                        return True
            except OSError:
                pass
            self.pump(0.2)
        return False

    def wait(self, seconds):
        """Waits a fixed time while still reading the pty."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            self.pump(min(0.5, max(0.0, deadline - time.time())))

    def dump(self):
        return "\n".join(self.screen.display)

    def send(self, b):
        os.write(self.fd, b)
