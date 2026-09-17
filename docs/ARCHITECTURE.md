# Architecture

Gate has one UI thread, one control thread, one event-driven capture/processing worker, a paced mixing worker, a background clip decoder while playing, and a render worker for each active output. The listening render worker exists during microphone monitoring or local clip playback. UI and control messages never synchronously call into audio processing.

## Data flow

WASAPI input mix format → float mono → SpeexDSP 48 kHz conversion → 480-sample framing → RNNoise and aligned dry blend → Q noise gate/envelope → independent cable/listening streaming queues → SpeexDSP endpoint-rate conversion → WASAPI render.

RNNoise's pinned implementation adds a frame for analysis overlap and another for delayed spectrum processing: 960 samples (20 ms). Q's delay aligns the dry path. Device buffering, frame assembly, output conversion, and cable transit add latency beyond this figure. The application does not claim a 10 ms end-to-end delay.

Each output has a fixed-capacity SPSC queue. The control thread disables its producer and waits for in-flight writes before joining a stopped render thread and clearing storage. Capture never waits for the UI or control thread. Enabling/stopping the listener does not rebuild the capture processor or primary cable stream.

The output clock is adjusted through `IAudioClockAdjustment` from the control thread using bounded queue-fill correction. This is transport control of Windows' resampler, not a Gate resampling algorithm. Queue overflow triggers explicit recovery instead of unbounded latency accumulation. Processing faults and missing input produce silence.

## Ownership and state

The control worker owns device initialization and teardown. Endpoint changes are event driven. Active operation wakes the control worker periodically for clock management; paused operation waits for commands/notifications. Processing parameters are packed into one atomic word, giving a consistent snapshot per block. Device IDs and visible status are protected outside the real-time path.

One requested test flag controls the speech contribution to the listening output. Active test state is reported only after successful initialization. Device change, hide, suspend, pause, and exit cancel the request; restoration never restarts listening automatically. No audio samples enter preferences or diagnostics.

Preferences are versioned under `HKCU\Software\Gate`; imported soundboard files are stored separately, never microphone audio. Theme changes arrive through Windows `UISettings`. The interface uses native controls for keyboard/accessibility behavior and custom painting for appearance.

## Extension boundary

An effect can later implement prepare/reset/process/latency and be inserted after cleanup. Do not add a dynamic plugin graph or new audio algorithm without a concrete requirement unmet by established libraries.

## Voice and soundboard

The mixer produces 480-sample blocks at 48 kHz using a waitable timer. Its period follows microphone queue fill with bounded drift correction; missing input becomes silence. Each output keeps its existing independent clock correction. Voice effects use Q components and pinned Signalsmith Stretch/Linear; dry and wet effect paths are latency-aligned, with smoothed preset/intensity changes. Normal voice bypasses the effect delay.

ClipPlayer streams mixer-ready mono PCM16 WAVs directly on its background worker; other WAV/MP3 formats use Media Foundation on that worker, downmixes, and uses SpeexDSP conversion into a bounded one-second SPSC buffer. Control owns replacement/stop, disables readers and joins decoding before queue reset. New playback waits for output initialization. Imported files are copied into the user's Gate soundboard directory, with metadata in versioned registry settings. No captured microphone audio is persisted.

Cable receives processed speech plus clips. Listening independently includes speech when Test Microphone/Hear Myself is enabled and clips when Hear Sounds is enabled. Hiding cancels speech monitoring but leaves clips active. Pause, suspend, and exit stop clips; natural clip completion allows the listening output to drain before closing.

The starter sound pack consists of six original PCM WAVs embedded as Win32 RCDATA. First launch after the feature update atomically extracts managed copies and saves their metadata with a StarterPackVersion marker. Later launches do not re-add removed clips or reset names. Existing imports are retained. No runtime network, Python, or audio generation is required.

## UI responsiveness

Page geometry and visibility updates are deferred as one batch, with unchanged bounds skipped and one queued redraw. Button actions update only affected controls; unchanged audio status is ignored by the UI, while the microphone meter retains its separate visible-only timer. DirectWrite layouts use a bounded 192-entry LRU, and window render targets retain contents and present without per-window vsync waits. Settings writes are coalesced for 400 ms; clip metadata is rewritten only when changed, with a final flush on exit. No new runtime dependencies or background UI animation loops are introduced.

While Soundboard is visible and Hear Sounds is enabled, the listening route stays ready with digital silence between clips. This avoids reopening the device on each click and does not enable microphone monitoring. Hiding/minimizing/leaving Soundboard releases the warm route when neither playback nor monitoring needs it. Starter WAV playback bypasses decoder-graph startup with a bounded streaming buffer, without caching entire audio files.
