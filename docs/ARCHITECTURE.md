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

Voice presets share a preallocated effect chain: Signalsmith pitch/formant shifting, a 16-band Q-filter vocoder with a band-limited carrier and consonant preservation, vibrato/ring modulation, Q cut/shelf filters, compression and soft distortion, a damped feedback echo, and a small Schroeder reverb. Intensity scales the preset's actual pitch, tone and effect amounts; Custom exposes 20 independent controls instead. Dry/wet Custom mixing uses the pitch engine's reported latency. Normal/off becomes an exact bypass after a short crossfade, and the FFT runs only for pitch/formant changes. Effect startup primes latency buffers, parameters are smoothed, tails clear on re-enable, and final output is finite and bounded. Denormal handling is scoped to effect processing.

Voice parameters cross into the mixer through atomics with a version check. A concurrent edit leaves the audio reader on its previous coherent snapshot without waiting. Original preset IDs and microphone preferences are preserved; Custom values live under the separately versioned CustomVoice registry subkey. UI controls use the existing native trackbars, mint tiles, theme and deferred settings writes. No additional runtime dependencies or models are installed.

ClipPlayer streams mixer-ready mono PCM16 WAVs directly on its background worker; other WAV/MP3 formats use Media Foundation on that worker, downmixes, and uses SpeexDSP conversion into a bounded one-second SPSC buffer. Control owns replacement/stop, disables readers and joins decoding before queue reset. New playback waits for output initialization. Imported files are copied into the user's Gate soundboard directory, with metadata in versioned registry settings. No captured microphone audio is persisted.

Cable receives processed speech plus clips. Listening independently includes speech when Test Microphone/Hear Myself is enabled and clips when Hear Sounds is enabled. Hiding cancels speech monitoring but leaves clips active. Pause, suspend, and exit stop clips; natural clip completion allows the listening output to drain before closing.

The starter sound pack consists of six original PCM WAVs embedded as Win32 RCDATA. First launch after the feature update atomically extracts managed copies and saves their metadata with a StarterPackVersion marker. Later launches do not re-add removed clips or reset names. Existing imports are retained. No runtime network, Python, or audio generation is required.

The single voice shortcut uses RegisterHotKey with MOD_NOREPEAT on the existing window/message thread, with no keyboard hook or polling. It swaps Normal with the last selected non-Normal preset, preserving intensity and Custom controls. Shortcut activation explicitly enables an effect if effects were off; ordinary tile selection still preserves the toggle. The chord and remembered preset persist under VoiceShortcutVersion 1. Conflicts remain inactive, capture temporarily unregisters the chord, cancellation restores it, hiding keeps registration, and exit unregisters it.

Sound shortcuts use the same native registration and shared picker. Each clip stores an optional Shortcut DWORD, gated by ClipShortcutsVersion 1. Action keys can be used without modifiers; native registration determines Windows availability. Only bare A–Z, top-row numbers and numpad numbers require the typing confirmation. Conflicts inside Gate require a separate confirmation naming the former owner. Both confirmations use native buttons inline below the picker, with wrapped text measured only when its text or width changes. No shortcut dialogs are used. The pending chord and confirmations stay separate from saved settings; both approvals are required when a bare typing key is already assigned. Cancel, changing the editor/page, or deactivating Gate discards the pending edit and restores the original registration. Keyboard focus may move between the picker and its inline action buttons without losing the edit. Approved replacement releases the former owner, registers the new owner and saves both bindings together; registration failure restores the former owner and shows an inline error. Gate hotkeys received while capturing become candidate chords rather than triggering audio; while confirmation is pending they are ignored. Playback uses the tile's existing play/stop command, closes clip settings and never changes microphone monitoring. Rebuilding clips releases and rebuilds their registrations, avoiding stale index bindings after removal; all registrations end on exit. No polling or keyboard hook is added.

## App audio capture

The Media mixer has independent microphone/app levels and mute switches, published together as one atomic value. Microphone gain follows voice effects and applies to cable and microphone monitoring; app gain affects only the cable mix. Soundboard gain is independent. Two post-gain peak meters use atomic diagnostics with a short visual decay; the UI refreshes only meter rectangles while Media is visible. The faders retain native trackbar accessibility with custom vertical painting and matching mouse/keyboard direction. Mixer levels are session-only.

The Media page enumerates visible application windows and identifies each process by PID and creation time. It captures only the explicitly selected process tree through WASAPI process loopback (`ActivateAudioInterfaceAsync`, include-target-process-tree mode). The UI thread never opens an audio stream. Activation runs on a separate MTA worker with an agile, independently owned completion handler; cancellation and a bounded activation timeout are supported. There is no whole-system capture fallback on unsupported Windows versions.

AppAudio requests 48 kHz stereo PCM16 from Windows, downmixes to mono, and sends it through a bounded SPSC transport. The mixer consumes it after microphone effects and combines voice, clips, and media with a single final clamp. Local monitoring excludes media because the source app already plays it. Old queued data is discarded after a stall. Controller-side start/stop joins the mixer before capture teardown and queue reset; UI/control commands and audio buffers never share locks.

App selection and sharing state are session-only. Capturing is explicit, and process exit, pause, suspend, or exit require another Start Sharing. Creation-time checks reject stale picker entries even if Windows reuses a PID. The stream retains the process handle and monitors its exit. A missing cable disables Start; output loss during capture is shown on Media. No media audio is persisted.

## UI responsiveness

Page geometry and visibility updates are deferred as one batch, with unchanged bounds skipped and one queued redraw. Button actions update only affected controls; unchanged audio status is ignored by the UI, while the microphone meter retains its separate visible-only timer. DirectWrite layouts use a bounded 192-entry LRU, and window render targets retain contents and present without per-window vsync waits. Settings writes are coalesced for 400 ms; clip metadata is rewritten only when changed, with a final flush on exit. No new runtime dependencies or background UI animation loops are introduced.

Scrolling retargets a frame-rate-independent damped motion instead of restarting the timer on every wheel event. A waitable high-resolution clock wakes the existing UI thread only during motion, with a normal window-timer fallback and support for disabled Windows animations. Scroll frames move cached active-page controls in one deferred batch and finish one repaint without re-running page layout or visibility checks. Positions snap to physical pixels. The animation clock stops on completion, page changes, resizing, hiding and minimization; it adds no worker thread or system-wide timer-resolution changes.

While Soundboard is visible and Hear Sounds is enabled, the listening route stays ready with digital silence between clips. This avoids reopening the device on each click and does not enable microphone monitoring. Hiding/minimizing/leaving Soundboard releases the warm route when neither playback nor monitoring needs it. Starter WAV playback bypasses decoder-graph startup with a bounded streaming buffer, without caching entire audio files.
