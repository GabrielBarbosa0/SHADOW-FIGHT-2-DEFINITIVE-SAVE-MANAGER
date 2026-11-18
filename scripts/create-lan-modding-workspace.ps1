param(
    [string]$SessionName = (Get-Date -Format 'yyyyMMdd-HHmmss')
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceDirectory = Join-Path $projectRoot 'game\Shadow Fight 2 DE'
$sourceApk = Join-Path $sourceDirectory 'Shadow Fight 2_ DE.apk'
$sourceSave = Join-Path $sourceDirectory 'users.xml'
$expectedApkHash = '09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA'
$expectedSaveHash = '124A6FD2701C9FA757B0C611ED7A347FDB411BDF26146AB5BF27BD8C3F0EEA20'

function Assert-FileHash {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$ExpectedHash,
        [Parameter(Mandatory)] [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label nao foi encontrado: $Path"
    }
    $actualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actualHash -ne $ExpectedHash) {
        throw "$Label possui hash inesperado. Esperado: $ExpectedHash. Encontrado: $actualHash"
    }
}

function Copy-VerifiedFile {
    param(
        [Parameter(Mandatory)] [string]$Source,
        [Parameter(Mandatory)] [string]$Destination,
        [Parameter(Mandatory)] [string]$ExpectedHash,
        [Parameter(Mandatory)] [string]$Label
    )

    if (Test-Path -LiteralPath $Destination) {
        throw "A copia de destino ja existe e nao sera substituida: $Destination"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination
    Assert-FileHash -Path $Destination -ExpectedHash $ExpectedHash -Label $Label
}

Assert-FileHash -Path $sourceApk -ExpectedHash $expectedApkHash -Label 'APK original'
Assert-FileHash -Path $sourceSave -ExpectedHash $expectedSaveHash -Label 'users.xml original'

$safetyRoot = Join-Path $projectRoot "game\safety-backups\$($expectedApkHash.Substring(0, 12))"
New-Item -ItemType Directory -Path $safetyRoot -Force | Out-Null
$safetyApk = Join-Path $safetyRoot 'Shadow Fight 2_ DE.original.apk'
$safetySave = Join-Path $safetyRoot 'users.original.xml'

if (-not (Test-Path -LiteralPath $safetyApk)) {
    Copy-VerifiedFile -Source $sourceApk -Destination $safetyApk -ExpectedHash $expectedApkHash -Label 'Backup do APK original'
} else {
    Assert-FileHash -Path $safetyApk -ExpectedHash $expectedApkHash -Label 'Backup do APK original'
}
if (-not (Test-Path -LiteralPath $safetySave)) {
    Copy-VerifiedFile -Source $sourceSave -Destination $safetySave -ExpectedHash $expectedSaveHash -Label 'Backup do users.xml original'
} else {
    Assert-FileHash -Path $safetySave -ExpectedHash $expectedSaveHash -Label 'Backup do users.xml original'
}

$workspace = Join-Path $projectRoot "game\lan-workspaces\$SessionName"
if (Test-Path -LiteralPath $workspace) {
    throw "A area de trabalho ja existe e nao sera substituida: $workspace"
}
New-Item -ItemType Directory -Path $workspace | Out-Null

$workApk = Join-Path $workspace 'Shadow Fight 2_ Definitive Edition LAN.work.apk'
$workSave = Join-Path $workspace 'users.work.xml'
Copy-VerifiedFile -Source $sourceApk -Destination $workApk -ExpectedHash $expectedApkHash -Label 'Copia de trabalho do APK'
Copy-VerifiedFile -Source $sourceSave -Destination $workSave -ExpectedHash $expectedSaveHash -Label 'Copia de trabalho do users.xml'

$manifest = [ordered]@{
    createdAt = (Get-Date).ToString('o')
    sourceApk = $sourceApk
    sourceApkSha256 = $expectedApkHash
    sourceSave = $sourceSave
    sourceSaveSha256 = $expectedSaveHash
    safetyBackup = $safetyRoot
    workspace = $workspace
    rule = 'Nunca modificar ou substituir os arquivos originais; toda alteracao ocorre nas copias de trabalho.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'ORIGIN-MANIFEST.json') -Encoding utf8

Write-Output "Backup de seguranca verificado: $safetyRoot"
Write-Output "Area de trabalho criada: $workspace"
