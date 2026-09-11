# v0.7.1 — first public prerelease

Windows x64 VST3 for capturing Roland SYSTEM-1 USB stereo audio while keeping the DAW on its usual audio interface. Includes USB MIDI editing, DAW note forwarding, tempo synchronization, an on-screen keyboard and preset library.

## Download

- **Windows-x64.zip**: VST3 bundle, capture helper, user guides and licenses. Copy both items inside `VST3` together; the helper belongs beside the bundle.
- **Source-full.zip**: matching project source, build scripts and the exact JUCE dependency used for this release.
- **SHA256SUMS.txt**: SHA-256 checksums for both ZIPs.

**No manual helper startup is required.** The VST automatically starts `System1Capture.exe` in the background when needed. Keep the executable beside the `.vst3` folder and enable the DAW audio engine and plugin; no helper window appears.

Requires a physical SYSTEM-1, its Roland Windows driver and a Windows x64 VST3 host. Tested with Bitwig Studio 6 and an RME Fireface UCX II. Binaries are unsigned. Source is AGPL-3.0-only.

## Notable fix

VOICE MODE now matches the hardware: Poly=CC119/0, Mono=64, Unison=127. Earlier private builds mislabeled these values. Saved raw values are preserved; an old preset labeled Poly may now correctly display Unison. Choose Poly again if needed.

## Preset and audio limits

Official PRM import is a **partial 49-control CC preview**, not complete hardware patch restoration. Official Volume 1–4 files were tested but are not included. Unconverted fields retain their current hardware values. Full PRM restoration, ARP / SCATTER control and PLUG-OUT support are not included.

Live recording only: fast offline rendering outputs silence. Approximately 50 ms latency is reported to the host. GUIDE graphs are illustrative; USB OUTPUT is the captured signal. See README and the user guide for setup and limitations.

## 日本語

SYSTEM-1のUSB音声を、普段のオーディオインターフェースを使うDAWへ取り込むVST3の初回公開版です。USB MIDI制御、DAWテンポ同期、鍵盤、プリセット管理を含みます。

Windows版ZIPを展開し、VST3バンドルとSystem1Capture.exeを同じフォルダーへ配置してください。VOICE MODEの表記ずれは修正済みです。公式PRMは49項目の部分対応で、音色の完全復元ではありません。公式バンクやドライバーは同梱していません。

**System1Capture.exeを先に手動で起動する必要はありません。** VSTが必要に応じてバックグラウンドで自動起動します。`.vst3` フォルダーと同じ階層へ配置し、DAWのオーディオエンジンとプラグインを有効にしてください。補助アプリのウィンドウは表示されません。音声アクセスに失敗する場合は、デスクトップに許可待ちウィンドウが出ていないかも確認してください。
