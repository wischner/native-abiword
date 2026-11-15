#!/usr/bin/env python3
"""Drive the SDL desktop with XTest; run under xvfb-run in the existing SDK.

These are real window-system events, including file-picker modal loops.
Each run owns a temporary config/document directory and its application process.
Captures are build artifacts for visual review, not golden-image assertions.
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

def click(window, px, py):
    """Click a client-relative point using physical pointer events."""
    dx = c.c_int()
    dy = c.c_int()
    child = c.c_ulong()
    xlib.XTranslateCoordinates(display, window, xlib.XDefaultRootWindow(display), int(px), int(py), c.byref(dx), c.byref(dy), c.byref(child))
    xtest.XTestFakeMotionEvent(display, -1, dx.value, dy.value, 0)
    xtest.XTestFakeButtonEvent(display, 1, 1, 0)
    xtest.XTestFakeButtonEvent(display, 1, 0, 0)
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
captures = root / 'build/dialog-screenshots'
captures.mkdir(parents=True, exist_ok=True)
scale = os.environ.get('FYODOR_TEST_DPI', '96')
subprocess.run(['xrdb', '-merge'], input=f'Xft.dpi: {scale}\n', text=True, check=True)
with tempfile.TemporaryDirectory(prefix='fyodor-inspect-') as config:
    env = dict(os.environ, XDG_CONFIG_HOME=config)
    app = subprocess.Popen([str(Path(sys.argv[1]).resolve())], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        main = wait_title('Untitled')
        focus(main)
        press('f', 'Control_L')
        finder = wait_title('Find and Replace')
        focus(finder)
        time.sleep(0.4)
        subprocess.run(['import', '-window', str(finder), str(captures / f'find-{scale}.png')], check=True)
        press('Tab')
        press('Tab')
        press('space')
        prefs = Path(config) / 'fyodor/preferences.conf'
        assert 'match_case=1' in prefs.read_text()
        press('Tab', 'Shift_L')
        press('Tab', 'Shift_L')
        for ch in 'alpha':
            press(ch)
        subprocess.run(['import', '-window', str(finder), str(captures / f'find-focus-{scale}.png')], check=True)
        press('Tab', 'Shift_L')
        press('space')
        time.sleep(0.2)
        assert not any(('Find and Replace' in n for _, n in windows()))
        # Entries -> Name -> Open -> Cancel, followed by Space.
        focus(main)
        press('o', 'Control_L')
        chooser = wait_title('Open')
        focus(chooser)
        time.sleep(0.3)
        subprocess.run(['import', '-window', str(chooser), str(captures / f'open-{scale}.png')], check=True)
        press('Tab')
        press('Tab')
        press('Tab')
        press('space')
        time.sleep(0.3)
        assert not any((n == 'Open' for _, n in windows()))
        focus(main)
        # Preserve document Tab, then cancel the unsaved-changes prompt.
        press('a')
        press('Tab')
        press('b')
        press('n', 'Control_L')
        wait_title('Fyodor')
        time.sleep(0.4)
        prompt = next((w for w, n in windows() if n == 'Fyodor'))
        focus(prompt)
        subprocess.run(['import', '-window', str(prompt), str(captures / f'prompt-{scale}.png')], check=True)
        press('Tab')
        press('Tab')
        press('Return')
        time.sleep(0.2)
        assert not any((n == 'Fyodor' for _, n in windows()))
        subprocess.run(['import', '-window', str(main), str(captures / f'main-{scale}.png')], check=True)
        focus(main)
        # Menu coordinates are logical pixels at the stock SDL control font.
        click(main, 264 * int(scale) / 96, 12 * int(scale) / 96)
        click(main, 290 * int(scale) / 96, 34 * int(scale) / 96)
        about = wait_title('About Fyodor')
        focus(about)
        subprocess.run(['import', '-window', str(about), str(captures / f'about-{scale}.png')], check=True)
        press('Return')
        focus(main)
        key('Control_L', 1)
        key('Shift_L', 1)
        press('s')
        key('Shift_L', 0)
        key('Control_L', 0)
        save = wait_title('Save As')
        focus(save)
        subprocess.run(['import', '-window', str(save), str(captures / f'save-{scale}.png')], check=True)
        press('a', 'Control_L')
        destination = Path(config) / 'keyboard.abw'
        for ch in str(destination):
            press({'/': 'slash', '.': 'period', '-': 'minus', '_': 'minus'}.get(ch, ch.lower()), 'Shift_L' if ch == '_' or ch.isupper() else None)
        press('Return')
        time.sleep(0.5)
        assert destination.exists()
        assert 'a\tb' in destination.read_text()
        print(f'Physical keyboard and captures passed at {scale} DPI')
    finally:
        app.terminate()
        try:
            out, err = app.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            app.kill()
            out, err = app.communicate()
        if err:
            print(err.decode())
