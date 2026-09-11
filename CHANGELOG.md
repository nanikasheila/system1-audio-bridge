# Changelog

## 0.7.1 — first public prerelease

- USB stereo audio bridge for Windows x64 VST3 hosts, independent of the DAW's audio device.
- USB MIDI editing, note forwarding, DAW tempo and optional transport synchronization.
- Hardware-inspired controls, firmware 1.20 waveforms, 128-key keyboard and foldable performance area.
- Live USB scope and illustrative waveform, envelope and filter graphs.
- Preset library with search, bank filter, favorites, rename, snapshots, import/export and archive.
- Partial PRM conversion tested against 64 official Volume 1–4 files (not redistributed).
- Fix voice mode mapping: CC119 0=Poly, 64=Mono, 127=Unison. Earlier private builds mislabeled these values. Existing raw parameter values are preserved.
- Public repository layout, pinned dependency, automated Windows build/tests, binary/full-source packages and checksums.

Earlier 0.2–0.7 builds were local development iterations. This release preserves plugin IDs and state format v5, with compatibility readers for older v1/v3/v4 states.
