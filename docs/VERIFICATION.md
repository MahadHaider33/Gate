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
- [ ] No recording/replay UI, sample files, microphone history, or audio in diagnostics.
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
