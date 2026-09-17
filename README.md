# Gate

A microphone app for Windows 10 22H2 and Windows 11, x64.

## Install

1. Extract the Gate ZIP and run `Gate.exe`.
2. To use Gate in other apps, [download VB-CABLE](https://vb-audio.com/Cable/). Extract it, run its setup as administrator, and restart Windows.
3. Select your microphone in Gate.
4. Select **CABLE Output** as the microphone in Discord, your game, or another app.

## Use

- **Background Noise Reduction:** removes background noise. The slider blends your original and processed audio. 100% uses only processed audio.
- **Noise Gate:** mutes quiet sounds. Higher settings block more sound. Lower it if your voice gets cut off.
- **Test Microphone:** plays your processed voice through the selected listening device. Use headphones. Press **Stop Test** to stop. The meter runs only during the test. Nothing is recorded.

Testing works without VB-CABLE. You do not need to leave the test on for other apps to hear you.

Closing the window leaves Gate running in the tray. Use the tray menu to pause or exit. Pausing stops audio sent to other apps.

## Build

Requires MSVC Build Tools, Windows SDK 10.0.26100.0, CMake 3.25+, Ninja, Git, PowerShell, and tar.

```powershell
./scripts/bootstrap.ps1
./scripts/build.cmd
./scripts/package.ps1
```

The app is built in `build/release`. The ZIP is saved in `dist`.

## License

Gate uses the [MIT license](LICENSE). See [third-party licenses](THIRD_PARTY_NOTICES.md). VB-CABLE is installed separately and has its own license.
