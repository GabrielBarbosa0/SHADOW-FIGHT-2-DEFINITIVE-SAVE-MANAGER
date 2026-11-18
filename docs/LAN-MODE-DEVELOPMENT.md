# Desenvolvimento do modo LAN

## Estado atual

- APK analisado: Definitive Edition 2.41.9, ARM64.
- Motor: Unity `2022.3.62f2` com IL2CPP metadata v31.
- Pacote original preservado: `com.sf2.de`.
- Pacote do prototipo: `com.sf2.de.lan`, instalavel separadamente.
- O APK ja contem `PvpScene`, dois jogadores, selecao de personagens, selecao de arenas e telas proprias de luta PVP.
- O prototipo 1 redireciona a opcao de creditos para o PVP oculto.
- A transmissao de comandos pela rede ainda nao faz parte do prototipo 1.

Artefato local:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-1.apk`

SHA-256:

`143D24B582C98CBD064B4F748FC1674F87100809B405D1534DA6E9E555F1C727`

O artefato nao e versionado no GitHub por conter os dados proprietarios do jogo.

## Regra de preservacao

O APK e o `users.xml` recebidos com a Definitive Edition sao tratados como arquivos imutaveis. Nenhuma ferramenta de extracao, patch, assinatura ou instalacao deve escrever sobre esses arquivos.

Antes de iniciar uma sessao de modificacao, execute:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\create-lan-modding-workspace.ps1
```

O script:

1. valida os hashes SHA-256 dos dois originais;
2. mantem uma copia permanente em `game/safety-backups/`;
3. cria uma nova copia de trabalho em `game/lan-workspaces/`;
4. recusa substituir qualquer area de trabalho existente;
5. grava um manifesto de origem junto da copia de trabalho.

As pastas ficam sob `game/`, portanto nao sao publicadas no GitHub.

## Objetivo da primeira versao jogavel

- um jogador hospeda uma partida na rede local;
- o segundo jogador informa ou descobre o endereco do anfitriao;
- cada aparelho usa o equipamento e os atributos do proprio save;
- a primeira versao nao aplica balanceamento;
- a partida deve funcionar sem servidor externo;
- a build resultante usa nome e assinatura distintos da distribuicao original sempre que a arquitetura do APK permitir, para que testes e retorno ao jogo original sejam seguros.

## Etapas tecnicas

1. identificar a versao do Unity/IL2CPP e mapear as classes responsaveis por luta, entrada e estado do guerreiro;
2. produzir um inventario reproduzivel dos arquivos e metadados do APK;
3. criar o transporte LAN e o protocolo de sala;
4. integrar a troca de estado e comandos de combate;
5. adicionar a interface de criar/entrar na sala;
6. recompilar, assinar e instalar a build de teste em dois aparelhos;
7. validar conexao, selecao dos saves e uma luta completa.

O APK final sera sempre gerado em uma pasta de saida separada. O APK original e o save original nao serao usados como destino de build.

## Descoberta do PVP oculto

A build 2.41.9 ja inclui `PvpScene`, selecao de dois personagens, selecao de arena, telas de versus/fim de luta e dois objetos de entrada independentes. Esse modo e local no mesmo aparelho e esta oculto na interface Android.

O primeiro prototipo redireciona temporariamente a opcao de creditos para `GeneralMenuScene.ShowPvp`. Isso permite validar os recursos de PVP que ja existem antes de adicionar a rede. O patch e limitado pelo hash da `libil2cpp.so` e sempre gera outra biblioteca, sem escrever sobre a original.

A etapa LAN usara o PVP existente como simulacao de combate e transformara a entrada do jogador 2 em uma entrada recebida pela rede local.

## Proxima integracao

O PVP usa InControl e mantem descritores separados para `PlayerOneInput` e `PlayerTwoInput`. A camada LAN devera:

1. capturar direcao, soco, chute, arma de longo alcance e magia do jogador local;
2. enviar estados numerados por quadro via UDP na rede local;
3. aplicar os estados recebidos ao segundo `InputDevice`;
4. trocar a configuracao dos dois lutadores antes da luta;
5. detectar divergencia de simulacao e solicitar ressincronizacao;
6. expor criar sala e entrar por IP dentro da interface.

O APK original nao declara permissao de internet. O clone de desenvolvimento ja declara `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` e `CHANGE_WIFI_MULTICAST_STATE`, sem alterar o manifesto original.
