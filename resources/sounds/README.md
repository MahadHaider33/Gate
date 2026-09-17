# Gate starter sounds

Six original, synthesized sound effects created for Gate. No recordings,
third-party samples, voices, or recognizable movie/music excerpts are used.
These assets use Gate's MIT license (see the repository's LICENSE).

Regenerate with `python scripts/generate-starter-sounds.py` (NumPy required).
The checked-in files are 48 kHz mono, 16-bit PCM WAVs with matched active
loudness, soft limiting, and short fades. Python is not needed at runtime.

The WAVs are embedded in Gate.exe. On the first launch with this pack, Gate
extracts managed copies into the user's Gate soundboard folder and adds tiles.
Removing or renaming a tile persists across restarts.
