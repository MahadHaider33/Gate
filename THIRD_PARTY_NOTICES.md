# Third-party components

Gate's application code is MIT licensed. This does not relicense dependencies.

| Component | Source / license | Included functionality |
|---|---|---|
| RNNoise | https://github.com/xiph/rnnoise — BSD-3-Clause and retained source notices | Suppression, native inference, upstream generated model |
| SpeexDSP | https://github.com/xiph/speexdsp — BSD-style license | Resampler only; no codec |
| Cycfi Q | https://github.com/cycfi/q — Boost Software License 1.0 at the pinned revision | Delay, gate, envelopes, filters, oscillator and required headers |
| Signalsmith Stretch | https://github.com/Signalsmith-Audio/signalsmith-stretch � MIT | Pitch shifting |
| Signalsmith Linear | https://github.com/Signalsmith-Audio/linear � MIT | FFT support for Stretch |
| Cycfi infra | https://github.com/cycfi/infra — MIT, as declared in the included headers | Header support for Q |

Full license texts and additional source notices accompany this package in `licenses/`. RNNoise's embedded model comes from the official Xiph distribution. Source revision and model checksum information is maintained in the development repository's `dependencies.lock.json`.

Windows system APIs and the MSVC runtime are supplied under Microsoft's terms. No Windows SDK binaries are redistributed other than compiler-permitted statically linked runtime portions.

VB-CABLE is external proprietary donationware from VB-Audio: https://vb-audio.com/Services/licensing.htm. It is not included, modified, renamed, or sublicensed by Gate. Users obtain it directly under VB-Audio's terms.

Build/test tools are not runtime components. CMake, Ninja, Git, NSIS, and GitHub Actions remain subject to their respective licenses. No WebRTC, DeepFilterNet, JUCE, GPL RNNoise application wrapper, or audio codec is included in the app.

Gate's six starter sound effects are original synthesized assets, distributed under Gate's MIT license. They contain no third-party recordings or samples. The reproducible generator is scripts/generate-starter-sounds.py.
