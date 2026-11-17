"""RRDBG-rotor: run Rotor Rescue in WinUAE (own config, own window), play a scripted flight with
real key events, and record the default playback device (WASAPI loopback) to a WAV. This is how the
rotor v4 cause was measured (SPEC section 9): vAmiga's recording is not what WinUAE plays.

Windows Python 3.12 with `pip install pyaudiowpatch numpy pillow`; run it from Windows (not WSL):
    set RR_SCRIPT=flight|short|rescue   (default flight; `probe` takes a screenshot every 3 s)
    set RR_ADF=J:\\amiga\\play\\first\\first.adf   (the build to play; default Paul's staged one)
    set RR_W2=1                         (Ctrl+L, W, 4 first: world 2)
    python winuae_capture.py OUTNAME [key=value ...]      extra .uae lines (e.g. sound_interpol=sinc)
Paths are this studio PC's (J:\\amiga\\WinUAE, J:\\amiga\\roms-agk, J:\\amiga\\play\\rrdbg-rotor staging,
C:\\Users\\Public\\claude-tmp\\rotor4\\cap output): edit the constants at the top. The window is titled
RRDBG-rotor-OUTNAME so it cannot be taken for the real game. Writes OUTNAME.wav (stereo) and
OUTNAME.json (marks in seconds of the stream) and OUTNAME-*.png screenshots. Key timing assumes the
title is up 39 s after launch (a floppy boot; RR_TITLE_AT); the screenshots say whether it was.
Never kill a WinUAE it did not start.
"""
import ctypes, json, os, subprocess, sys, threading, time, wave
import numpy as np
import pyaudiowpatch as pa

OUT = r"C:\Users\Public\claude-tmp\rotor4\cap"
os.makedirs(OUT, exist_ok=True)
WINUAE = r"J:\amiga\WinUAE\winuae64.exe"
ADF = sys.argv[3] if len(sys.argv) > 3 and not "=" in sys.argv[3] else r"J:\amiga\play\first\first.adf"
name = sys.argv[1]
extra = [a for a in sys.argv[2:] if "=" in a]

STAGE = r"J:\amiga\play\rrdbg-rotor"
os.makedirs(STAGE, exist_ok=True)
cfg = os.path.join(STAGE, f"RRDBG-rotor-{name}.uae")
lines = ["config_description=RRDBG-rotor " + name, "config_hardware=true", "config_host=true", "use_gui=no",
         r"kickstart_rom_file=J:\amiga\roms-agk\amiga-os-130.rom",
         "floppy0=" + os.environ.get("RR_ADF", r"J:\amiga\play\first\first.adf"), "floppy0type=0", "nr_floppies=1", "floppy_speed=0",
         "cpu_compatible=true", "cycle_exact=false", "cpu_speed=real", "joyport0=mouse", "joyport1=kbd2",
         "gfx_width=720", "gfx_height=568", "gfx_linemode=double", "sound_output=exact",
         "chipset=ocs", "cpu_model=68000", "chipmem_size=1", "bogomem_size=2"] + extra
with open(cfg, "w", newline="\r\n") as f:
    f.write("\n".join(lines) + "\n")

user32 = ctypes.windll.user32
KEYS = {"up": (0x26, 0x48, 1), "down": (0x28, 0x50, 1), "left": (0x25, 0x4B, 1), "right": (0x27, 0x4D, 1),
        "fire": (0xA3, 0x1D, 1), "m": (0x4D, 0x32, 0), "esc": (0x1B, 0x01, 0), "l": (0x4C, 0x26, 0), "w": (0x57, 0x11, 0),
        "4": (0x34, 0x05, 0), "lctrl": (0xA2, 0x1D, 0), "p": (0x50, 0x19, 0), "space": (0x20, 0x39, 0)}


def key(k, down):
    vk, sc, ext = KEYS[k]
    fl = (1 if ext else 0) | (0 if down else 2)
    user32.keybd_event(vk, sc, fl, 0)


def find_window(pid):
    found = []
    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def cb(h, _):
        p = ctypes.c_ulong()
        user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h):
            n = user32.GetWindowTextLengthW(h)
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(h, buf, n + 1)
            if n:
                found.append((h, buf.value))
        return True
    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return found


# ---- recorder
p = pa.PyAudio()
dev = p.get_default_wasapi_loopback()
rate, ch = int(dev["defaultSampleRate"]), dev["maxInputChannels"]
chunks = []   # (t_arrival, array)
stop = False
t0 = time.perf_counter()


pos = 0   # samples recorded so far (including gap fill)


def rec():
    global pos
    st = p.open(format=pa.paInt16, channels=ch, rate=rate, input=True, input_device_index=dev["index"], frames_per_buffer=1024)
    while not stop:
        d = st.read(1024, exception_on_overflow=False)
        a = np.frombuffer(d, dtype="<i2").reshape(-1, ch)
        now = time.perf_counter() - t0
        behind = now - (pos + len(a)) / rate          # how far the wall clock is ahead of the samples
        if behind > 0.4:                              # loopback sends nothing while the device is silent: fill the gap
            z = np.zeros((int((behind - 0.02) * rate), ch), dtype="<i2")
            chunks.append((now, z)); pos += len(z)
        chunks.append((now, a)); pos += len(a)
    st.stop_stream(); st.close()


th = threading.Thread(target=rec, daemon=True)
th.start()
marks = {}


def mark(n):
    marks[n] = time.perf_counter() - t0
    marks[n + "@s"] = pos / rate
    print(f"{marks[n]:7.2f} {n}  (stream {pos/rate:.2f})", flush=True)


proc = subprocess.Popen([WINUAE, "-f", cfg])
mark("launch")
for _ in range(100):
    time.sleep(0.2)
    w = find_window(proc.pid)
    if w:
        break
print("windows:", w, flush=True)
hwnd = w[0][0]


from PIL import ImageGrab


def focus():
    for _ in range(3):
        user32.keybd_event(0x12, 0x38, 0, 0); user32.keybd_event(0x12, 0x38, 2, 0)
        user32.ShowWindow(hwnd, 9)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.15)
        if user32.GetForegroundWindow() == hwnd:
            return True
    print("WARNING: could not focus WinUAE", flush=True)
    return False


def shot(tag):
    class R(ctypes.Structure):
        _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long), ("r", ctypes.c_long), ("b", ctypes.c_long)]
    r = R(); user32.GetWindowRect(hwnd, ctypes.byref(r))
    try:
        ImageGrab.grab((r.l, r.t, r.r, r.b)).save(os.path.join(OUT, f"{name}-{tag}.png"))
    except Exception as e:
        print("shot failed", e)


def level_now(sec=0.5):
    """rms of the last `sec` seconds recorded (both channels), and its fraction above 1 kHz"""
    n = int(sec * rate)
    got, tot = [], 0
    for t, a in reversed(chunks):
        got.append(a); tot += len(a)
        if tot >= n: break
    if not got: return 0.0, 0.0
    x = np.concatenate(got[::-1]).astype(float).mean(axis=1) / 32768
    x = x - x.mean()
    if len(x) < 512: return 0.0, 0.0
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / rate)
    return float(np.sqrt((x ** 2).mean())), float(X[f > 1000].sum() / max(X.sum(), 1e-12))


def tap(k, dur=0.12):
    focus()
    key(k, True); time.sleep(dur); key(k, False)


def hold(keys, dur):
    focus()
    for k in keys: key(k, True)
    time.sleep(dur)
    for k in keys: key(k, False)


def finish():
    out = np.concatenate([c for _, c in chunks]) if chunks else np.zeros((1, ch), dtype="<i2")
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(out.tobytes())
    json.dump({"rate": rate, "marks": {k[:-2]: v for k, v in marks.items() if k.endswith("@s")}, "wall": {k: v for k, v in marks.items() if not k.endswith("@s")}, "cfg": lines}, open(os.path.join(OUT, name + ".json"), "w"), indent=1)
    print("done", name, "rate", rate, "ch", ch)


SCRIPT = os.environ.get("RR_SCRIPT", "flight")
if SCRIPT == "probe":
    for i in range(24):
        time.sleep(3); mark(f"p{i}"); shot(f"p{i:02d}")
    mark("end")
    SCRIPT = "x"
    stop = True; th.join(3); proc.kill(); sys.exit(0)
time.sleep(max(0, float(os.environ.get("RR_TITLE_AT", "39")) - (time.perf_counter() - t0)))
mark("title_music")
time.sleep(2.5)
shot("title")
if os.environ.get("RR_MUSIC_OFF"):
    tap("m"); mark("music_off"); time.sleep(1.5)
if os.environ.get("RR_W2"):
    focus()
    key("lctrl", True); time.sleep(0.15); tap("l", 0.12); key("lctrl", False); time.sleep(0.4)
    tap("w", 0.12); time.sleep(0.4); tap("4", 0.12); mark("w2_requested")
    time.sleep(float(os.environ.get("RR_W2_LOAD", "14"))); shot("w2_card"); mark("w2_card")
tap("fire"); mark("fire_start")
if SCRIPT == "rescue":
    F = 0.02
    def fr(n): time.sleep(n * F)
    def pr(k, n):
        keys = k if isinstance(k, list) else [k]
        focus()
        for kk in keys: key(kk, True)
        fr(n)
        for kk in keys: key(kk, False)
    fr(2); mark("intro"); fr(125)
    mark("lift"); pr("up", 30); mark("fly_out"); pr("left", 170); fr(60); mark("land_prep"); pr("down", 70)
    shot("landed_barracks"); mark("landed")
    pr("fire", 1); fr(25); pr("fire", 1); fr(30); mark("barracks_open"); fr(90); shot("boarding"); mark("boarding"); fr(200)
    shot("full"); mark("loaded")
    pr("up", 30); mark("fly_home"); pr("right", 180); fr(50); pr("down", 70); mark("landed_home"); shot("home")
    fr(200); mark("saved_run"); fr(120); mark("end")
    stop = True; th.join(3); proc.kill(); finish(); sys.exit(0)
if SCRIPT != "done": time.sleep(1.5); shot("after_fire")
# wait for the game to go to PLAY: the music stops (title theme is bright, the rotor is not)
t_wait = time.perf_counter()
if not os.environ.get("RR_MUSIC_OFF") and not os.environ.get("RR_W2"):
    quiet = 0
    while time.perf_counter() - t_wait < 20:
        r, hf = level_now(0.5)
        quiet = quiet + 1 if hf < 0.02 else 0
        if quiet >= 3: break
        time.sleep(0.25)
else:
    time.sleep(4)
mark("play_started")
shot("play")
time.sleep(3); mark("idle")
hold(["up"], 0.3); mark("takeoff")
if SCRIPT == "short":
    time.sleep(14); mark("hover"); time.sleep(8); mark("hover_end"); shot("hover")
    hold(["right"], 4.0); mark("cruise_end")
    time.sleep(2); mark("end")
    stop = True; th.join(3); proc.kill()
    SCRIPT = "done"
if SCRIPT != "done":
  time.sleep(9); mark("hover"); shot("hover")
  hold(["right"], 4.0); mark("cruise_end"); shot("cruise")
if SCRIPT != "done":
  time.sleep(1.5); mark("coast")
  hold(["up"], 1.2); mark("climb_end")
  time.sleep(1.5); mark("hover2")
  hold(["right", "down"], 1.2); mark("descend_end")
  time.sleep(1.0)
  tap("fire", 0.1); time.sleep(0.4); tap("fire", 0.1); mark("cannon")
  time.sleep(3.0)
  shot("end")
  mark("end")
if SCRIPT != "done":
  stop = True
  th.join(3)
  proc.kill()
out = np.concatenate([c for _, c in chunks]) if chunks else np.zeros((1, ch), dtype="<i2")
with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
    w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(out.tobytes())
json.dump({"rate": rate, "marks": {k[:-2]: v for k, v in marks.items() if k.endswith("@s")}, "wall": {k: v for k, v in marks.items() if not k.endswith("@s")}, "cfg": lines}, open(os.path.join(OUT, name + ".json"), "w"), indent=1)
print("done", name, "rate", rate, "ch", ch)
