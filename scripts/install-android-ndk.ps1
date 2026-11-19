$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $repoRoot '.tools'
$archive = Join-Path $tools 'android-ndk-r30-windows.zip'
$ndkRoot = Join-Path $tools 'android-ndk-r30'
$clang = Join-Path $ndkRoot 'toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android24-clang.cmd'
$download = 'https://dl.google.com/android/repository/android-ndk-r30-windows.zip'
$expectedSha1 = '9bf167a1985fa7d4a036186b78f702eab9179408'

if (Test-Path -LiteralPath $clang) {
    Write-Output "Android NDK r30 ja esta disponivel em: $ndkRoot"
    exit 0
}

New-Item -ItemType Directory -Path $tools -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Write-Output 'Baixando o Android NDK r30 oficial...'
    & curl.exe -L --fail --retry 3 -o $archive $download
    if ($LASTEXITCODE -ne 0) { throw 'Download do Android NDK falhou.' }
}

$actualSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $archive).Hash.ToLowerInvariant()
if ($actualSha1 -ne $expectedSha1) {
    throw "O SHA-1 do NDK nao confere. Esperado: $expectedSha1; recebido: $actualSha1"
}

Write-Output 'Extraindo o Android NDK verificado...'
Expand-Archive -LiteralPath $archive -DestinationPath $tools -Force
if (-not (Test-Path -LiteralPath $clang)) {
    throw 'O compilador ARM64 nao foi encontrado depois da extracao.'
}
Write-Output "Android NDK r30 instalado em: $ndkRoot"
