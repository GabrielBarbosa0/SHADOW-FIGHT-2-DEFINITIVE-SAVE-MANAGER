param(
    [string]$SessionName = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [string]$NodePath = $env:SF2_REA_NODE,
    [string]$ReaCliPath = $env:SF2_REA_CLI
)

$ErrorActionPreference = 'Stop'

$componentRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $componentRoot
$sourceApk = Join-Path $projectRoot 'game\Shadow Fight 2 DE\Shadow Fight 2_ DE.apk'
$expectedApkHash = '09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA'
$expectedMetadataHash = '2BD627B8E4170F7A30FF596CA2FDA7C75A4E1EDB2A5D4774C1B105CCC0FFCAD2'
$expectedBinaryHash = 'C0BF9F3ECBC22FF7F651845D6E8B06C34B83C109B5F2D841960661C2AEC2A667'

function Resolve-NodePath {
    if ($NodePath) {
        return (Resolve-Path -LiteralPath $NodePath).Path
    }

    $node = Get-Command node -ErrorAction SilentlyContinue
    if ($node) {
        return $node.Source
    }

    throw 'Node.js nao foi encontrado. Informe -NodePath ou defina SF2_REA_NODE.'
}

function Resolve-ReaCliPath {
    if ($ReaCliPath) {
        return (Resolve-Path -LiteralPath $ReaCliPath).Path
    }

    throw 'O CLI do REA nao foi encontrado. Informe -ReaCliPath ou defina SF2_REA_CLI.'
}

function Assert-Hash {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Expected,
        [Parameter(Mandatory)] [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label nao encontrado: $Path"
    }

    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actual -ne $Expected) {
        throw "$Label possui hash inesperado. Esperado: $Expected. Encontrado: $actual"
    }
}

function Expand-ApkEntry {
    param(
        [Parameter(Mandatory)] [System.IO.Compression.ZipArchive]$Archive,
        [Parameter(Mandatory)] [string]$EntryName,
        [Parameter(Mandatory)] [string]$Destination
    )

    $entry = $Archive.GetEntry($EntryName)
    if (-not $entry) {
        throw "Entrada ausente no APK: $EntryName"
    }

    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $Destination, $false)
}

function Get-TypeInventory {
    param(
        [Parameter(Mandatory)] $Result,
        [Parameter(Mandatory)] [string]$Namespace,
        [Parameter(Mandatory)] [string]$Name,
        [Parameter(Mandatory)] [hashtable]$ParameterLookup,
        [Parameter(Mandatory)] [hashtable]$FieldLookup,
        [Parameter(Mandatory)] [hashtable]$PropertyLookup
    )

    $typeIndex = -1
    for ($index = 0; $index -lt $Result.types.Count; $index++) {
        $candidate = $Result.types[$index]
        if ($candidate.namespace -eq $Namespace -and $candidate.name -eq $Name) {
            $typeIndex = $index
            break
        }
    }
    if ($typeIndex -lt 0) {
        return $null
    }

    $type = $Result.types[$typeIndex]
    $methods = for ($index = $type.method_start; $index -lt ($type.method_start + $type.method_count); $index++) {
        $method = $Result.methods[$index]
        $parameters = if ($ParameterLookup.ContainsKey($index)) { @($ParameterLookup[$index]) } else { @() }
        [ordered]@{
            index = $index
            name = $method.name
            parameters = $parameters
        }
    }
    $fields = if ($FieldLookup.ContainsKey($typeIndex)) { @($FieldLookup[$typeIndex]) } else { @() }
    $properties = if ($PropertyLookup.ContainsKey($typeIndex)) { @($PropertyLookup[$typeIndex]) } else { @() }

    [ordered]@{
        index = $typeIndex
        fullName = "$Namespace.$Name"
        fields = @($fields)
        properties = @($properties)
        methods = @($methods)
    }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
Assert-Hash -Path $sourceApk -Expected $expectedApkHash -Label 'APK original'

$resolvedNode = Resolve-NodePath
$resolvedRea = Resolve-ReaCliPath
$outputRoot = Join-Path $projectRoot "build\rea-definitive-edition\$SessionName"
if (Test-Path -LiteralPath $outputRoot) {
    throw "A sessao ja existe e nao sera substituida: $outputRoot"
}
New-Item -ItemType Directory -Path $outputRoot | Out-Null

$metadataPath = Join-Path $outputRoot 'assets\bin\Data\Managed\Metadata\global-metadata.dat'
$binaryPath = Join-Path $outputRoot 'lib\arm64-v8a\libil2cpp.so'
$archive = [System.IO.Compression.ZipFile]::OpenRead($sourceApk)
try {
    Expand-ApkEntry -Archive $archive -EntryName 'assets/bin/Data/Managed/Metadata/global-metadata.dat' -Destination $metadataPath
    Expand-ApkEntry -Archive $archive -EntryName 'lib/arm64-v8a/libil2cpp.so' -Destination $binaryPath
} finally {
    $archive.Dispose()
}

Assert-Hash -Path $metadataPath -Expected $expectedMetadataHash -Label 'Metadata IL2CPP extraido'
Assert-Hash -Path $binaryPath -Expected $expectedBinaryHash -Label 'Binario IL2CPP extraido'

$reaVersion = (& $resolvedNode $resolvedRea --version 2>&1 | Out-String).Trim()
$evidencePath = Join-Path $outputRoot 'metadata-evidence.json'
$summaryOutput = Join-Path $outputRoot 'rea-summary-output.json'
$reaOutput = & $resolvedNode $resolvedRea inspect-il2cpp-metadata $metadataPath `
    --detail summary `
    --evidence_output $evidencePath `
    --format json 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "REA falhou ao analisar o metadata: $($reaOutput | Out-String)"
}
$reaOutput | Set-Content -LiteralPath $summaryOutput -Encoding utf8

$bundle = Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
$record = $bundle.records[0]
$result = $record.normalized_result

$parameterLookup = @{}
foreach ($parameter in $result.parameters) {
    $methodIndex = [int]$parameter.method
    if (-not $parameterLookup.ContainsKey($methodIndex)) {
        $parameterLookup[$methodIndex] = [System.Collections.Generic.List[string]]::new()
    }
    $parameterLookup[$methodIndex].Add([string]$parameter.name)
}
$fieldLookup = @{}
for ($index = 0; $index -lt $result.fields.Count; $index++) {
    $field = $result.fields[$index]
    $typeIndex = [int]$field.declaring_type
    if (-not $fieldLookup.ContainsKey($typeIndex)) {
        $fieldLookup[$typeIndex] = [System.Collections.Generic.List[object]]::new()
    }
    $fieldLookup[$typeIndex].Add([ordered]@{ index = $index; name = $field.name })
}
$propertyLookup = @{}
for ($index = 0; $index -lt $result.properties.Count; $index++) {
    $property = $result.properties[$index]
    $typeIndex = [int]$property.declaring_type
    if (-not $propertyLookup.ContainsKey($typeIndex)) {
        $propertyLookup[$typeIndex] = [System.Collections.Generic.List[object]]::new()
    }
    $propertyLookup[$typeIndex].Add([ordered]@{ index = $index; name = $property.name })
}
$targets = @(
    @('Nekki.SF2.GUI.Pvp', 'PvpScene'),
    @('Nekki.SF2.GUI.Fight', 'FightScene'),
    @('Nekki.SF2.GUI.Scenes', 'DojoScene'),
    @('Nekki.SF2.GUI.Fight', 'PreFight'),
    @('Nekki.SF2.Core.Fights.Controls', 'GameController'),
    @('Nekki.SF2.GUI.Fight', 'PvpVsScreen'),
    @('Nekki.SF2.GUI.Fight', 'EndPvpFightScreen')
)
$inventories = foreach ($target in $targets) {
    Get-TypeInventory -Result $result -Namespace $target[0] -Name $target[1] `
        -ParameterLookup $parameterLookup -FieldLookup $fieldLookup -PropertyLookup $propertyLookup
}
$inventories = @($inventories | Where-Object { $null -ne $_ })

$pvpStrings = @()
for ($index = 0; $index -lt $result.string_literals.Count; $index++) {
    $value = $result.string_literals[$index].value
    if ($value -match '(?i)pvp|modulepvp|player.?one|player.?two') {
        $pvpStrings += [ordered]@{ index = $index; value = $value }
    }
}

$analysis = [ordered]@{
    generatedAt = (Get-Date).ToString('o')
    reaVersion = $reaVersion
    source = [ordered]@{
        apk = $sourceApk
        apkSha256 = $expectedApkHash
        metadataSha256 = $expectedMetadataHash
        binarySha256 = $expectedBinaryHash
    }
    metadata = [ordered]@{
        version = $result.metadata_version
        images = $result.images.Count
        types = $result.types.Count
        methods = $result.methods.Count
        fields = $result.fields.Count
        properties = $result.properties.Count
        events = $result.events.Count
        parameters = $result.parameters.Count
        stringLiterals = $result.string_literals.Count
        linkedMethodAddresses = $result.method_addresses.Count
    }
    types = $inventories
    pvpStrings = $pvpStrings
    limitation = 'A analise metadata-only confirma nomes e proprietarios, mas nao recupera corpos de funcoes nem tipos de campos/parametros.'
}
$analysisPath = Join-Path $outputRoot 'definitive-edition-analysis.json'
$analysis | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $analysisPath -Encoding utf8

$pvpScene = $inventories | Where-Object { $_.fullName -eq 'Nekki.SF2.GUI.Pvp.PvpScene' }
$report = @(
    '# Relatorio REA - Shadow Fight 2: Definitive Edition',
    '',
    "- REA: $reaVersion",
    "- Metadata IL2CPP: v$($result.metadata_version)",
    "- Tipos: $($result.types.Count)",
    "- Metodos: $($result.methods.Count)",
    "- Campos: $($result.fields.Count)",
    "- Literais de string: $($result.string_literals.Count)",
    "- Enderecos nativos vinculados nesta execucao: $($result.method_addresses.Count)",
    '',
    '## PvpScene',
    '',
    "A classe foi encontrada com $($pvpScene.fields.Count) campos, $($pvpScene.properties.Count) propriedades e $($pvpScene.methods.Count) metodos.",
    '',
    'Metodos de controle identificaveis:',
    ''
)
foreach ($methodName in @('get_PlayerOneInput', 'get_PlayerTwoInput', 'SwitchPlayerOneInput', 'SwitchPlayerTwoInput', 'get_PlayerMenu', 'get_SceneId')) {
    $method = $pvpScene.methods | Where-Object { $_.name -eq $methodName }
    if ($method) {
        $report += "- ``$methodName`` (indice $($method.index))"
    }
}
$report += @(
    '',
    '## Limite observado',
    '',
    'O REA confirma a infraestrutura PVP no metadata. Ele nao recria a cena Unity ausente nem decompila sozinho os corpos ARM64.',
    'O proximo marco e rastrear a criacao dos dois lutadores em FightScene/PreFight e adaptar esse fluxo a uma cena empacotada.',
    '',
    'Os arquivos originais nao foram alterados. Esta sessao contem apenas copias extraidas e relatorios ignorados pelo Git.'
)
$reportPath = Join-Path $outputRoot 'REPORT.md'
$report | Set-Content -LiteralPath $reportPath -Encoding utf8

Write-Output "REA $reaVersion concluiu a analise metadata-only."
Write-Output "Relatorio: $reportPath"
Write-Output "Inventario JSON: $analysisPath"
Write-Output "Evidence completa: $evidencePath"
