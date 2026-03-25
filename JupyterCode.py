# This file contains code for the cells used in our jupyter notebook 
# Added menu select for effects as well as sliders for parameters
# Multi - FX functional


########################### Overlay Cell ##############################
from pynq import Overlay
import numpy as np
import wave, io

ol = Overlay('audiofx.bit')

# These names must match what appears in the Vivado block design
dist_ip  = ol.distortion_0
crush_ip = ol.bitcrusher_0 
echo_ip  = ol.echo_0

print(ol.ip_dict.keys())  # run this first to confirm the names
#######################################################################


########################### Register Cell ##############################

# Edit register values based on x(IP NAME)_hw.h file 

# ── distortion_0 ──────────────────────────────────────────────
DIST_CTRL      = 0x00
DIST_X         = 0x10
DIST_Y         = 0x18
DIST_GAIN      = 0x20
DIST_THRESHOLD = 0x28

# ── bitcrusher_0 ──────────────────────────────────────────────
CRUSH_CTRL     = 0x00
CRUSH_X        = 0x10
CRUSH_Y        = 0x18
CRUSH_BITS     = 0x20

# ── echo_0 ────────────────────────────────────────────────────
ECHO_CTRL      = 0x00
ECHO_X         = 0x10
ECHO_Y         = 0x18
ECHO_DELAY     = 0x20
ECHO_FEEDBACK  = 0x28
ECHO_BUFIDX    = 0x30
#######################################################################



########################### WAV Handler Cell ##############################
# Referenced StackOverflow
def load_wav(path_or_bytes):
    """
    Accept either a file path (string) or raw bytes (from an upload widget).
    Returns (samples_int16, sample_rate, num_channels).
    """
    if isinstance(path_or_bytes, (str, bytes)):
        src = io.BytesIO(path_or_bytes) if isinstance(path_or_bytes, bytes) else path_or_bytes
    else:
        src = io.BytesIO(bytes(path_or_bytes))  # ipywidgets upload content

    with wave.open(src) as wf:
        n_ch     = wf.getnchannels()
        sr       = wf.getframerate()
        n_frames = wf.getnframes()
        sw       = wf.getsampwidth()

        if sw != 2:
            raise ValueError(f'Need 16-bit PCM. Got {sw*8}-bit. '
                             'Convert: ffmpeg -i in.wav -acodec pcm_s16le out.wav')

        raw = wf.readframes(n_frames)

    samples = np.frombuffer(raw, dtype=np.int16).copy()
    return samples, sr, n_ch


def save_wav(samples, sr, n_ch=1):
    """Pack a numpy int16 array back into WAV bytes ready for download."""
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as wf:
        wf.setnchannels(n_ch)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(samples.astype(np.int16).tobytes())
    return buf.getvalue()


def to_mono(samples, n_ch):
    """Average stereo (or more) down to mono."""
    if n_ch == 1:
        return samples
    return samples.reshape(-1, n_ch).mean(axis=1).astype(np.int16)
  #######################################################################


########################### Sample Hardware Driver Cell ##############################
  def run_sample(ip, ctrl, x_off, y_off, x_val):
    """
    Send one int16 sample through an s_axilite IP and return the int16 result.
    
    The ap_ctrl_hs handshake:
      write 0x01 to ctrl  → pulse ap_start
      poll  ctrl bit 1    → wait for ap_done
      read  y_off         → grab output
    """
    # Write the input sample (mask to 16 bits for safety)
    ip.write(x_off, int(x_val) & 0xFFFF)

    # Pulse start
    ip.write(ctrl, 0x01)

    # Poll ap_done (bit 1). Typically resolves in 1–3 AXI cycles.
    # 10 000 iterations is a generous timeout (~1 ms at AXI speeds).
    for _ in range(10_000):
        if ip.read(ctrl) & 0x02:
            break

    # Read output and sign-extend from 16 bits back to Python int
    raw = ip.read(y_off) & 0xFFFF
    return raw if raw < 0x8000 else raw - 0x10000


def apply_distortion(samples, pre_gain=4, threshold=16383):
    dist_ip.write(DIST_GAIN,      pre_gain  & 0xFF)
    dist_ip.write(DIST_THRESHOLD, threshold & 0xFFFF)

    out = np.empty(len(samples), dtype=np.int16)
    for i, x in enumerate(samples):
        out[i] = run_sample(dist_ip, DIST_CTRL, DIST_X, DIST_Y, x)
    return out


def apply_bitcrusher(samples, bits_to_crush=4):
    crush_ip.write(CRUSH_BITS, bits_to_crush & 0xF)

    out = np.empty(len(samples), dtype=np.int16)
    for i, x in enumerate(samples):
        out[i] = run_sample(crush_ip, CRUSH_CTRL, CRUSH_X, CRUSH_Y, x)
    return out


def apply_echo(samples, delay_ms=300, feedback=0.5, sr=48000):
    delay_samples = int(delay_ms * sr / 1000)
    feedback_q15  = int(feedback * 32768)   # convert 0.0–1.0 to Q1.15 integer

    echo_ip.write(ECHO_DELAY,    delay_samples & 0x7FFFFFFF)
    echo_ip.write(ECHO_FEEDBACK, feedback_q15  & 0xFFFF)
    echo_ip.write(ECHO_BUFIDX,   0)  # reset circular buffer for clean start

    out = np.empty(len(samples), dtype=np.int16)
    for i, x in enumerate(samples):
        out[i] = run_sample(echo_ip, ECHO_CTRL, ECHO_X, ECHO_Y, x)
    return out
  #######################################################################





########################### Upload Cell ##############################
import ipywidgets as widgets
from IPython.display import display, Audio, HTML, clear_output
import base64, time

# ── Upload ────────────────────────────────────────────────────────────────
upload = widgets.FileUpload(accept='.wav', multiple=False, description='Upload WAV')
display(upload)
#######################################################################


########################### Run/Download Cell ##############################
import base64, time
import ipywidgets as widgets
from IPython.display import display, Audio, HTML, clear_output

# ── Load uploaded file ────────────────────────────────────────────────────
fname        = next(iter(upload.value))
raw          = upload.value[fname]['content']
samples, sr, n_ch = load_wav(raw)
mono         = to_mono(samples, n_ch)
print(f'Loaded: {fname}')
print(f'{sr} Hz  ·  {n_ch} ch  ·  {len(mono)} samples  ·  {len(mono)/sr:.2f}s')

# ── Sliders ───────────────────────────────────────────────────────────────
dist_box  = widgets.VBox([
    widgets.IntSlider(value=4,     min=1,    max=16,    description='Gain',      continuous_update=False),
    widgets.IntSlider(value=16383, min=1000, max=32767, description='Threshold', continuous_update=False)
])
crush_box = widgets.VBox([
    widgets.IntSlider(value=4, min=0, max=14, description='Crush bits', continuous_update=False)
])
echo_box  = widgets.VBox([
    widgets.IntSlider(value=300,  min=10,  max=1000, description='Delay (ms)',  continuous_update=False),
    widgets.FloatSlider(value=0.5, min=0.0, max=0.95, description='Feedback',   continuous_update=False)
])

# ── Effect selector ───────────────────────────────────────────────────────
param_map  = {'Distortion': dist_box, 'Bitcrusher': crush_box, 'Echo': echo_box}
effect_sel = widgets.ToggleButtons(options=['Distortion', 'Bitcrusher', 'Echo'])
param_area = widgets.Output()

def show_params(change):
    with param_area:
        clear_output(wait=True)
        display(param_map[effect_sel.value])

effect_sel.observe(show_params, names='value')
show_params(None)  # render default params on load

# ── Run button ────────────────────────────────────────────────────────────
run_btn     = widgets.Button(description='▶ Apply', button_style='success')
output_area = widgets.Output()

def on_run(btn):
    with output_area:
        clear_output(wait=True)
        effect = effect_sel.value

        t0 = time.perf_counter()

        if effect == 'Distortion':
            gain, thresh = dist_box.children
            out = apply_distortion(mono, pre_gain=gain.value, threshold=thresh.value)

        elif effect == 'Bitcrusher':
            crush, = crush_box.children
            out = apply_bitcrusher(mono, bits_to_crush=crush.value)

        elif effect == 'Echo':
            delay_w, feedback_w = echo_box.children
            out = apply_echo(mono, delay_ms=delay_w.value, feedback=feedback_w.value, sr=sr)

        elapsed = time.perf_counter() - t0
        print(f'Done in {elapsed*1000:.0f} ms  ({len(mono)/elapsed/1000:.0f}k samples/sec)')

        # ── Playback ──────────────────────────────────────────────────────
        wav_bytes = save_wav(out, sr, n_ch=1)
        display(Audio(data=wav_bytes, rate=sr))

        # ── Download link ─────────────────────────────────────────────────
        safe_name = effect.replace(' ', '_').lower()
        out_name  = f'output_{safe_name}.wav'
        b64       = base64.b64encode(wav_bytes).decode()
        display(HTML(
            f'<a href="data:audio/wav;base64,{b64}" download="{out_name}">'
            f'⬇ Download {out_name}</a>'
        ))

run_btn.on_click(on_run)
display(effect_sel, param_area, run_btn, output_area)
