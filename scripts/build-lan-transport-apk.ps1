param(
    [string]$SourceApk = "",
    [Parameter(Mandatory)] [string]$WorkingDirectory,
    [Parameter(Mandatory)] [string]$OutputApk,
    [switch]$KeepWorkingDirectory
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $SourceApk) {
    $SourceApk = Join-Path $repoRoot 'game\Shadow Fight 2 DE\Shadow Fight 2_ DE.apk'
}
$sourcePath = (Resolve-Path -LiteralPath $SourceApk).Path
$expectedApkHash = '09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash -ne $expectedApkHash) {
    throw "O APK de origem nao corresponde ao backup verificado: $expectedApkHash"
}

$workPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $WorkingDirectory))
$outputPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputApk))
$volumeRoot = [IO.Path]::GetPathRoot($workPath).TrimEnd('\')
if ($workPath.TrimEnd('\') -eq $volumeRoot -or $workPath.TrimEnd('\') -eq $repoRoot.TrimEnd('\')) {
    throw "WorkingDirectory deve ser uma subpasta dedicada: $workPath"
}
if ($outputPath.StartsWith($workPath.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputApk nao pode ficar dentro de WorkingDirectory, que e removida apos a build.'
}
if (Test-Path -LiteralPath $workPath) {
    throw "A area de trabalho ja existe e nao sera substituida: $workPath"
}
if (Test-Path -LiteralPath $outputPath) {
    throw "O APK de saida ja existe e nao sera substituido: $outputPath"
}

$apktool = Join-Path $repoRoot '.tools\apktool_3.0.3.jar'
$keystore = Join-Path $repoRoot '.tools\shadow-fight-lan-debug.keystore'
$androidSdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
$androidJar = Join-Path $androidSdk 'platforms\android-36.1\android.jar'
$buildTools = Join-Path $androidSdk 'build-tools\36.1.0'
$d8 = Join-Path $buildTools 'd8.bat'
$zipalign = Join-Path $buildTools 'zipalign.exe'
$apksigner = Join-Path $buildTools 'apksigner.bat'
$ndkRoot = Join-Path $repoRoot '.tools\android-ndk-r30'
$ndkClang = Join-Path $ndkRoot 'toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android24-clang.cmd'
foreach ($required in @($apktool, $keystore, $androidJar, $d8, $zipalign, $apksigner, $ndkClang)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Dependencia de build ausente: $required"
    }
}

$decoded = Join-Path $workPath 'apktool'
$javaClasses = Join-Path $workPath 'java-classes'
$dexOutput = Join-Path $workPath 'dex'
$miniStage = Join-Path $workPath 'mini-stage'
$bridgeDecoded = Join-Path $workPath 'bridge-decoded'
$bridgeJar = Join-Path $workPath 'sf2-lan-bridge.jar'
$miniApk = Join-Path $workPath 'sf2-lan-bridge-mini.apk'
$patchedLibrary = Join-Path $workPath 'libil2cpp.lan.so'
$nativeOutput = Join-Path $workPath 'libsf2lanhook.so'
$unsignedApk = Join-Path $workPath 'unsigned.apk'
$alignedApk = Join-Path $workPath 'aligned.apk'
$workCreated = $false
try {
New-Item -ItemType Directory -Path $workPath, $javaClasses, $dexOutput, $miniStage | Out-Null
$workCreated = $true

Write-Output 'Decodificando uma nova copia do APK original...'
& java -jar $apktool d $sourcePath -o $decoded
if ($LASTEXITCODE -ne 0) { throw 'apktool decode falhou.' }

Write-Output 'Compilando a ponte UDP Android...'
$javaSources = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'lan_bridge\android\src') -Filter '*.java' -Recurse |
    ForEach-Object FullName
& javac --release 8 -encoding UTF-8 -classpath $androidJar -d $javaClasses $javaSources
if ($LASTEXITCODE -ne 0) { throw 'javac falhou.' }
& jar --create --file $bridgeJar -C $javaClasses .
if ($LASTEXITCODE -ne 0) { throw 'A criacao do JAR da ponte LAN falhou.' }
& $d8 --release --min-api 24 --lib $androidJar --output $dexOutput $bridgeJar
if ($LASTEXITCODE -ne 0) { throw 'd8 falhou.' }

Write-Output 'Convertendo a ponte para smali isolado...'
Push-Location $miniStage
try {
    & jar xf $sourcePath AndroidManifest.xml
    Copy-Item -LiteralPath (Join-Path $dexOutput 'classes.dex') -Destination 'classes.dex'
    & jar --create --file $miniApk AndroidManifest.xml classes.dex
    if ($LASTEXITCODE -ne 0) { throw 'A criacao do APK auxiliar falhou.' }
} finally {
    Pop-Location
}
& java -jar $apktool d -r $miniApk -o $bridgeDecoded
if ($LASTEXITCODE -ne 0) { throw 'A conversao da ponte para smali falhou.' }
$smaliTarget = Join-Path $decoded 'smali_classes8'
New-Item -ItemType Directory -Path $smaliTarget | Out-Null
Copy-Item -LiteralPath (Join-Path $bridgeDecoded 'smali\com') -Destination $smaliTarget -Recurse

Write-Output 'Compilando o hook ARM64 do GamePad 1...'
& $ndkClang -std=c11 -O2 -fPIC -shared -D_GNU_SOURCE `
    '-Wl,--no-undefined' '-Wl,-z,max-page-size=16384' `
    -o $nativeOutput `
    (Join-Path $repoRoot 'lan_bridge\android\native\sf2_lan_hook.c') `
    -llog -ldl
if ($LASTEXITCODE -ne 0) { throw 'A compilacao do hook ARM64 falhou.' }
$nativeTarget = Join-Path $decoded 'lib\arm64-v8a\libsf2lanhook.so'
Copy-Item -LiteralPath $nativeOutput -Destination $nativeTarget

Write-Output 'Adicionando bootstrap e permissoes LAN ao manifesto da copia...'
$manifestPath = Join-Path $decoded 'AndroidManifest.xml'
[xml]$manifestDocument = Get-Content -Raw -LiteralPath $manifestPath
$androidNamespace = 'http://schemas.android.com/apk/res/android'
$manifest = $manifestDocument.DocumentElement
$application = $manifest.SelectSingleNode('application')
if (-not $application) { throw 'Elemento application ausente no manifesto decodificado.' }

$permissionNames = @(
    'android.permission.INTERNET',
    'android.permission.ACCESS_NETWORK_STATE',
    'android.permission.ACCESS_WIFI_STATE',
    'android.permission.CHANGE_WIFI_MULTICAST_STATE'
)
foreach ($permissionName in $permissionNames) {
    $alreadyPresent = $false
    foreach ($permission in $manifest.SelectNodes('uses-permission')) {
        if ($permission.GetAttribute('name', $androidNamespace) -eq $permissionName) {
            $alreadyPresent = $true
            break
        }
    }
    if (-not $alreadyPresent) {
        $permission = $manifestDocument.CreateElement('uses-permission')
        [void]$permission.SetAttribute('name', $androidNamespace, $permissionName)
        [void]$manifest.InsertBefore($permission, $application)
    }
}

$provider = $manifestDocument.CreateElement('provider')
[void]$provider.SetAttribute('name', $androidNamespace, 'com.sf2.lan.LanBootstrapProvider')
[void]$provider.SetAttribute('authorities', $androidNamespace, 'com.sf2.de.sf2lan.bootstrap')
[void]$provider.SetAttribute('exported', $androidNamespace, 'false')
[void]$provider.SetAttribute('initOrder', $androidNamespace, '1000')
[void]$application.AppendChild($provider)

$xmlSettings = [Xml.XmlWriterSettings]::new()
$xmlSettings.Encoding = [Text.UTF8Encoding]::new($false)
$xmlSettings.Indent = $true
$writer = [Xml.XmlWriter]::Create($manifestPath, $xmlSettings)
try {
    $manifestDocument.Save($writer)
} finally {
    $writer.Dispose()
}

Write-Output 'Aplicando o acesso ao PVP oculto em outra biblioteca...'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'patch-hidden-pvp.ps1') `
    -SourceLibrary (Join-Path $decoded 'lib\arm64-v8a\libil2cpp.so') `
    -DestinationLibrary $patchedLibrary
if ($LASTEXITCODE -ne 0) { throw 'O patch do PVP falhou.' }
Copy-Item -LiteralPath $patchedLibrary -Destination (Join-Path $decoded 'lib\arm64-v8a\libil2cpp.so')

Write-Output 'Recompilando e assinando o APK de transporte LAN...'
& java -jar $apktool b $decoded -o $unsignedApk
if ($LASTEXITCODE -ne 0) { throw 'apktool build falhou.' }
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

$resultHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
Write-Output "APK LAN criado sem substituir os originais: $outputPath"
Write-Output "SHA-256: $resultHash"
} finally {
    if ($workCreated -and -not $KeepWorkingDirectory -and (Test-Path -LiteralPath $workPath)) {
        Write-Output "Removendo intermediarios da build: $workPath"
        Remove-Item -LiteralPath $workPath -Recurse -Force
    } elseif ($workCreated -and $KeepWorkingDirectory) {
        Write-Output "Intermediarios mantidos para diagnostico: $workPath"
    }
}
