# Desenvolvimento do modo LAN

## Estado atual

- APK analisado: Definitive Edition 2.41.9, ARM64.
- Motor: Unity `2022.3.62f2` com IL2CPP metadata v31.
- Pacote original preservado: `com.sf2.de`.
- O APK ja contem `PvpScene`, dois jogadores, selecao de personagens, selecao de arenas e telas proprias de luta PVP.
- O prototipo 15 redireciona `GeneralMenuScene.ShowCredits` e `GeneralMenuScene.ShowExit` para o PVP oculto.
- O PVP local foi aberto e executado no MuMu Player 15 com dois lutadores na mesma arena.
- Movimento e ataque foram enviados ao jogo sem travamento, encerramento do processo ou erro fatal no log.
- A ponte UDP Android ja conecta host e cliente, valida o token e mantem heartbeat.
- Os pacotes de entrada ja sao validados, ordenados e mantidos num snapshot dentro do processo Android.
- Um hook ARM64 liga o snapshot aos sete leitores do `GamePad 1` usados pelo jogador 2.
- Movimento horizontal e ataque do segundo lutador foram confirmados no MuMu via UDP.
- Um segundo Android agora pode funcionar como controle LAN, com joystick e botoes sobrepostos ao jogo.
- O controle transmite eixos e botoes a aproximadamente 30 Hz e retem toques curtos ate o primeiro envio.
- O hook agora captura os resultados nativos do `GamePad 0` sem modificar os controles originais.
- Com `nativeControls=true`, cada aparelho transmite seu proprio analogico e botoes e recebe o adversario no `GamePad 1`.
- Cada peer le o perfil selecionado em `userdata/users.xml` e troca somente nivel, visual e equipamento de combate.
- A negociacao `PROFILE`/`READY` repete pacotes perdidos e so libera a sessao quando os dois perfis foram confirmados.

Artefato local:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-15.apk`

SHA-256:

`D6FE0959863BDEF5E35E9380AE1A0E6B26230C46B8A51B16B9FAD2B999097532`

O artefato nao e versionado no GitHub por conter os dados proprietarios do jogo.
O prototipo 15 foi compilado, assinado e instalado no MuMu Player 15. A atualizacao
preservou o `users.xml`, e o hash do APK instalado corresponde ao artefato em `dist/`.

## Compatibilidade de identidade do aplicativo

Os testes mostraram que a validacao interna desta Definitive Edition exige exatamente:

- pacote `com.sf2.de`;
- `versionName` `2.41.9`.

Mudar o pacote para `com.sf2.de.lan` ou acrescentar um sufixo a versao faz o jogo rejeitar a build durante a inicializacao. Por isso, o prototipo 15 nao pode coexistir com a instalacao original no mesmo Android e deve ser tratado como uma substituicao de teste.

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

O script de build apaga automaticamente a pasta de intermediarios ao terminar, inclusive
quando uma etapa falha. Isso evita manter cerca de 4 GB de arquivos decompilados por
prototipo. Use `-KeepWorkingDirectory` somente quando esses arquivos forem necessarios
para diagnosticar uma compilacao e remova a pasta manualmente depois.

## Descoberta do PVP oculto

A build 2.41.9 ja inclui `PvpScene`, selecao de dois personagens, selecao de arena, telas de versus/fim de luta e dois objetos de entrada independentes. Esse modo e local no mesmo aparelho e esta oculto na interface Android.

O prototipo redireciona temporariamente a opcao de creditos e a acao de sair/voltar para `GeneralMenuScene.ShowPvp`. Dentro da cena do dojo, o botao circular da esquerda abre o estado de combate com os dois lutadores. Isso permite validar os recursos de PVP que ja existem antes de adicionar a entrada remota. O patch e limitado pelo hash da `libil2cpp.so` e sempre gera outra biblioteca, sem escrever sobre a original.

### Validacao no MuMu Player 15

- Android 15, ABI `x86_64` com traducao ARM64;
- APK assinado e instalado com sucesso;
- inicializacao, importacao do save completo e abertura do PVP confirmadas;
- dois lutadores renderizados simultaneamente com controles de combate;
- processo `com.sf2.de` permaneceu ativo depois de comandos de movimento e ataque;
- nenhum `FATAL EXCEPTION`, `SIGSEGV` ou ANR do aplicativo no log verificado.

Essa validacao confirma o PVP local oculto, nao uma partida LAN completa. O segundo jogador ainda precisa receber os pacotes de entrada da camada de rede ja funcional.

### Validacao de entrada do prototipo 11

- ponte Java inicializada dentro do processo `com.sf2.de` por um provider nao exportado;
- cliente Android no MuMu conectado ao host Python no computador por UDP;
- handshake autenticado e heartbeat continuo confirmados nos dois lados;
- encerramento do host detectado pelo Android sem encerrar o jogo;
- status gravado em `sf2lan-status.json` para diagnostico via ADB;
- eixos, botoes e sequencia do pacote `INPUT` conferidos no MuMu;
- comandos atrasados sao descartados e um timeout restaura entrada neutra;
- `libsf2lanhook.so` localizou `libil2cpp.so` e instalou sete hooks ARM64;
- eixo horizontal remoto moveu o segundo lutador da borda para o centro;
- pulsos do botao de soco produziram ataques no segundo lutador;
- nenhum `FATAL EXCEPTION`, `SIGSEGV` ou ANR durante o teste.

O formato dos pacotes, a configuracao e os comandos de diagnostico estao em [`LAN-PROTOCOL.md`](LAN-PROTOCOL.md).

### Validacao do controle Android no prototipo 13

- overlay multitouch exibido em paisagem sobre o jogo, com joystick, soco, chute, arma de longo alcance, magia, habilidade de chefe e pausa;
- modo ativado somente com `controller=true`, sem instalar o hook do jogador 2 no aparelho-controle;
- handshake do MuMu com o host de diagnostico confirmado;
- joystick transmitiu valores graduais de `0.05` ate `1.00` e retornou a zero ao soltar;
- um toque curto em SOCO transmitiu `buttons=0x01` seguido por `buttons=0x00`;
- pulsos de botao ficam pendentes ate serem enviados, evitando perder um toque entre dois pacotes;
- APK anterior e saves das duas instancias do MuMu foram copiados antes da instalacao de teste.

Esta etapa transforma o segundo aparelho em controle remoto. Ela ainda nao replica a imagem,
o save nem a simulacao da luta no segundo aparelho. A luta continua sendo executada no anfitriao.

### Captura dos controles nativos

O caminho definitivo nao usa o overlay. Os mesmos sete hooks instalados para o jogador 2
deixam o retorno original do `GamePad 0` passar normalmente e registram eixos, botoes
pressionados e liberados. A ponte Java consulta esse snapshot antes de cada envio UDP.
O analogico virtual e os botoes originais continuam sendo desenhados e processados pelo jogo.

Essa integracao foi validada no prototipo 15. O status confirmou `nativeHook=true`,
`nativeControls=true`, leitura do perfil local e recepcao dos eixos e botoes enviados
pelo segundo peer sem substituir os controles originais.

### Validacao da sessao do prototipo 15

- os dados do MuMu foram copiados antes da atualizacao;
- o APK instalado e o artefato local possuem SHA-256 identico;
- `PROFILE` trocou nivel, visual e equipamento entre Android e peer de referencia;
- o Android recebeu um perfil remoto com o conjunto do Shogun;
- `localReady`, `remoteReady` e `sessionReady` ficaram verdadeiros;
- `INPUT` remoto registrou eixos `0.625`, `-0.25` e botoes `PUNCH | MAGIC`;
- o hook ARM64 permaneceu ativo durante a sessao;
- o MuMu expunha somente uma VM; por isso, o segundo peer desta validacao foi a
  implementacao Python, sem alterar o Poco X7 Pro.

O relay UDP de dois jogadores foi adicionado para contornar NAT de emuladores. Ele
autentica os dois clientes pelo token, responde ao heartbeat e encaminha `PROFILE`,
`READY` e `INPUT` sem modificar sequencia ou payload. Conexao Android-a-Android em
duas VMs ainda precisa ser repetida quando uma segunda instancia estiver disponivel.

O receptor Android expoe o ultimo comando valido por `LanTransport.getRemoteInput()`. O snapshot e imutavel e contem os eixos, a mascara de botoes, a sequencia UDP e o horario de recebimento. Essa API e a fronteira preparada para o hook JNI/IL2CPP da proxima etapa.

A etapa LAN usara o PVP existente como simulacao de combate e transformara a entrada do jogador 2 em uma entrada recebida pela rede local.

## Proxima integracao

O PVP usa InControl e mantem descritores separados para `PlayerOneInput` e `PlayerTwoInput`. A analise confirmou que o modo local configura o jogador 1 como `GamePad 0` e o jogador 2 como `GamePad 1`. A camada LAN devera:

1. capturar direcao, soco, chute, arma de longo alcance e magia do jogador local;
2. enviar estados numerados por quadro via UDP na rede local;
3. aplicar os estados recebidos ao segundo `InputDevice`;
4. trocar a configuracao dos dois lutadores antes da luta;
5. detectar divergencia de simulacao e solicitar ressincronizacao;
6. expor criar sala e entrar por IP dentro da interface.

O APK original nao declara permissao de internet. O prototipo 15 acrescenta `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` e `CHANGE_WIFI_MULTICAST_STATE` apenas na copia de trabalho. O manifesto do APK original permanece imutavel.

### Dependencia nativa

O hook e compilado com o Android NDK r30 LTS. Para preparar `.tools/` sem versionar o pacote de aproximadamente 700 MB:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\install-android-ndk.ps1
```

O instalador confere o SHA-1 oficial antes de extrair o compilador ARM64.
