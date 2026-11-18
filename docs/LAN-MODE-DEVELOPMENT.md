# Desenvolvimento do modo LAN

## Estado atual

- APK analisado: Definitive Edition 2.41.9, ARM64.
- Motor: Unity `2022.3.62f2` com IL2CPP metadata v31.
- Pacote original preservado: `com.sf2.de`.
- O APK ja contem `PvpScene`, dois jogadores, selecao de personagens, selecao de arenas e telas proprias de luta PVP.
- O prototipo 7 redireciona `GeneralMenuScene.ShowCredits` e `GeneralMenuScene.ShowExit` para o PVP oculto.
- O PVP local foi aberto e executado no MuMu Player 15 com dois lutadores na mesma arena.
- Movimento e ataque foram enviados ao jogo sem travamento, encerramento do processo ou erro fatal no log.
- A ponte UDP Android ja conecta host e cliente, valida o token e mantem heartbeat.
- Os pacotes de entrada ainda nao estao ligados ao `PlayerTwoInput`.

Artefato local:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-7.apk`

SHA-256:

`9F057375E1F784B0D82CD09DF8EE70DEAD6285EF39343C75B94A0EFCDDA230A6`

O artefato nao e versionado no GitHub por conter os dados proprietarios do jogo.

## Compatibilidade de identidade do aplicativo

Os testes mostraram que a validacao interna desta Definitive Edition exige exatamente:

- pacote `com.sf2.de`;
- `versionName` `2.41.9`.

Mudar o pacote para `com.sf2.de.lan` ou acrescentar um sufixo a versao faz o jogo rejeitar a build durante a inicializacao. Por isso, o prototipo 7 nao pode coexistir com a instalacao original no mesmo Android e deve ser tratado como uma substituicao de teste.

Antes de instala-lo em um aparelho real, e obrigatorio extrair o save atual, manter uma copia do APK original e confirmar que ambos podem ser restaurados. O teste atual foi feito apenas no emulador; o Poco X7 Pro nao foi alterado.

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
- o retorno ao jogo original e garantido por backup e restauracao, pois a validacao interna impede usar outro nome de pacote nesta build.

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

O prototipo 7 redireciona temporariamente a opcao de creditos e a acao de sair/voltar para `GeneralMenuScene.ShowPvp`. Dentro da cena do dojo, o botao circular da esquerda abre o estado de combate com os dois lutadores. Isso permite validar os recursos de PVP que ja existem antes de adicionar a entrada remota. O patch e limitado pelo hash da `libil2cpp.so` e sempre gera outra biblioteca, sem escrever sobre a original.

### Validacao no MuMu Player 15

- Android 15, ABI `x86_64` com traducao ARM64;
- APK assinado e instalado com sucesso;
- inicializacao, importacao do save completo e abertura do PVP confirmadas;
- dois lutadores renderizados simultaneamente com controles de combate;
- processo `com.sf2.de` permaneceu ativo depois de comandos de movimento e ataque;
- nenhum `FATAL EXCEPTION`, `SIGSEGV` ou ANR do aplicativo no log verificado.

Essa validacao confirma o PVP local oculto, nao uma partida LAN completa. O segundo jogador ainda precisa receber os pacotes de entrada da camada de rede ja funcional.

### Validacao do transporte do prototipo 7

- ponte Java inicializada dentro do processo `com.sf2.de` por um provider nao exportado;
- cliente Android no MuMu conectado ao host Python no computador por UDP;
- handshake autenticado e heartbeat continuo confirmados nos dois lados;
- encerramento do host detectado pelo Android sem encerrar o jogo;
- status gravado em `sf2lan-status.json` para diagnostico via ADB;
- nenhum `FATAL EXCEPTION`, `SIGSEGV` ou ANR durante o teste.

O formato dos pacotes, a configuracao e os comandos de diagnostico estao em [`LAN-PROTOCOL.md`](LAN-PROTOCOL.md).

A etapa LAN usara o PVP existente como simulacao de combate e transformara a entrada do jogador 2 em uma entrada recebida pela rede local.

## Proxima integracao

O PVP usa InControl e mantem descritores separados para `PlayerOneInput` e `PlayerTwoInput`. A camada LAN devera:

1. capturar direcao, soco, chute, arma de longo alcance e magia do jogador local;
2. enviar estados numerados por quadro via UDP na rede local;
3. aplicar os estados recebidos ao segundo `InputDevice`;
4. trocar a configuracao dos dois lutadores antes da luta;
5. detectar divergencia de simulacao e solicitar ressincronizacao;
6. expor criar sala e entrar por IP dentro da interface.

O APK original nao declara permissao de internet. O prototipo 7 acrescenta `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` e `CHANGE_WIFI_MULTICAST_STATE` apenas na copia de trabalho. O manifesto do APK original permanece imutavel.
