param(
    [string]$KeepApk = "",
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'

$componentRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $componentRoot)).Path
$distRoot = Join-Path $repoRoot 'dist'
$backupRoot = Join-Path $repoRoot 'backups'
$deviceBackupRoot = Join-Path $repoRoot 'game\device-backups'

if ($KeepApk) {
    $keepPath = (Resolve-Path -LiteralPath $KeepApk).Path
} else {
    $latest = Get-ChildItem -LiteralPath $distRoot -File -Filter '*.apk' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    $keepPath = if ($latest) { $latest.FullName } else { '' }
}

if ($keepPath -and -not $keepPath.StartsWith($distRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "KeepApk precisa apontar para um APK dentro de dist: $keepPath"
}

$candidates = @()
if (Test-Path -LiteralPath $distRoot) {
    $candidates += Get-ChildItem -LiteralPath $distRoot -File -Filter '*.apk' |
        Where-Object { -not $keepPath -or $_.FullName -ne $keepPath }
}
foreach ($root in @($backupRoot, $deviceBackupRoot)) {
    if (Test-Path -LiteralPath $root) {
        $candidates += Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.apk'
    }
}

$validated = foreach ($file in $candidates) {
    $path = $file.FullName
    $allowed = $path.StartsWith($distRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $path.StartsWith($backupRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $path.StartsWith($deviceBackupRoot + '\', [StringComparison]::OrdinalIgnoreCase)
    if (-not $allowed) {
        throw "A limpeza recusou um caminho fora das pastas permitidas: $path"
    }
    $file
}

$bytes = ($validated | Measure-Object -Property Length -Sum).Sum
$gib = [Math]::Round($bytes / 1GB, 3)
Write-Output "APK preservado: $(if ($keepPath) { $keepPath } else { 'nenhum APK em dist' })"
Write-Output "Candidatos: $($validated.Count) APK(s), $gib GiB"

if (-not $Apply) {
    Write-Output 'Simulacao concluida. Use -Apply para remover somente os APKs listados pelas regras acima.'
    exit 0
}

foreach ($file in $validated) {
    Remove-Item -LiteralPath $file.FullName -Force
}

Write-Output "Limpeza concluida: $($validated.Count) APK(s) removidos, $gib GiB liberados."
Write-Output 'Saves, XMLs, APK original e game\safety-backups nao sao alterados por este script.'
