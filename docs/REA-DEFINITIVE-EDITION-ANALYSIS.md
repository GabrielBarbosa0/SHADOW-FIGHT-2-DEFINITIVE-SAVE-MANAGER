# Analise da Definitive Edition com REA

## Escopo

Esta analise usa exclusivamente o APK Android de **Shadow Fight 2: Definitive
Edition 2.41.9** (`com.sf2.de`). O SF2Recomp nao e entrada de build, dependencia
nem destino do projeto. Ele pode ser consultado apenas como referencia de arquitetura
de rede.

O APK e o `users.xml` originais permanecem imutaveis. O REA recebe apenas copias do
`global-metadata.dat` e da `libil2cpp.so`, extraidas em `build/`, que e ignorada pelo
Git.

## Ferramenta e artefatos verificados

- REA oficial: <https://github.com/morluto/rea>
- versao usada nesta etapa: `6.9.0`;
- metadata IL2CPP: versao `31`;
- SHA-256 do APK: `09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA`;
- SHA-256 do metadata: `2BD627B8E4170F7A30FF596CA2FDA7C75A4E1EDB2A5D4774C1B105CCC0FFCAD2`;
- SHA-256 da `libil2cpp.so`: `C0BF9F3ECBC22FF7F651845D6E8B06C34B83C109B5F2D841960661C2AEC2A667`.

O inventario metadata-only encontrou:

| Item | Quantidade |
|---|---:|
| assemblies/images | 131 |
| tipos | 15.934 |
| metodos | 105.889 |
| campos | 69.335 |
| propriedades | 17.947 |
| eventos | 600 |
| parametros | 102.946 |
| literais de string | 22.784 |

## Evidencias do PVP local legado

O REA confirmou a classe `Nekki.SF2.GUI.Pvp.PvpScene` no indice de tipo `2331`,
com 12 campos, 6 propriedades e 27 metodos. Entre os nomes preservados estao:

- `get_PlayerOneInput`;
- `get_PlayerTwoInput`;
- `SwitchPlayerOneInput`;
- `SwitchPlayerTwoInput`;
- `get_PlayerMenu`;
- `get_HintController`;
- `get_SceneId`.

Tambem foram confirmados componentes que formavam o fluxo completo do modo:

- `CharacterSelectModule`, `LocationSelectModule` e `PlayersSelectTable`;
- `PvpVsScreen` e `EndPvpFightScreen`;
- `PreFight.SetPlayerPvpMode`;
- `GameController`, alem de controles separados no namespace `InControl`;
- literais como `PVP`, `PvP_ChooseCharacter`, `PvP_ChooseLocation`,
  `PvPLeftHintList`, `PvPRightHintList` e `PvpBattle|pvp|1`.

Essas observacoes sao independentes do Il2CppDumper usado nas primeiras etapas e
confirmam que a logica de dois jogadores ainda existe no binario Android.

## Cena ausente e novo caminho

O APK contem somente `assets/bin/Data/level0` a `level7`. Nao existe `level9` nem
um asset chamado `ModulePvp`. O carregamento forcado feito no prototipo 19 ja havia
confirmado em execucao que o ID do modulo aponta para uma cena nao adicionada ao build.

Portanto, chamar `PvpScene` diretamente nao basta. O caminho atual e:

1. mapear `FightScene.CreateFight`, `FightScene.StartFight` e `PreFight.CreateFight`;
2. identificar como uma cena incluida instancia os dois modelos de lutador;
3. reaproveitar uma cena de luta existente como recipiente visual;
4. ativar o modo PVP de `PreFight` e ligar `PlayerOneInput`/`PlayerTwoInput`;
5. somente depois conectar os perfis e a ponte LAN ja implementada;
6. gerar sempre outro APK e testar primeiro no MuMu.

O proximo marco de viabilidade nao e um novo menu: e demonstrar dois lutadores
controlaveis de forma independente dentro de uma cena que ja existe no APK.

## Limite atual do REA

O REA 6.9.0 analisou corretamente o metadata v31. A vinculacao automatica entre o
metadata e esta `libil2cpp.so`, porem, foi recusada com seguranca porque o carregador
encontrou mais de um candidato possivel para um registro de modulo. Por isso, esta
etapa nao atribui enderecos nativos novos nem afirma ter decompilado os corpos ARM64.

Para a proxima etapa, os nomes e indices confirmados pelo REA serao combinados com o
mapa de enderecos ja produzido pelo Il2CppDumper e, quando necessario, com um
descompilador ARM64. Qualquer endereco usado em patch continuara limitado pelos hashes
exatos acima.

O primeiro cruzamento reproduzido encontrou estes RVAs na biblioteca verificada:

| Tipo | Metodo | RVA |
|---|---|---:|
| `PvpScene` | `get_PlayerTwoInput` | `0x32E239C` |
| `PvpScene` | `SwitchPlayerOneInput` | `0x32E2B50` |
| `PvpScene` | `get_PlayerOneInput` | `0x32E2F90` |
| `PvpScene` | `SwitchPlayerTwoInput` | `0x32E3A00` |
| `PvpScene` | `get_SceneId` | `0x32E4260` |
| `PreFight` | `SetPlayerPvpMode` | `0x31E2C28` |
| `PreFight` | `CreateFight` | `0x31E4A0C` |
| `FightScene` | `CreateFight` | `0x3234A0C` |
| `FightScene` | `StartFight` | `0x3234BB8` |

A inspecao ARM64 de `FightScene.CreateFight` tambem confirmou que a cena cria um
`PreFight` e entrega ao construtor interno da luta quatro entradas: configuracao da
batalha, lutador principal, lista de adversarios e o proprio `PreFight`. Esse e o ponto
mais promissor para substituir o adversario controlado pela IA por um segundo perfil
humano sem precisar reconstruir todo o ambiente visual.

## Reproduzir a analise

Instale o REA oficial e use Node.js. Depois, informe os dois caminhos sem copiar as
ferramentas para o repositorio:

```powershell
$env:SF2_REA_NODE = 'C:\caminho\para\node.exe'
$env:SF2_REA_CLI = 'C:\caminho\para\rea.mjs'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\analyze-definitive-edition-with-rea.ps1
```

O script:

1. valida o hash do APK original;
2. cria uma sessao nova, sem sobrescrever resultados existentes;
3. extrai somente copias do metadata e do binario ARM64;
4. valida os hashes das copias;
5. executa a analise metadata-only do REA;
6. gera um inventario JSON e um relatorio Markdown em `build/`.
