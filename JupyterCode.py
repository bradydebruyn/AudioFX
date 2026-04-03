########################### Overlay Cell ##############################
from pynq import Overlay, allocate
import numpy as np
import wave, io

ol = Overlay('audiofx.bit') #change to JuPyter Notebook path

# IP cores (ap_ctrl_hs block control + AXI-Lite param registers)
dist_ip  = ol.distortion_0
crush_ip = ol.bitcrusher_0
echo_ip  = ol.echo_0

# One DMA per effect – adjust names to match your block design
dist_dma  = ol.axi_dma_dist
crush_dma = ol.axi_dma_crush
echo_dma  = ol.axi_dma_echo

print(ol.ip_dict.keys())
#######################################################################


########################### Register Cell ##############################
# AXI-Lite offsets for parameter registers only.
# x / y are now AXI-Stream – no register offsets needed for them.

# ── distortion_0 ──────────────────────────────────────────────
DIST_CTRL      = 0x00
DIST_GAIN      = 0x10   # update these offsets to match new HLS export
DIST_THRESHOLD = 0x18

# ── bitcrusher_0 ──────────────────────────────────────────────
CRUSH_CTRL = 0x00
CRUSH_BITS = 0x10

# ── echo_0 ────────────────────────────────────────────────────
ECHO_CTRL     = 0x00
ECHO_DELAY    = 0x10
ECHO_FEEDBACK = 0x18
#######################################################################


########################### WAV Handler Cell ##############################
def load_wav(path_or_bytes):
    if isinstance(path_or_bytes, (str, bytes)):
        src = io.BytesIO(path_or_bytes) if isinstance(path_or_bytes, bytes) else path_or_bytes
    else:
        src = io.BytesIO(bytes(path_or_bytes))

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
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as wf:
        wf.setnchannels(n_ch)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(samples.astype(np.int16).tobytes())
    return buf.getvalue()


def to_mono(samples, n_ch):
    if n_ch == 1:
        return samples
    return samples.reshape(-1, n_ch).mean(axis=1).astype(np.int16)
#######################################################################


########################### DMA Driver Cell ##############################
def run_dma(ip, dma, param_writes, samples):
    """
    Send a full numpy int16 block through an AXI-Stream IP via DMA.

    param_writes : list of (offset, value) tuples for AXI-Lite param regs
    samples      : 1-D numpy int16 array (mono)
    returns      : numpy int16 array of processed samples
    """
    n = len(samples)

    # Write parameters over AXI-Lite before starting the kernel
    for offset, value in param_writes:
        ip.write(offset, int(value))

    # Allocate contiguous physical memory buffers (PYNQ allocate)
    in_buf  = allocate(shape=(n,), dtype=np.int16)
    out_buf = allocate(shape=(n,), dtype=np.int16)

    np.copyto(in_buf, samples)

    # Start the HLS kernel (ap_ctrl_hs: write 0x01 to kick off, it auto-clears)
    ip.write(0x00, 0x01)

    # Launch DMA transfers (send triggers TLAST on the last sample automatically)
    dma.sendchannel.transfer(in_buf)
    dma.recvchannel.transfer(out_buf)

    # Wait for both channels to finish
    dma.sendchannel.wait()
    dma.recvchannel.wait()

    result = np.array(out_buf, dtype=np.int16)

    # Free physical buffers
    in_buf.freebuffer()
    out_buf.freebuffer()

    return result


def apply_distortion(samples, pre_gain=4, threshold=16383):
    return run_dma(dist_ip, dist_dma,
                   [(DIST_GAIN, pre_gain & 0xFF),
                    (DIST_THRESHOLD, threshold & 0xFFFF)],
                   samples)


def apply_bitcrusher(samples, bits_to_crush=4):
    return run_dma(crush_ip, crush_dma,
                   [(CRUSH_BITS, bits_to_crush & 0xF)],
                   samples)


def apply_echo(samples, delay_ms=300, feedback=0.5, sr=48000):
    delay_samples = int(delay_ms * sr / 1000)
    feedback_q15  = int(feedback * 32768)
    return run_dma(echo_ip, echo_dma,
                   [(ECHO_DELAY,    delay_samples & 0x7FFFFFFF),
                    (ECHO_FEEDBACK, feedback_q15  & 0xFFFF)],
                   samples)
#######################################################################


########################### Upload Cell ##############################
import ipywidgets as widgets
from IPython.display import display, Audio, HTML, clear_output

upload = widgets.FileUpload(accept='.wav', multiple=False, description='Upload WAV')
display(upload)
#######################################################################


########################### Run/Download Cell ##############################
import base64, time

# ── Load uploaded file (ipywidgets v7 and v8 compatible) ─────────────────
file_info = next(iter(upload.value)) if isinstance(upload.value, dict) else upload.value[0]
raw       = file_info['content'] if isinstance(file_info, dict) else file_info.content
fname     = file_info['name']    if isinstance(file_info, dict) else file_info.name

samples, sr, n_ch = load_wav(raw)
mono = to_mono(samples, n_ch)
print(f'Loaded: {fname}  |  {sr} Hz  ·  {n_ch} ch  ·  {len(mono)} samples  ·  {len(mono)/sr:.2f}s')

# ── Sliders ───────────────────────────────────────────────────────────────
dist_box = widgets.VBox([
    widgets.IntSlider(  value=4,     min=1,    max=16,    description='Gain',      continuous_update=False),
    widgets.IntSlider(  value=16383, min=1000, max=32767, description='Threshold', continuous_update=False)
])
crush_box = widgets.VBox([
    widgets.IntSlider(  value=4,   min=0,   max=14,  description='Crush bits', continuous_update=False)
])
echo_box = widgets.VBox([
    widgets.IntSlider(  value=300, min=10,  max=1000, description='Delay (ms)', continuous_update=False),
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
show_params(None)

# ── Run button ────────────────────────────────────────────────────────────
run_btn     = widgets.Button(description='▶ Apply', button_style='success')
output_area = widgets.Output()

def on_run(btn):
    with output_area:
        clear_output(wait=True)
        effect = effect_sel.value
        t0 = time.perf_counter()

        if effect == 'Distortion':
            gain_w, thresh_w = dist_box.children
            out = apply_distortion(mono, pre_gain=gain_w.value, threshold=thresh_w.value)

        elif effect == 'Bitcrusher':
            crush_w, = crush_box.children
            out = apply_bitcrusher(mono, bits_to_crush=crush_w.value)

        elif effect == 'Echo':
            delay_w, feedback_w = echo_box.children
            out = apply_echo(mono, delay_ms=delay_w.value, feedback=feedback_w.value, sr=sr)

        elapsed = time.perf_counter() - t0
        print(f'Done in {elapsed*1000:.0f} ms  ({len(mono)/elapsed/1000:.1f}k samples/sec)')

        wav_bytes = save_wav(out, sr, n_ch=1)
        print(f'Output WAV: {len(wav_bytes)} bytes')
        display(Audio(data=wav_bytes, rate=sr, autoplay=False))

        safe_name = effect.replace(' ', '_').lower()
        out_name  = f'output_{safe_name}.wav'
        b64 = base64.b64encode(wav_bytes).decode()
        display(HTML(
            f'<a href="data:audio/wav;base64,{b64}" download="{out_name}">'
            f'⬇ Download {out_name}</a>'
        ))

run_btn.on_click(on_run)
display(effect_sel, param_area, run_btn, output_area)
#######################################################################
