# Checkpoint p146 — equipamento canônico do duelo LAN

## Escopo

Este checkpoint trata somente da identidade e do equipamento dos dois Fighters
nas duas instâncias. Não inclui sincronização de dano, vida, rounds, interface
ou reconciliação de rede.

Preset temporário aplicado aos quatro avatares:

- nome `Shadow` e retrato `avatar_hero`;
- esqueleto `Skeleton`;
- armadura `Body` e capacete `Head`;
- arma `WEAPON_NINJA_SWORD`;
- `NoRanged` e `NoMagic`;
- listas de encantamentos da arma, armadura e capacete vazias;
- nenhum perfil ou equipamento de boss.

## Causa da divergência

Os setters dos seis slots do `EFPINIIBPLJ` (`PlayerProfile`) atualizavam os
campos de equipamento, mas não reconstruíam `NLNHPPKKGMM` em `Profile+0x28`.
Essa lista de strings é consumida por `HPDIKFPFIEI.FCDDEMCCPGM` ao criar o
grafo físico do Fighter. Assim, o campo da arma podia apontar para a Espada
Ninja e produzir a pose/moveset de katana, enquanto o modelo continuava sem
lâmina ou carregava a arma sorteada anteriormente.

A tentativa tardia de reaplicar a arma por `EFEBLAILCLI` não corrigia isso: o
método compara os IDs e retorna imediatamente quando a arma do Profile já é a
mesma. O log antigo que dizia que a malha havia sido reaplicada era, portanto,
um falso positivo e foi removido.

A correção p146 chama `EFPINIIBPLJ.ICAGHFBEELJ` depois de gravar os seis slots.
Isso recria `Profile+0x28` antes de Viewer, Fighter, controlador de animação e
configuração dos ataques consumirem o perfil.

## Mapa IL2CPP confirmado

### Profile (`EFPINIIBPLJ`)

| RVA/offset | Uso |
| --- | --- |
| `+0x28` | `List<string> NLNHPPKKGMM`, recursos físicos do modelo |
| `+0x30`, `+0x38` | listas de equipamento auxiliares |
| `+0x68` | flag de controle por IA |
| `+0xB8..+0xE0` | Skeleton, Weapon, Armor, Helm, Ranged e Magic |
| `+0x188`, `+0x198` | nome e retrato |
| `+0x1B0`, `+0x1B8`, `+0x1C0` | encantamentos de arma, armadura e capacete |
| `0x3169DC4` | construtor de cópia do Profile |
| `0x316F390` / `0x316F674` | resolução de item por slot |
| `0x316F7E8` | setter virtual do equipamento |
| `0x316FFA8` | reconstrução da lista de recursos em `+0x28` |

### FightConfig, Fighter, Viewer e ataques

| RVA/offset | Uso |
| --- | --- |
| `0x3121DCC` | regra de equipamento do FightConfig; anulada no LAN |
| `0x33987C0` | construtor do Fighter |
| `0x3398108` | construção do modelo físico a partir de `Profile+0x28` |
| `0x3397218` | aplicação de item sorteado do FightConfig; bloqueada no LAN |
| `0x339BA2C` | troca tardia de equipamento; no-op se o ID não mudou |
| `Fighter+0x148` | Profile efetivo |
| `Fighter+0x248` | cache `HEJIMFJIGDE` de equipamentos/ações |
| `Fighter+0x250` | grafo físico `MKOAKJBABIJ` |
| `Fighter+0x290` | controlador de animação `FOEDLNHCMNF` |
| `Fighter+0x298` | planejador de IA |
| `Fighter+0x3A0` | inventário de ações/configurações de ataque |
| `0x31E2E08` / `0x31E36DC` | Viewer/PreFight, padronizados antes da criação visual |
| `0x33A4314` | montagem dos controladores e ações do Fighter |
| `0x333F718` | criação do inventário de ações a partir do Profile |
| `0x339AEC4` | resolução dos nós físicos usados pelas configurações de ataque |

## Evidências nas duas instâncias do MuMu

Build nativa testada: SHA-256
`31F672C1B9C1E0D1B3E228C4C31F83F63A0F34B283E1734947C44E7537A22F94`.

APK incremental p146 assinado: `dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-146.apk`,
SHA-256 `F1848F599F188984CBA9F0760CF91908DE54DAF631341EE9A76A145DCD140727`.
A biblioteca ARM64 embutida no APK tem exatamente o mesmo SHA-256 da build
nativa testada. A assinatura APK v2 e v3 foi validada com `apksigner`.

Em `emulator-5554` e `emulator-5556`, os dois Fighters da luta carregaram,
antes do primeiro frame:

```text
physical-resources[0]=mdl_skeleton.xml
physical-resources[1]=mdl_weapon_ninja_sword.xml
physical-resources[2]=mdl_body.xml
physical-resources[3]=mdl_head.xml
```

Em cada processo, os lados esquerdo e direito apresentaram as mesmas sete
ações de arma e os mesmos objetos de configuração:

```text
KatanaSlash                 katana_slash.bytes
KatanaDoubleSlash           katana_double_slash.bytes
KatanaHeavySlash            katana_heavy_slash.bytes
KatanaSpinningSlash         katana_spinning_slash.bytes
KatanaUpperSlash            katana_upper_slash.bytes
KatanaLowSlash              katana_low_slash.bytes
NinjaSwordSuperSlash        ninja_sword_super_slash.bytes
Action audit LAN: match=1 left=7 right=7
```

As assinaturas dos nomes foram iguais nas duas instâncias
(`131a58b906869c06`). As assinaturas dos ponteiros de configuração são iguais
entre esquerda/direita dentro de cada processo; diferem entre processos por
ASLR, como esperado. A auditoria retornou `forbidden=0` para adaga, sai,
punhos, cutelos, Açougueiro e soco-inglês.

A IA permaneceu desligada nos dois lados (`Profile+0x68 = 0`) e as tentativas
do planejador continuaram registradas como `IA LAN suprimida`. O teste de
entrada registrou P1 como `index=0`, P2 como `index=1`, incluindo movimento e
soco; no segundo MuMu o redirecionamento local de P2 também foi confirmado.

Evidência visual:

- [quatro Fighters canônicos](evidence/p146-canonical-fighters.jpg);
- [movimento e golpes com Espada Ninja](evidence/p146-ninja-sword-controls.jpg).

## Integridade dos originais

- APK original: `09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA`;
- APK-base p141: `C9C9C7AF1235BB5BF2B42A16B376946D75655016EE32DB53F7EBEE74594CEE5C`;
- nenhum dos dois APKs foi substituído;
- nenhum `users.xml` foi alterado;
- as bibliotecas instaladas antes do experimento permanecem em
  `backups/native-libraries/before-equipment-p142/`.
