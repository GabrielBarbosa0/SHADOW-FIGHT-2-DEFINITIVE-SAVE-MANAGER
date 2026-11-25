param(
    [Parameter(Mandatory)] [string]$BaseApk,
    [Parameter(Mandatory)] [string]$WorkingDirectory,
    [Parameter(Mandatory)] [string]$OutputApk,
    [switch]$AllowRemoteAiForDiagnostics,
    [switch]$TraceAllFighterFloats
)

$ErrorActionPreference = 'Stop'

$componentRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $componentRoot
$basePath = (Resolve-Path -LiteralPath $BaseApk).Path
$baseHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $basePath).Hash
$workPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $WorkingDirectory))
$outputPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputApk))
$volumeRoot = [IO.Path]::GetPathRoot($workPath).TrimEnd('\')
if ($workPath.TrimEnd('\') -eq $volumeRoot -or $workPath.TrimEnd('\') -eq $repoRoot.TrimEnd('\')) {
    throw "WorkingDirectory deve ser uma subpasta dedicada: $workPath"
}
if ($outputPath.StartsWith($workPath.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputApk nao pode ficar dentro de WorkingDirectory.'
}
if (Test-Path -LiteralPath $workPath) {
    throw "A area de trabalho ja existe e nao sera substituida: $workPath"
}
if (Test-Path -LiteralPath $outputPath) {
    throw "O APK de saida ja existe e nao sera substituido: $outputPath"
}

$ndkClang = Join-Path $repoRoot '.tools\android-ndk-r30\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android24-clang.cmd'
$keystore = Join-Path $repoRoot '.tools\shadow-fight-lan-debug.keystore'
$buildTools = Join-Path $env:LOCALAPPDATA 'Android\Sdk\build-tools\36.1.0'
$zipalign = Join-Path $buildTools 'zipalign.exe'
$apksigner = Join-Path $buildTools 'apksigner.bat'
foreach ($required in @($ndkClang, $keystore, $zipalign, $apksigner)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Dependencia de build ausente: $required"
    }
}

$stage = Join-Path $workPath 'stage'
$nativeDirectory = Join-Path $stage 'lib\arm64-v8a'
$nativeOutput = Join-Path $nativeDirectory 'libsf2lanhook.so'
$unsignedApk = Join-Path $workPath 'native-updated-unsigned.apk'
$alignedApk = Join-Path $workPath 'native-updated-aligned.apk'
$workCreated = $false
try {
    New-Item -ItemType Directory -Path $nativeDirectory | Out-Null
    $workCreated = $true

    Write-Output 'Compilando somente a instrumentacao ARM64 atualizada...'
$diagnosticDefines = @()
if ($AllowRemoteAiForDiagnostics) {
    $diagnosticDefines += '-DSF2_LAN_DIAGNOSTIC_ALLOW_REMOTE_AI=1'
    Write-Warning 'Build de diagnostico: a IA do adversario LAN ficara ativa.'
}
if ($TraceAllFighterFloats) {
    $diagnosticDefines += '-DSF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS=1'
    Write-Warning 'Build de diagnostico: os 16 getters float do lutador serao rastreados no logcat.'
}

    & $ndkClang -std=c11 -O2 -fPIC -shared -D_GNU_SOURCE @diagnosticDefines `
        '-Wl,--no-undefined' '-Wl,-z,max-page-size=16384' `
        -o $nativeOutput `
        (Join-Path $componentRoot 'lan_bridge\android\native\sf2_lan_hook.c') `
        -llog -ldl
    if ($LASTEXITCODE -ne 0) { throw 'A compilacao do hook ARM64 falhou.' }

    Write-Output 'Criando uma nova copia do APK-base e substituindo apenas libsf2lanhook.so...'
    Copy-Item -LiteralPath $basePath -Destination $unsignedApk
    & jar --update --file $unsignedApk -C $stage 'lib/arm64-v8a/libsf2lanhook.so'
    if ($LASTEXITCODE -ne 0) { throw 'A atualizacao incremental do APK falhou.' }

    & $zipalign -p -f 4 $unsignedApk $alignedApk
    if ($LASTEXITCODE -ne 0) { throw 'zipalign falhou.' }
    $outputParent = Split-Path -Parent $outputPath
    if ($outputParent) { New-Item -ItemType Directory -Path $outputParent -Force | Out-Null }
    & $apksigner sign --ks $keystore --ks-key-alias sf2lan --ks-pass pass:android --key-pass pass:android `
        --v4-signing-enabled false `
        --out $outputPath $alignedApk
    if ($LASTEXITCODE -ne 0) { throw 'apksigner falhou.' }
    & $apksigner verify --verbose $outputPath
    if ($LASTEXITCODE -ne 0) { throw 'A verificacao da assinatura falhou.' }

    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $basePath).Hash -ne $baseHash) {
        throw 'O APK-base foi alterado durante o processo incremental.'
    }
    $resultHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
    Write-Output "APK incremental criado sem substituir o APK-base: $outputPath"
    Write-Output "SHA-256: $resultHash"
} finally {
    if ($workCreated -and (Test-Path -LiteralPath $workPath)) {
        Write-Output "Removendo intermediarios da build incremental: $workPath"
        Remove-Item -LiteralPath $workPath -Recurse -Force
    }
}
