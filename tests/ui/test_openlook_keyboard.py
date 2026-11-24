#!/usr/bin/env python3
"""Drive OPEN LOOK toolbars and Find/Replace with real XTest events.

Run in the Open Look SDK under Xvfb and olwm. Captures go to build/.
"""
import ctypes as c
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
xlib = c.CDLL('libX11.so.6')
xtest = c.CDLL('libXtst.so.6')
xlib.XOpenDisplay.restype = c.c_void_p
xlib.XOpenDisplay.argtypes = [c.c_char_p]
display = xlib.XOpenDisplay(None)
assert display
xlib.XDefaultRootWindow.argtypes = [c.c_void_p]
xlib.XDefaultRootWindow.restype = c.c_ulong
xlib.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
xlib.XFetchName.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_char_p)]
xlib.XFree.argtypes = [c.c_void_p]
xlib.XStringToKeysym.argtypes = [c.c_char_p]
xlib.XStringToKeysym.restype = c.c_ulong
xlib.XKeysymToKeycode.argtypes = [c.c_void_p, c.c_ulong]
xlib.XKeysymToKeycode.restype = c.c_uint
xlib.XSetInputFocus.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_ulong]
xlib.XFlush.argtypes = [c.c_void_p]
xtest.XTestFakeKeyEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
xlib.XTranslateCoordinates.argtypes = [c.c_void_p, c.c_ulong, c.c_ulong, c.c_int, c.c_int, c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_ulong)]
xtest.XTestFakeMotionEvent.argtypes = [c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_ulong]
xtest.XTestFakeButtonEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]

def click(window, px, py, button=1):
    """Click a client-relative point using physical pointer events."""
    dx = c.c_int()
    dy = c.c_int()
    child = c.c_ulong()
    xlib.XTranslateCoordinates(display, window, xlib.XDefaultRootWindow(display), int(px), int(py), c.byref(dx), c.byref(dy), c.byref(child))
    xtest.XTestFakeMotionEvent(display, -1, dx.value, dy.value, 0)
    xtest.XTestFakeButtonEvent(display, button, 1, 0)
    xtest.XTestFakeButtonEvent(display, button, 0, 0)
    xlib.XFlush(display)
    time.sleep(0.3)

def windows(w=None):
    """Walk X windows and their titles, releasing Xlib allocations."""
    if w is None:
        w = xlib.XDefaultRootWindow(display)
    name = c.c_char_p()
    xlib.XFetchName(display, w, c.byref(name))
    title = name.value.decode(errors='replace') if name.value else ''
    if name:
        xlib.XFree(name)
    yield (w, title)
    root = c.c_ulong()
    parent = c.c_ulong()
    children = c.POINTER(c.c_ulong)()
    count = c.c_uint()
    if xlib.XQueryTree(display, w, c.byref(root), c.byref(parent), c.byref(children), c.byref(count)):
        ids = [children[i] for i in range(count.value)]
        if children:
            xlib.XFree(children)
        for child in ids:
            yield from windows(child)

def wait_title(title):
    """Wait for the application to map a named window."""
    for _ in range(100):
        for w, n in windows():
            if title in n:
                return w
        time.sleep(0.05)
    raise RuntimeError('No window ' + title)

def key(name, down):
    """Send one physical key transition."""
    xtest.XTestFakeKeyEvent(display, xlib.XKeysymToKeycode(display, xlib.XStringToKeysym(name.encode())), down, 0)
    xlib.XFlush(display)

def wait_closed(title):
    """Allow deferred native frame destruction to finish."""
    for _ in range(100):
        if not any(title in n for _, n in windows()):
            return
        time.sleep(0.05)
    raise AssertionError('Window did not close: ' + title)

def press(name, mod=None):
    """Press and release a key with an optional modifier."""
    if mod:
        key(mod, 1)
    key(name, 1)
    key(name, 0)
    if mod:
        key(mod, 0)
    time.sleep(0.15)

def focus(w):
    """Allow mapping to finish before assigning X keyboard focus."""
    time.sleep(0.5)
    xlib.XSetInputFocus(display, w, 1, 0)
    xlib.XFlush(display)
    time.sleep(0.1)

root = Path(__file__).resolve().parents[2]
captures = root / 'build/openlook-screenshots'
captures.mkdir(parents=True, exist_ok=True)

def capture(window, name):
    subprocess.run(['import', '-window', str(window),
                    str(captures / (name + '.png'))], check=True)

def type_text(text):
    for ch in text:
        press(ch)

with tempfile.TemporaryDirectory(prefix='fyodor-openlook-') as config:
    document = Path(config) / 'keyboard.abw'
    document.write_text('<?xml version="1.0"?><abiword><section>'
                        '<p>alpha beta alpha</p></section></abiword>')
    env = dict(os.environ, XDG_CONFIG_HOME=config)
    with (captures / 'application.log').open('w') as log:
        app = subprocess.Popen([str(Path(sys.argv[1]).resolve()), str(document)],
                               env=env, stdout=log, stderr=log)
        try:
            main = wait_title('keyboard.abw')
            focus(main)
            capture(main, 'toolbar')
            click(main, 10, 66)  # Mouse use must not give toolbar choices a focus arrow.
            press('Escape')
            capture(main, 'toolbar-after-click')
            click(main, 300, 230)
            press('f', 'Control_L')
            finder = wait_title('Find and Replace')
            time.sleep(0.5)
            type_text('alpha')
            press('Return')  # Default Find; focus must stay in this field.
            press('Tab')
            type_text('omega')
            press('Tab')
            press('space')
            capture(finder, 'keyboard-state')
            prefs = Path(config) / 'fyodor/preferences.conf'
            assert 'match_case=1' in prefs.read_text(), 'Tab reaches Match case'
            press('Tab')
            press('space')
            assert 'whole_word=1' in prefs.read_text(), 'Tab reaches Whole word'
            capture(finder, 'find')
            capture(xlib.XDefaultRootWindow(display), 'centered-dialog')
            press('Tab')  # Find
            press('Tab')  # Replace
            press('Return')
            press('Tab')  # Replace All
            press('Return')
            press('Tab')  # Close
            press('Return')
            wait_closed('Find and Replace')
            click(main, 300, 230)
            capture(main, 'before-save')
            press('s', 'Control_L')
            saved = document.read_text()
            assert 'omega beta omega' in saved, saved
            press('f', 'Control_L')
            finder = wait_title('Find and Replace')
            time.sleep(0.5)
            press('Tab', 'Shift_L')  # Wrap from search to Close.
            press('Return')
            wait_closed('Find and Replace')
            capture(main, 'after-replace')
            click(main, 310, 12, button=3)
            capture(xlib.XDefaultRootWindow(display), 'format-menu')
            press('Escape')
            click(main, 300, 230)
            press('q', 'Control_L')
            assert app.wait(timeout=5) == 0, 'Application exits cleanly'
            print('OPEN LOOK keyboard: initial typing without a click, default Find, Tab/Shift+Tab, checkboxes, Replace, Replace All and Close passed')
        finally:
            if app.poll() is None:
                app.terminate()
                try:
                    app.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    app.kill()
                    app.wait()
