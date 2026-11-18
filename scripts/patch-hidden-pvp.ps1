param(
    [Parameter(Mandatory)] [string]$SourceLibrary,
    [Parameter(Mandatory)] [string]$DestinationLibrary
)

$ErrorActionPreference = 'Stop'

$expectedSourceHash = 'C0BF9F3ECBC22FF7F651845D6E8B06C34B83C109B5F2D841960661C2AEC2A667'
$showPvpRva = 0x30DAEBCL
$showCreditsRva = 0x30DB6D8L
$showCreditsOffset = 0x30D76D8L
$expectedPrologue = [byte[]](0xFE, 0x5F, 0xBD, 0xA9)

$sourcePath = (Resolve-Path -LiteralPath $SourceLibrary).Path
$sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
if ($sourceHash -ne $expectedSourceHash) {
    throw "libil2cpp.so inesperada. O patch so e aceito para a build verificada: $expectedSourceHash"
}
if (Test-Path -LiteralPath $DestinationLibrary) {
    throw "O destino ja existe e nao sera substituido: $DestinationLibrary"
}

$sourceBytes = [IO.File]::ReadAllBytes($sourcePath)
for ($index = 0; $index -lt $expectedPrologue.Length; $index++) {
    if ($sourceBytes[$showCreditsOffset + $index] -ne $expectedPrologue[$index]) {
        throw "Prologo inesperado em GeneralMenuScene.ShowCredits; patch cancelado."
    }
}

$branchImmediate = [int64](($showPvpRva - $showCreditsRva) / 4)
$branchInstruction = [uint32](0x14000000L -bor ($branchImmediate -band 0x03FFFFFFL))
$branchBytes = [BitConverter]::GetBytes($branchInstruction)
[Array]::Copy($branchBytes, 0, $sourceBytes, $showCreditsOffset, $branchBytes.Length)

$destinationDirectory = Split-Path -Parent $DestinationLibrary
if ($destinationDirectory) {
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
}
[IO.File]::WriteAllBytes($DestinationLibrary, $sourceBytes)

$patchedBytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $DestinationLibrary))
for ($index = 0; $index -lt $branchBytes.Length; $index++) {
    if ($patchedBytes[$showCreditsOffset + $index] -ne $branchBytes[$index]) {
        throw "A verificacao do branch ARM64 falhou."
    }
}

Write-Output ("GeneralMenuScene.ShowCredits redirecionado para ShowPvp: 0x{0:X8}" -f $branchInstruction)
Write-Output "Biblioteca modificada criada em: $DestinationLibrary"
