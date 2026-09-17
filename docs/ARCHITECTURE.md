# Architecture

Gate has one UI thread, one control thread, one event-driven capture/processing worker, and a render worker for each active output. The listening render worker exists only during Test Microphone. UI and control messages never synchronously call into audio processing.

## Data flow

WASAPI input mix format → float mono → SpeexDSP 48 kHz conversion → 480-sample framing → RNNoise and aligned dry blend → Q noise gate/envelope → independent cable/listening streaming queues → SpeexDSP endpoint-rate conversion → WASAPI render.

RNNoise's pinned implementation adds a frame for analysis overlap and another for delayed spectrum processing: 960 samples (20 ms). Q's delay aligns the dry path. Device buffering, frame assembly, output conversion, and cable transit add latency beyond this figure. The application does not claim a 10 ms end-to-end delay.

Each output has a fixed-capacity SPSC queue. The control thread disables its producer and waits for in-flight writes before joining a stopped render thread and clearing storage. Capture never waits for the UI or control thread. Enabling/stopping the listener does not rebuild the capture processor or primary cable stream.

The output clock is adjusted through `IAudioClockAdjustment` from the control thread using bounded queue-fill correction. This is transport control of Windows' resampler, not a Gate resampling algorithm. Queue overflow triggers explicit recovery instead of unbounded latency accumulation. Processing faults and missing input produce silence.

## Ownership and state

The control worker owns device initialization and teardown. Endpoint changes are event driven. Active operation wakes the control worker periodically for clock management; paused operation waits for commands/notifications. Processing parameters are packed into one atomic word, giving a consistent snapshot per block. Device IDs and visible status are protected outside the real-time path.

One requested test flag controls one listening output. Active test state is reported only after successful initialization. Device change, hide, suspend, pause, and exit cancel the request; restoration never restarts listening automatically. No audio samples enter preferences or diagnostics.

Preferences are versioned under `HKCU\Software\Gate`; no audio data is stored. Theme changes arrive through Windows `UISettings`. The interface uses native controls for keyboard/accessibility behavior and custom painting for appearance.

## Extension boundary

An effect can later implement prepare/reset/process/latency and be inserted after cleanup. Do not add a dynamic plugin graph or new audio algorithm without a concrete requirement unmet by established libraries.
