# Validation scope — v0.7.1

The public source was built from a new build directory with Visual Studio 2022, Windows SDK, Release x64 and the pinned JUCE revision. The release build uses the static Microsoft C++ runtime and disables ASIO in the plugin/helper.

## Automated checks

All six checks passed: audio reader drift, clock planning, output scope, MIDI control/state, editor rendering and preset library. The first clean run exposed a missing output directory in the editor test; the test now creates that directory and passed on rerun. No hardware is needed for these checks.

Coverage includes:

- Audio-rate conversion with clock drift and bounded buffering.
- MIDI queues, CC state, voice-mode text/value mapping and old state compatibility.
- Clock scheduling and transport transitions.
- Scope snapshots and concurrent read/write handling.
- 128-key hit testing, 12 waveform choices, COLOR guides, shared OSC alignment, 37 double-click resets with host gestures, and collapsed layout.
- Library parsing, snapshot round trips, deduplication, original-byte preservation, rename, favorites, archive and editor recall preserving USB gain/clock.

## Hardware observations

During development, USB capture and MIDI control were exercised on a physical SYSTEM-1 using Windows 11, Bitwig Studio 6 and an RME Fireface UCX II. Voice-mode mapping was corrected after comparing the GUI with the hardware's Poly operation and MONO LED. Official Volume 1–4 banks (64 files) were parsed with 49 mapped CCs per file. Bank files and user recordings are not redistributed.

The freshly packaged VST3 was also loaded by a VST3 test host and its editor/state instantiated. The short capture attempt was silent, so it is not counted as an audio recording pass. Earlier development recordings are not presented as measurements of this new public build.

## Not established

These checks do not establish support for every VST3 host, exact round-trip latency, full PRM sound reproduction, PLUG-OUT, SYSTEM-1m, or sample-accurate MIDI timing. GUIDE graphs are illustrative and were not calibrated against the hardware. See README for user-facing limits.
