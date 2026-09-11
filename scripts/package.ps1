param(
    [string]$BuildDir = 'build',
    [Parameter(Mandatory = $true)][string]$JuceDir,
    [string]$OutputDir = 'dist'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot -Parent
function Resolve-FromRoot([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}
$buildRoot = Resolve-FromRoot $BuildDir
$juceRoot = Resolve-FromRoot $JuceDir
$outRoot = Resolve-FromRoot $OutputDir
$version = [regex]::Match((Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw), 'project\(System1Bridge VERSION ([0-9.]+)').Groups[1].Value
if (!$version) { throw 'Project version not found.' }
$pin = '72782788ce18c2d4d760b28e0921d6ffc6431102'
if (!(Test-Path -LiteralPath (Join-Path $juceRoot 'CMakeLists.txt'))) { throw 'JUCE source is missing.' }
$juceIsGit = Test-Path -LiteralPath (Join-Path $juceRoot '.git')
if ($juceIsGit) {
    $revision = (& git -C $juceRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $revision -ne $pin) { throw 'JUCE must match the pinned commit.' }
    $dirty = (& git -C $juceRoot status --porcelain)
    if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'JUCE checkout must be clean.' }
} else {
    $marker = Join-Path $juceRoot '.system1-revision'
    if (!(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne $pin) { throw 'Use the pinned JUCE Git checkout or the dependency from Source-full.zip.' }
}
$bundle = Join-Path $buildRoot 'System1Bridge_artefacts/Release/VST3/SYSTEM-1 Audio Bridge.vst3'
$helper = Join-Path $buildRoot 'System1Capture_artefacts/Release/System1Capture.exe'
$moduleInfo = Join-Path $bundle 'Contents/Resources/moduleinfo.json'
if (!(Test-Path -LiteralPath $helper) -or !(Test-Path -LiteralPath $moduleInfo)) { throw 'Release binaries are missing.' }
$info = Get-Content -LiteralPath $moduleInfo -Raw | ConvertFrom-Json
if ($info.Version -ne $version) { throw 'Built plugin version differs from project version.' }
New-Item -ItemType Directory -Path $outRoot -Force | Out-Null
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('system1-package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Add-File($Zip, [string]$File, [string]$Entry) {
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $File, $Entry.Replace('\','/'), [IO.Compression.CompressionLevel]::Optimal) | Out-Null
}
function Add-Tree($Zip, [string]$Folder, [string]$Prefix) {
    foreach ($file in Get-ChildItem -LiteralPath $Folder -Recurse -File -Force) {
        $relative = $file.FullName.Substring($Folder.Length).TrimStart('\','/')
        if ($relative -match '(^|[\\/])\.git([\\/]|$)') { continue }
        Add-File $Zip $file.FullName ($Prefix + '/' + $relative)
    }
}
$baseName = "SYSTEM-1-Audio-Bridge-v$version"
$binaryZip = Join-Path $tempRoot "$baseName-Windows-x64.zip"
$zip = [IO.Compression.ZipFile]::Open($binaryZip, [IO.Compression.ZipArchiveMode]::Create)
try {
    Add-Tree $zip $bundle 'VST3/SYSTEM-1 Audio Bridge.vst3'
    Add-File $zip $helper 'VST3/System1Capture.exe'
    foreach ($name in @('README.md','LICENSE','THIRD-PARTY.md','CHANGELOG.md')) { Add-File $zip (Join-Path $repoRoot $name) $name }
    Add-Tree $zip (Join-Path $repoRoot 'docs') 'docs'
    Add-Tree $zip (Join-Path $repoRoot 'licenses') 'licenses'
} finally { $zip.Dispose() }
$sourceZip = Join-Path $tempRoot "$baseName-Source-full.zip"
$zip = [IO.Compression.ZipFile]::Open($sourceZip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($name in @('CMakeLists.txt','README.md','LICENSE','THIRD-PARTY.md','CHANGELOG.md','.gitignore','.gitattributes')) { Add-File $zip (Join-Path $repoRoot $name) "$baseName/$name" }
    foreach ($folder in @('Source','docs','licenses','scripts','.github')) { Add-Tree $zip (Join-Path $repoRoot $folder) "$baseName/$folder" }
    if ($juceIsGit) {
        $upstreamZip = Join-Path $tempRoot 'juce.zip'
        & git -C $juceRoot archive --format=zip "--output=$upstreamZip" "--prefix=$baseName/dependencies/JUCE/" $pin
        if ($LASTEXITCODE -ne 0) { throw 'JUCE source archive failed.' }
        $upstream = [IO.Compression.ZipFile]::OpenRead($upstreamZip)
        try {
            foreach ($entry in $upstream.Entries) {
                $copy = $zip.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
                $inputStream = $entry.Open(); $outputStream = $copy.Open()
                try { $inputStream.CopyTo($outputStream) } finally { $inputStream.Dispose(); $outputStream.Dispose() }
            }
        } finally { $upstream.Dispose() }
        $marker = $zip.CreateEntry("$baseName/dependencies/JUCE/.system1-revision")
        $writer = New-Object IO.StreamWriter($marker.Open())
        try { $writer.WriteLine($pin) } finally { $writer.Dispose() }
    } else { Add-Tree $zip $juceRoot "$baseName/dependencies/JUCE" }
} finally { $zip.Dispose() }
foreach ($file in @($binaryZip,$sourceZip)) { Copy-Item -LiteralPath $file -Destination $outRoot -Force }
$checksums = foreach ($file in @($binaryZip,$sourceZip)) { (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetFileName($file) }
[IO.File]::WriteAllLines((Join-Path $outRoot 'SHA256SUMS.txt'), $checksums, [Text.UTF8Encoding]::new($false))
Write-Output "Packaged v$version in $outRoot"
Write-Output "Temporary source staging retained at $tempRoot"
