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

## Voice Changer and Soundboard

- **Voice Changer:** choose Normal, Deep, High Pitch, Robot, Radio, or Echo. Adjust Intensity, or switch Voice Effects off while keeping the selected preset. Hear Myself shares the Microphone page's test control.
- **Starter sounds:** Air Horn, Drum Roll, Rimshot, Buzzer, Chime, and Sad Trombone are included. These are original synthesized effects, available offline. Gate adds them once; renames and removals persist.
- **Soundboard:** import WAV/MP3 files, then click a sound tile to play or stop it. Playing another sound replaces the current one. The selected sound's editor offers rename and remove.
- **Hear Sounds:** plays clips through the listening device selected on the Microphone page, independently of Hear Myself. Clips use a fixed playback level.
- Clips bypass microphone noise reduction, the gate, and voice effects. They work without a microphone; local playback also works without VB-CABLE. Pause, suspend, and exit stop playback without automatically restarting it.

Imported copies live in `%LOCALAPPDATA%\Gate\Soundboard`. Removing a tile deletes its managed copy, never the original. Settings are saved under `HKCU\Software\Gate`. Microphone audio is never recorded. Pitch effects add processing latency; full listening/device compatibility verification remains manual.

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
