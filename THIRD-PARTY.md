# Third-party software and names

SYSTEM-1 Audio Bridge project source is licensed under AGPL-3.0-only; see LICENSE. Copyright (c) 2026 nanikasheila and contributors.

JUCE 9.0.2 is used under its AGPLv3 option at commit `72782788ce18c2d4d760b28e0921d6ffc6431102`. Copyright (c) Raw Material Software Limited. See `licenses/JUCE-LICENSE.md` and the upstream inventory in `licenses/JUCE.spdx.json`.

JUCE incorporates additional libraries, including the Steinberg VST3 SDK, image codecs, fonts/text shaping code and platform integration code. Their original notices remain in the complete JUCE source distributed in the full-source asset. The upstream SBOM lists all JUCE packages, including modules this project does not compile. The VST3 SDK license is also copied to `licenses/VST3-LICENSE.txt`.

The release targets explicitly disable ASIO. The DAW can use its own ASIO driver, but this project does not distribute Roland or RME drivers or a separate proprietary ASIO SDK.

Roland, SYSTEM-1, AIRA, PLUG-OUT, Elektron, Overbridge, Bitwig and other product names belong to their respective owners. Names describe compatibility only; this project is not endorsed by those companies. Official Roland sound banks, user recordings and DAW projects are not included.
