# Building on Windows x64

Requirements: Visual Studio 2022 (Desktop development with C++ and a Windows SDK), CMake 3.22+, Git, PowerShell. No Roland hardware or driver is required to build or run automated tests.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target System1Bridge_VST3 System1ControlTest System1EditorCheck System1LibraryTest ReaderTest ClockTest ScopeTest --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

JUCE is fixed at commit `72782788ce18c2d4d760b28e0921d6ffc6431102` (9.0.2). CMake downloads this revision when no local dependency is present. The full-source release ZIP already contains it at `dependencies/JUCE` for an offline build. For an existing checkout, supply `-DJUCE_PATH=C:/path/to/JUCE` at that exact revision.

The plugin is in `build/System1Bridge_artefacts/Release/VST3/`. This folder also contains `System1Capture.exe` next to the `.vst3` bundle. The helper is also built under `build/System1Capture_artefacts/Release/`. Both binaries use the static Microsoft C++ runtime. ASIO is disabled in these targets; the DAW independently owns its chosen audio interface.

`BUILD_TESTING` defaults to ON. Automated tests exercise audio drift, MIDI queue/state, clock planning, scope, library handling and editor rendering. They do not open MIDI ports or require physical sound banks. The editor test writes images under its working directory's `work/` folder.

## Packaging a release

After a clean build and tests:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -BuildDir build -JuceDir build/_deps/juce-src
```

Use `-JuceDir dependencies/JUCE` when building from the full-source archive. When packaging from a Git checkout, the JUCE revision and working tree must match the pin. Packaging copies only the public repository files and the dependency source, omitting build output, repository metadata, private recordings and local presets. It writes Windows x64 and full-source ZIPs plus SHA256SUMS.txt into `dist/`.

The source package includes the upstream dependency notices embedded throughout JUCE. GitHub's automatic source archive does not include downloaded dependencies. Always attach the full-source asset when distributing binaries.

GitHub Actions builds and tests on `windows-2022` for pushes, pull requests and manual runs. Release publication remains an explicit maintainer action.
