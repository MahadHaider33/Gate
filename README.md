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

- **Voice Changer:** choose Normal, Deep, High Pitch (helium/chipmunk), Masculine, Feminine, Baby / Child, Robot, Monster, Alien, Radio, Telephone, Echo, Reverb, Loud Mic, or Custom. Intensity changes the strength of each preset. Selecting a voice preserves the Voice Effects toggle, and controls stay adjustable while effects are off. Hear Myself shares the Microphone page's test control.
- **Loud Mic:** bass-heavy, compressed, blown-out microphone distortion with bounded output.
- **Voice shortcut:** Ctrl + Alt + V switches between Normal and your last selected effect, including Custom. It works on other pages, in other apps, and while Gate is in the tray. Click the shortcut on Voice Changer to change it, or Clear to disable it. Unavailable shortcuts show an error and stay inactive. Microphone cleanup and monitoring settings stay unchanged.
- **Custom:** 20 sliders for pitch, resonance, bass, treble, low/high cut, compression, distortion, robot amount/tone, alien modulation amount/speed, vibrato amount/speed, echo amount/delay/feedback, reverb, effect mix, and output level. Changes apply live and save automatically. Reset Custom restores neutral settings; Back to voices keeps your custom settings. These are local audio effects, with no AI models or downloads.
- **Starter sounds:** Air Horn, Drum Roll, Rimshot, Buzzer, Chime, and Sad Trombone are included. These are original synthesized effects, available offline. Gate adds them once; renames and removals persist.
- **Soundboard:** import WAV/MP3 files, then click a sound tile to play or stop it. Playing another sound replaces the current one and closes the editor. Open a tile's settings to rename it inline, remove it, or choose a color emoji from a searchable grid with categories.
- **Sound shortcuts:** open a sound's settings, click **Set shortcut**, and press your keys. It plays/stops that sound from any app, including while Gate is in the tray. Plain keys and Shift combinations are supported; Ctrl/Alt are optional. A single letter or number asks for confirmation below the button because it can trigger while typing. If another Gate action uses those keys, a separate inline confirmation names it and offers **Replace** or **Cancel**. Saved shortcuts stay unchanged until all required confirmations are accepted. Use **Cancel** to leave capture and **Clear** to remove a binding. Keys reserved by Windows or another app show an inline error; Fn depends on what your keyboard sends to Windows.
- **Hear Sounds:** plays clips through the listening device selected on the Microphone page, independently of Hear Myself. Clips use a fixed playback level.
- Clips bypass microphone noise reduction, the gate, and voice effects. They work without a microphone; local playback also works without VB-CABLE. Pause, suspend, and exit stop playback without automatically restarting it.

Imported copies live in `%LOCALAPPDATA%\Gate\Soundboard`. Removing a tile deletes its managed copy, never the original. Settings are saved under `HKCU\Software\Gate`. Microphone audio is never recorded. Pitch effects add processing latency; full listening/device compatibility verification remains manual.

Closing the window leaves Gate running in the tray. Use the tray menu to pause or exit. Pausing stops audio sent to other apps.

## Media sharing

On Windows 11, open **Media**, use **Change app** to select an open app such as **opera.exe — Opera GX**, then click **Start sharing**. Play the video or music in that app and select **CABLE Output** as the microphone in your call. The two mixer cards have independent microphone and app-audio faders, live output meters, and mute buttons. Muting either channel preserves the other channel and soundboard; **Stop sharing** stops only the app audio. Levels and mute settings apply across pages for the current session and reset when Gate restarts. Microphone level also affects Hear Myself. Use Up/Down or Page Up/Down on a fader; Home selects 100% and End selects 0%.

The app's whole process tree is shared, including browser tabs. Keep your call in a separate app to avoid echo. Media bypasses microphone cleanup and voice effects and is mixed to mono for Gate's microphone output. You continue hearing it through its original app; Gate does not play a second local copy. Call-app noise suppression may filter music; disable it in that app if needed.

Refresh Apps after opening or restarting an app. Sharing never starts automatically and stops on app exit, pause, sleep, or Gate exit. Switching pages or closing Gate to the tray keeps sharing active. No media is recorded or saved. Protected media may not be capturable. App capture requires Windows build 20348 or later; the Media page explains the limitation on Windows 10 22H2 while existing microphone and soundboard features remain available.

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
