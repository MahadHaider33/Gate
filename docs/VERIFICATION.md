# Verification and release checklist

## Automated checks

`gate_tests` checks coherent parameter persistence, bounded SPSC behavior under concurrent wraparound, dry-path latency, suppression silence, gate opening/closing, sample-rate conversion, cable identity independent of endpoint names, and a 1,000-frame processing benchmark. Inputs are synthetic developer fixtures, never microphone recordings.

`gate_probe` enumerates endpoints and, only with `--capture-seconds N`, runs the normal capture/cable path for aggregate counters. No listening output starts, and no samples are saved. Diagnostic output contains endpoint names and counts, so review it before publishing.

## Manual acceptance (not implied by a passing build)

- [ ] Natural speech: quiet consonants, laughter, different speakers, close/far microphones.
- [ ] Fans, hum, keyboards, changing room noise; assess both attenuation and speech damage.
- [ ] Strength at 0/25/50/75/100%; listen for coloration and switching artifacts.
- [ ] Gate doesn't chatter or cut word boundaries at appropriate settings.
- [ ] Test button starts/stops once; rapid clicks never produce duplicate streams.
- [ ] Test output only reaches the chosen physical listening device; cable route stays uninterrupted.
- [ ] Test without VB-CABLE installed; missing-device feedback is clear.
- [ ] Device change/loss, pause, hide, suspend, and exit stop testing without auto-restart.
- [ ] Microphone loss does not switch physical inputs without user selection.
- [ ] No microphone recording/history or audio in diagnostics; imported soundboard files remain separate.
- [ ] Windows 10 22H2 and current Windows 11; USB/integrated/wired devices, 44.1/48/96 kHz; simultaneous capture.
- [ ] Discord, Zoom, and at least two games receive CABLE Output.
- [ ] Two-hour capture/cable/listening clock-drift test, bounded memory and latency.
- [ ] Both themes, 100–200% DPI, small windows, keyboard, screen reader, high contrast.
- [ ] Installer/ZIP on a clean machine; uninstall preserves independently installed VB-CABLE.

## Performance protocol

Measure an identified older four-core x64 machine and a modern gaming machine. Report CPU seconds per wall second in units of one logical core (not ambiguous Task Manager percentages), private bytes, working set, and Windows audio-engine overhead separately.

Targets: warm/cold launch <500 ms/1.5 s; paused hidden CPU <0.1% of one core over 60 seconds; active processing <5% of one core; processing p99 <1 ms per 10 ms block; private memory <60 MiB; compressed/installed Gate <15/40 MiB; added software latency ≤40 ms on reference wired/USB devices. These are release goals, not advertised measurements.

Use WPR/WPA to examine scheduling, allocations, discontinuities and wakeups. Use PresentMon with repeated identical CPU-bound/GPU-bound game workloads: Gate stopped, paused, active hidden, active visible, and live test. Investigate repeatable >1% average FPS or >2% p99 frame-time regression beyond baseline variation.

Compare suppression candidates offline using licensed developer fixtures. WebRTC and SpeexDSP comparison, subjective speech evaluation, physical latency measurements, and game tests require a dedicated validation pass and are not replaced by synthetic unit tests.

## Release blockers

Speech damage, blend artifacts, unstable drift correction, incorrect listening/cable routing, growing latency, significant game regressions, unresolved model redistribution rights, or confusing setup must be resolved before calling a release production ready.

## Voice/soundboard development checks

`gate_feature_tests` uses synthetic input and a temporary generated WAV to check normal/effects-off bypass, bounded effect output, independent cable/listening mixes, held playback, decoding completion, and stop clearing audio. It does not open a microphone or output device. The voice/soundboard update uses the release build, existing core tests, and these focused checks only; the manual matrix above is left to the user.

Expanded voice checks measure the frequency of synthetic tones for Deep, High Pitch, half-strength pitch and Custom negative pitch; check every preset produces finite, bounded non-silent output; verify echo decay, reverb tails, exact disabled bypass, stale-tail clearing, invalid-input sanitization, preserved preset IDs, and coherent multi-slider snapshots during concurrent writes. These checks do not establish subjective voice quality. Natural speech, preset/intensity listening, custom extremes, full UI/device/theme/DPI coverage and long-running performance remain manual.

The single voice shortcut passed the release build and core/feature suites. Focused tests cover last-effect recall, manual Normal selection, disabled effects, Custom/intensity preservation, unchanged cleanup, invalid chords, hidden-window registration, conflicts and unregister/re-register. A native UI smoke check covered assigning Ctrl+Alt+V, enabling the selected effect from bypass, returning to Normal, and restoring the effect from File Explorer while Gate was hidden in the tray. Normal voice and the previous effects-off setting were restored afterward. Full game compatibility, shortcut/theme/DPI combinations, restart persistence and listening checks remain manual.

Sound shortcut update: release build and core/feature suites passed. Focused coverage includes unassigned defaults, conflicts with voice/other sound bindings, save-change detection, and bindings following clips through rename/removal. The native UI check confirmed rejecting Ctrl+Alt+V during sound assignment without triggering Voice Changer, assigning/saving Ctrl+Alt+1, triggering the matching tile and closing its settings, and clearing/saving the binding. The temporary binding was cleared afterward. Playback was observed through the app's playing indicator; listening quality, per-sound shortcuts across restarts/tray/game scenarios and the full device/UI matrix remain manual.

Shortcut conflict confirmation update: release build and both suites passed. Native UI checks verified that the prompt names Voice Changer or the specific sound, No leaves the saved bindings unchanged, and Yes transfers the chord and clears the previous owner. The transfer was checked in both directions between Voice Changer and Drum Roll; original shortcuts and voice settings were restored afterward. No audio was triggered during the prompts. External registration-failure recovery and the full UI/device matrix remain manual.

Unrestricted shortcut and inline confirmation update: release build and both suites passed. Focused tests cover plain action keys, Shift/Win modifier combinations, letter/number warning classification (including numpad digits), and malformed/modifier-only chords. Native UI checks verified the inline single-letter warning on both pages, cancellation preserving the voice binding, separate typing and overwrite confirmations before transferring I from Voice Changer to Drum Roll, and Insert saving without a typing warning. Saved values stayed unchanged until the final confirmation. Original shortcuts and voice settings were restored afterward. The updated app was left running. Windows-reserved keys, physical Fn behavior, external registration-failure recovery, and the full keyboard/theme/DPI/device matrix remain manual.

## Media sharing verification

The mixer layout adds focused tests for microphone gain/mute, simultaneous channel mutes, and preserving soundboard audio. Verify vertical dragging, keyboard arrows/page keys/Home/End, mute/unmute without losing the fader value, and meter decay when the source falls silent. Check the source picker, single Start/Stop action, and narrow-window scrolling. The output strip tells users to select CABLE Output; it does not claim to detect another app's input selection.

`scripts/build.cmd` runs the core and feature suites. Feature tests cover media gain/mute, independent local monitoring, combined clipping protection, invalid capture requests, immediate cancellation, and cleared samples after Stop.

Run `build/release/gate_app_audio_probe.exe` manually on Windows 11 with an available default output. It plays two quiet synthesized tones for about eight seconds, without opening a microphone or recording. It verifies capture of a selected process's child, exclusion of a sibling process, stop/restart, source exit, and stale process identity rejection. It is deliberately excluded from unattended CTest runs because it renders sound.

Manual call acceptance: select Opera GX on Media, start a video, choose CABLE Output in a separate call app, and check voice plus video at the remote end. Check sharing-volume zero/full, soundboard overlap, tab changes, page switching, tray hide, pause/resume, suspend/resume, source restart, and cable unplug/reconnect. Confirm that voice effects and Gate's noise controls leave media unchanged, and that Gate does not duplicate the source's local playback. Check keyboard navigation, minimum window size, light/dark theme, and DPI scaling. Test Windows 10 22H2's unsupported-state explanation separately.
