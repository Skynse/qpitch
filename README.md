# QPitch

QPitch is a JUCE pitch-correction audio plugin with VST3 and CLAP builds.

## Dependencies

- CMake 3.21+
- A C++17 compiler
- Git, when using CMake dependency fetching
- Runtime optional: Rubber Band Library (`librubberband`) for the higher-quality live pitch shifter. If it is not installed, QPitch falls back to its built-in phase-vocoder shifter.

Linux build packages vary by distro. On Ubuntu/Debian, start with:

```sh
sudo apt install build-essential cmake git pkg-config libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libfreetype-dev libfontconfig1-dev
```

Optional Rubber Band runtime:

```sh
sudo apt install librubberband2
```

## Build

The project can use local `JUCE/` and `clap-juce-extensions/` folders when present. If they are missing, CMake fetches them automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QPitch_VST3 QPitch_CLAP -j
```

Or with the included preset:

```sh
cmake --preset release
cmake --build --preset release
```

Build outputs:

- VST3: `build/QPitch_artefacts/Release/VST3/QPitch.vst3`
- CLAP: `build/QPitch_artefacts/Release/CLAP/QPitch.clap`

## Build With Vendored Dependencies

If you want fully offline builds, commit compatible copies of:

- `JUCE/`
- `clap-juce-extensions/` including its `clap-libs/clap` and `clap-libs/clap-helpers` subfolders

Then configure with dependency fetching disabled:

```sh
cmake -S . -B build -DQPITCH_FETCH_DEPS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QPitch_CLAP -j
```

## Notes

QPitch dynamically loads Rubber Band at runtime on Linux/macOS. This keeps builds simple and lets the plugin run without bundling Rubber Band, but the best sound quality expects the runtime library to be installed on the user machine.
