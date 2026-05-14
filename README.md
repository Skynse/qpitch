# QPitch

QPitch is a JUCE pitch-correction audio plugin with VST3 and CLAP builds.

## Dependencies

- CMake 3.21+
- A C++17 compiler
- Git, for cloning submodules and optional CMake dependency fetching
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

Clone with submodules so JUCE is available locally:

```sh
git clone --recurse-submodules https://github.com/Skynse/qpitch.git
cd qpitch
```

If you already cloned without submodules:

```sh
git submodule update --init --recursive
```

Then build:

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

JUCE is vendored as a Git submodule at `JUCE/`. `clap-juce-extensions` is fetched by CMake automatically unless a local `clap-juce-extensions/` folder is present.

For a fully offline build, also vendor a compatible copy of:

- `clap-juce-extensions/` including its `clap-libs/clap` and `clap-libs/clap-helpers` subfolders

Then configure with dependency fetching disabled:

```sh
cmake -S . -B build -DQPITCH_FETCH_DEPS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QPitch_CLAP -j
```

## Notes

QPitch dynamically loads Rubber Band at runtime on Linux/macOS. This keeps builds simple and lets the plugin run without bundling Rubber Band, but the best sound quality expects the runtime library to be installed on the user machine.

## Release

GitHub Releases are created by the `release` workflow. Push a version tag:

```sh
git tag v1.1.0
git push origin v1.1.0
```

The workflow builds Linux, macOS, and Windows VST3/CLAP packages and uploads:

- `QPitch-<version>-linux-vst3.zip`
- `QPitch-<version>-linux-clap.zip`
- `QPitch-<version>-macos-universal-vst3.zip`
- `QPitch-<version>-macos-universal-clap.zip`
- `QPitch-<version>-windows-vst3.zip`
- `QPitch-<version>-windows-clap.zip`

You can also run the release workflow manually from GitHub Actions and provide a version like `v1.1.0`.
