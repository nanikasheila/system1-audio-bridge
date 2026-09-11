# SYSTEM-1 Audio Bridge

A Windows x64 VST3 that captures Roland SYSTEM-1 USB stereo audio while your DAW keeps its usual audio interface. It also forwards DAW MIDI notes, edits supported synth controls over USB MIDI, follows DAW tempo, and manages presets.

**Requires a physical SYSTEM-1 and the Roland driver.** This is an independent project, not affiliated with Roland or Elektron. The first public release, v0.7.1, is a prerelease. Tested with Windows 11, Bitwig Studio 6 and an RME Fireface UCX II; other hosts are not yet hardware-tested.

## Install

Download the Windows x64 ZIP from [Releases](https://github.com/nanikasheila/system1-audio-bridge/releases). Save and close any DAW projects using an older version. Copy both items from its `VST3` directory into `%LOCALAPPDATA%\Programs\Common\VST3\System1Bridge`:

```text
SYSTEM-1 Audio Bridge.vst3/
System1Capture.exe
```

Keep the helper **beside the bundle**, not inside it. Add that directory to your DAW's VST3 search paths and rescan. Add the plugin to an instrument track. The binary is unsigned; it uses the static Microsoft C++ runtime.

Connect the SYSTEM-1 by USB. Look for `USB connected` and `USB MIDI ready`. Hold the hardware MANUAL button to receive current controls. `*` marks an unsynced value. Choose **VOICE MODE → Poly** for chords. To synchronize tempo, choose `DAW tempo` and set the hardware MIDI Clock Source to `AUTO`.

## Presets

Search, bank filtering, favorites, rename, recoverable archive, folder import, original-file export and `.s1preset` snapshots are included. Download official sound banks separately from [Roland](https://www.roland.com/us/support/by_product/system-1/), extract them, then choose `Preset library → Import folder`.

**Official PRM recall is a partial, 49-control CC preview.** It does not fully restore official sounds. MONO, FILTER TYPE, SUB TYPE, NOISE TYPE, COARSE and BEND RANGE are not converted from PRM. ARP / SCATTER and other unsupported fields are preserved in the original file only. Loading leaves those hardware values unchanged. Eight-bit PRM amounts are reduced to seven-bit MIDI amounts.

Library data lives in `%APPDATA%\System1Bridge\Library`. Save current as new stores known synth controls, not the USB gain, clock or MIDI routing. Loading a DAW project does not bulk-send controls; use `Send saved` explicitly.

## Limits and development

Live recording only; fast offline rendering outputs silence. Approximately 50 ms latency is reported to the host, not automatically measured. One instance owns the synth MIDI connection. SYSTEM-1m, PLUG-OUT and other operating systems are not supported. GUIDE graphs are illustrative; only the USB OUTPUT scope displays captured audio.

If capture is denied or silent, check the desktop for a pending audio permission prompt. See [Building](BUILDING.md), [Changelog](../CHANGELOG.md) and [validation notes](VALIDATION.md).

Licensed under AGPL-3.0-only. [Third-party notices](../THIRD-PARTY.md) apply. Each release includes complete source with the pinned JUCE dependency as a separate `Source-full.zip` asset.
