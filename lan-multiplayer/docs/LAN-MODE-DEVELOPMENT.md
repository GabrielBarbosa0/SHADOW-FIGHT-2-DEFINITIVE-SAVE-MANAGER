# Desenvolvimento do modo LAN

## Estado atual

- APK analisado: Definitive Edition 2.41.9, ARM64.
- Motor: Unity `2022.3.62f2` com IL2CPP metadata v31.
- Pacote original preservado: `com.sf2.de`.
- O IL2CPP contem `PvpScene`, logica para dois jogadores, selecao de personagens e arenas.
- O prototipo 15 contem redirecionamentos em `GeneralMenuScene`, mas essa cena nao e percorrida pelo fluxo da Definitive Edition.
- O prototipo 19 confirmou que `ModulePvp` e solicitado corretamente, mas a cena nao foi incluida no build Unity Android.
- O REA 6.9.0 confirmou de forma independente `PvpScene`, 12 campos internos, 27 metodos e os acessores/trocas de entrada dos dois jogadores.
- A tela com dois lutadores observada no dojo era uma pre-visualizacao, nao uma luta PVP completa.
- A ponte UDP Android ja conecta host e cliente, valida o token e mantem heartbeat.
- Os pacotes de entrada ja sao validados, ordenados e mantidos num snapshot dentro do processo Android.
- Um hook ARM64 converte o snapshot remoto nos mesmos comandos de toque usados pela luta.
- O prototipo 28 comprovou por ponteiros que direcao e ataque com `index=1` sao
  roteados ao objeto do adversario real, e nao ao jogador nem a uma pre-visualizacao.
- O prototipo 32 reutilizou a configuracao de duelo em modo `FightPVP`, inseriu um
  perfil humano clonado e manteve o adversario neutro sem entrada remota.
- O prototipo 33 corrigiu a ordem ativar/desativar dos eventos ofuscados, enviou
  soco remoto ao ponteiro exato do oponente e capturou os controles visuais nativos
  do jogador local para transmissao UDP.
- O prototipo 34 liga `PROFILE`/`READY` ao hook JNI. A configuracao `FightPVP`, o
  `DualMode` e a entrada remota so sao ativados quando a sessao esta pronta; uma
  desconexao neutraliza a entrada e um duelo offline conserva o modo original.
- Os prototipos 35 e 36 localizaram o gerenciador de itens IL2CPP e resolveram os
  cinco IDs recebidos (`Armor`, `Helm`, `Weapon`, `Ranged` e `Magic`) sem alterar o save.
- O prototipo 38 confirmou que o perfil adversario armazenado pela luta conserva os
  cinco objetos remotos. O diagnostico `--mirror-equipment lynx` produziu um Titã
  local contra o conjunto do Lynx numa arena de Torneio.
- O prototipo 39 consolida essa injecao e os logs de campos. Um comando horizontal
  enviado pelo computador foi registrado como `remote=1`, `index=1` e roteado ao
  ponteiro exato do adversario, que se deslocou para o outro lado da arena.
- O prototipo 40 intercepta os eventos de decisao da IA apenas quando o lutador e o
  perfil correspondem ao adversario LAN. Sem entrada remota, a luta de Torneio ficou
  sem ataques e sem perda de vida; um comando horizontal posterior voltou a ser
  roteado com `index=1` e moveu o mesmo oponente.
- Os prototipos 41 e 42 separam perfil pronto de arena pronta. O Android envia
  `FIGHT_READY` depois de criar os lutadores, recebe `START`, confirma `START_ACK` e
  mantem o `FixedUpdate` bloqueado ate o instante agendado. No MuMu, um atraso de
  4000 ms foi liberado com desvio medido de 4 ms. O prototipo 42 tambem publica esse
  estado em `sf2lan-status.json`.
- O prototipo 43 acrescenta o pacote `STATE`, com snapshots a 20 Hz e seis campos
  diagnósticos lidos no `FixedUpdate`. Os dois primeiros acessores candidatos a
  posição retornavam constantes e foram descartados, sem tratá-los como sucesso.
- O prototipo 44 usa o `GameObject` do lutador, `get_transform` e
  `Transform.get_position_Injected`. O valor `(-968, 176)` permaneceu constante
  mesmo durante deslocamentos: ele pertence ao contêiner da cena, não à posição
  jogável do lutador, e por isso foi retirado do pacote `STATE`.
- Os prototipos 45 a 47 ativaram a IA somente em builds de diagnostico e varreram
  todos os 16 getters `float` sem argumentos do lutador. Nenhum acompanhou dano ou
  movimento; `fight + 0x13C` foi identificado apenas como contador interno de frames.
- O prototipo 48 seguiu `FightScene + 0x98` até `PreFight`, `ViewerFight` e os dois
  `PlayerLifeBar`. No MuMu, `get_TimeLeft()` caiu de 150 para 141 e
  `get_TimeLeftFrames()` de 9000 para 8507 em sincronia com o HUD. As barras
  esquerda/direita foram lidas em `PlayerLifeBar + 0x80`; ambas permaneceram em
  `1.0` porque o save de teste usa vida infinita.
- O estado atual troca os antigos campos sem significado por cronômetro, barras
  esquerda/direita e contadores de frames, preservando o payload `STATE` de 40 bytes.
- O prototipo 49 consolida essa leitura na build normal. O host tambem reinicia a
  negociacao de perfil, `READY` e inicio de luta quando o Android se reconecta.
- O prototipo 50 compara o snapshot local com o remoto por identidade: como cada
  aparelho mostra seu proprio lutador a esquerda, o comparador cruza esquerda com
  direita. Ele publica deltas de tempo, vida, barra de impacto e idade do pacote em
  `sf2lan-status.json`, sem escrever esses valores de volta na luta.
- O prototipo 51 acrescenta autoridade de inicio ao relay para duas instancias em
  modo cliente e frame pacing no `FixedUpdate`. O aparelho adiantado segura a
  simulacao quando seu contador passa mais de oito frames do ultimo estado remoto
  recente; pacotes com mais de 500 ms nunca bloqueiam a luta.
- Um segundo Android agora pode funcionar como controle LAN, com joystick e botoes sobrepostos ao jogo.
- O controle transmite eixos e botoes a aproximadamente 30 Hz e retem toques curtos ate o primeiro envio.
- O hook agora captura os resultados nativos do `GamePad 0` sem modificar os controles originais.
- Com `nativeControls=true`, cada aparelho transmite seu proprio analogico e botoes e recebe o adversario no `GamePad 1`.
- Cada peer le o perfil selecionado em `userdata/users.xml` e troca somente nivel, visual e equipamento de combate.
- A negociacao `PROFILE`/`READY` repete pacotes perdidos e so libera a sessao quando os dois perfis foram confirmados.
- O prototipo 73 localizou uma fase visual persistente e passou a aplicar as
  coordenadas autoritativas do jogador 1 no segundo processo.
- O prototipo 74 tentou alinhar a orientacao pelo vetor de escala, mas testes
  posteriores comprovaram que esse campo nao controla o facing real do Fighter.
- O prototipo 77 eliminou o roteamento cruzado: P1/indice 0 permanece sempre a
  esquerda e P2/indice 1 sempre a direita nas duas telas.
- O prototipo 83 mantem o `mode=7` e clona a identidade do oponente original em vez
  do perfil do jogador. Os dois Fighters agora nascem frente a frente e a virada
  nativa ocorre quando eles cruzam. Um evento inicial da IA e liberado para iniciar
  o relogio; todas as decisoes posteriores continuam suprimidas.
- O prototipo 87 aplica temporariamente o preset canonico de P1 aos dois Fighters
  nas duas VMs. Os quatro modelos recebem o mesmo avatar e os mesmos cinco IDs
  enquanto posicao, camera, vida e rounds ainda estao sendo sincronizados.

Artefato local:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-87.apk`

SHA-256:

`7249F4F197B2D4C4FBD4F6A3C9BB633E0235E9D6AF5DACBF41D01ED3CF260F65`

O artefato nao e versionado no GitHub por conter os dados proprietarios do jogo.
O prototipo 51 foi compilado, assinado e instalado em duas VMs do MuMu Player 15.
Antes da atualizacao, o APK do prototipo 50 e todo o `userdata` de cada VM foram
copiados para `backups/mumu-pre-prototype51-20261124/peer1/` e `peer2/`. Os dois APKs
preservados possuem SHA-256
`49F61A8CF7C53C31F41058C53C0EBF2B787B1AD2448D6D8402AA496A177FF93A`.
Os hashes dos saves permaneceram iguais antes e depois da instalacao. O Poco X7 Pro
nao foi acessado.

### Validacao Android contra Android do prototipo 51

Duas VMs Android 15 independentes foram ligadas ao relay pelas portas 24190 e 24191.
O relay recebeu `FIGHT_READY` dos dois processos, criou a rodada `738708020`, enviou
`START` a ambos e registrou os dois `START_ACK`. Os dois status publicaram
`localReady=true`, `remoteReady=true`, `startScheduled=true`, `startAcked=true` e o
mesmo instante de inicio.

Sem frame pacing, o prototipo 50 havia chegado a aproximadamente 281 frames de
diferenca em cerca de um minuto. Com o prototipo 51, uma leitura intermediaria ficou
em sete frames e, depois de mais de um minuto, as duas comparacoes ficaram em oito e
nove frames, com `withinTolerance=true` e as quatro leituras de vida em `1.0`. O log
nativo registrou os holds aplicados ao processo adiantado. O status tambem confirmou
que um eixo capturado localmente numa VM chegou como entrada remota na outra.

Isso comprova duas simulacoes Android, handshake de arena, inicio coordenado,
transporte bilateral e limitacao de deriva de relogio. Nao comprova ainda um duelo
completo e autoritativo: posicao, animacao, dano e resultado continuam sendo
calculados separadamente e ainda podem divergir.

### Leitura validada do HUD

O dump e a desmontagem ARM64 confirmaram a cadeia `FightScene -> PreFight ->
ViewerFight -> ScreenModel -> PlayerLifeBar`. Os acessores de `PreFight` retornam
diretamente o tempo restante, os frames restantes, os frames transcorridos e o total
da rodada. Cada `PlayerLifeBar` conserva o valor atual em `+0x80` e o valor atrasado
de impacto em `+0x84`. Esses valores foram observados numa luta real do MuMu e agora
substituem a posição de contêiner e os seis probes inconclusivos no transporte.

O teste ainda não mede redução de vida porque o save da Definitive Edition usado no
MuMu mantém as duas barras infinitas. Isso limita a validação de dano, mas não a
identificação estrutural das barras: o valor lido é o mesmo que o método `Render()`
do `ScreenModel` envia ao componente visual.

Para validar o comparador com apenas um MuMu, o host de diagnostico devolveu cada
`STATE` com os lados invertidos. Depois de 602 frames, o Android registrou idade de
61 ms, delta de cronometro 0, delta maximo de vida 0 e `withinTolerance=true`. Esse
teste comprova serializacao, inversao e calculo; ele nao substitui o teste posterior
entre duas instancias Android independentes.

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
powershell -NoProfile -ExecutionPolicy Bypass -File .\lan-multiplayer\scripts\create-lan-modding-workspace.ps1
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

## Descoberta do codigo PVP e da cena ausente

A build 2.41.9 inclui no IL2CPP `PvpScene`, selecao de dois personagens, selecao de arena, telas de versus/fim de luta e dois objetos de entrada independentes. Entretanto, o APK Android possui somente `level0` a `level7`. Ao solicitar `ModulePvp`, a Unity registra: `Scene with build index: 9 couldn't be loaded because it has not been added to the build settings`.

O prototipo 19 localizou o carregador interno de cenas e solicitou diretamente o ID 9 no thread do jogo por meio de `DojoScene.FixedUpdate`. O log confirmou `GoToModule ModulePvp` e `LoaderScene::LoadLevel: scene: ModulePvp`, seguido do erro de cena ausente. O botao circular do dojo apenas alterna a pre-visualizacao do adversario e do saco de treino; ele nao abre a cena PVP. O patch e limitado pelo hash da `libil2cpp.so` e sempre gera outra biblioteca, sem escrever sobre a original.

O codigo de diagnostico usado nos prototipos 17 a 19 foi removido depois dessa confirmacao. Os resultados permanecem documentados, enquanto o codigo ativo volta a manter apenas a infraestrutura LAN validada no prototipo 15.

### Validacao no MuMu Player 15

- Android 15, ABI `x86_64` com traducao ARM64;
- APK assinado e instalado com sucesso;
- inicializacao e importacao do save completo confirmadas;
- chamada ao modulo PVP e falha de carregamento reproduzidas sem `SIGSEGV` ou `FATAL EXCEPTION`;
- ausencia de `level9`/`ModulePvp` confirmada no APK original;
- processo permaneceu ativo, parado na tela de carregamento por falta da cena.

Essa validacao corrige a conclusao anterior: a infraestrutura de codigo PVP existe, mas o modo nao e jogavel neste APK sem o asset da cena. A proxima etapa e localizar uma cena `ModulePvp` da mesma versao Unity/IL2CPP ou reconstruir uma cena equivalente antes de aplicar perfis e sincronizacao de luta.

A analise posterior com o REA refinou esse plano: em vez de depender de encontrar o
asset original, o primeiro caminho sera reutilizar uma cena de luta ja empacotada. O
marco e instanciar nela dois lutadores independentes por meio de `FightScene`,
`PreFight.SetPlayerPvpMode`, `PlayerOneInput` e `PlayerTwoInput`. Os resultados e o
script reproduzivel estao em [`REA-DEFINITIVE-EDITION-ANALYSIS.md`](REA-DEFINITIVE-EDITION-ANALYSIS.md).

### Diagnostico da FightScene

O proximo prototipo nao usa o dojo como indicador e nao solicita `ModulePvp`. O hook
nativo instrumenta `FightScene.CreateFight` no RVA `0x3234A0C` e registra somente uma
luta realmente criada, incluindo a instancia da simulacao e os dois combatentes nos
offsets `0xB0` e `0xB8`. Os leitores de entrada tambem registram a primeira consulta
real ao `GamePad 1`.

O patch antigo do menu PVP passou a ser opcional e fica desativado por padrao. Assim,
uma tela com dois modelos no dojo nao pode ser confundida novamente com multiplayer.
O teste positivo exige `FightScene.CreateFight` no log, HUD de luta e comandos do
segundo jogador consultados durante a rodada.

Os prototipos 20 e 21 atingiram a primeira metade desse criterio no MuMu. Um duelo
real registrou `FightScene.CreateFight`, exibiu o HUD e criou dois combatentes
distintos. A IA continuou controlando o adversario e nenhum leitor de `GamePad 1`
foi consultado. Portanto, isso ainda nao e uma luta multiplayer.

O prototipo 21 instrumentou tambem `KMDCKLBDEPP.DGGDIICKKIA`, no RVA `0x33DF1AC`.
Toques nos botoes e no analogico nativos chegaram ao roteador como `index=0` e
foram associados consistentemente ao mesmo lutador. Esse roteador e agora o ponto
de integracao preferido.

Os prototipos 22 a 24 mostraram que apenas alimentar os leitores de `GamePad 1` nao
bastava: a luta normal nao os consultava. Os prototipos 25 a 27 prepararam a injecao
direta, mas falharam antes de produzir uma entrada valida por causa de suposicoes
incorretas sobre alocacao IL2CPP; eles nao contam como resultado de gameplay.

O prototipo 28 localiza a classe de descritor `ICEJFAAGIAG` pelas APIs oficiais do
IL2CPP, instancia um comando de toque com `index=1` e o entrega diretamente a luta.
No mesmo evento, o log registrou:

- `player=0x7240bbd81000` e `opponent=0x723e79244540` na criacao da luta;
- soco remoto (`control=9`) resolvido para `fighter=0x723e79244540`;
- direcao para frente (`control=3`) resolvida para o mesmo ponteiro;
- eventos separados de pressionar e soltar para ambos os comandos;
- nenhuma falha nativa durante o teste.

Essa igualdade exata de ponteiros e a primeira prova de que a entrada LAN alcancou
o adversario real. O movimento visual do bot, isoladamente, continua nao sendo usado
como evidencia. A IA normal ainda controla o adversario em paralelo; portanto, o
prototipo 28 ainda nao e um multiplayer jogavel.

### Remocao da IA e entrada bidirecional

Os prototipos 29 a 31 isolaram o caminho que ainda faltava:

- o prototipo 29 demonstrou que a IA nao passa pelas funcoes de toque instrumentadas;
- o prototipo 30 mostrou que alterar somente o modo de luta para `FightPVP` nao basta;
- o prototipo 31 tentou carregar `PvpBattle|pvp|1`, mas o catalogo dessa configuracao
  nao esta disponivel no fluxo normal do duelo.

O prototipo 32 manteve a configuracao ja carregada da arena, mudou seu modo para
`FightPVP` e adicionou uma copia valida do perfil do jogador na lista de oponentes da
configuracao. Durante cerca de 25 segundos sem pacotes, o cronometro continuou e as
duas barras de vida permaneceram intactas. Depois, um soco UDP com `index=1` resolveu
para o mesmo endereco registrado como `opponent` e atingiu o lutador da direita.

O prototipo 33 confirmou o caminho inverso. Um toque no botao de soco original gerou
`buttons=0x01` seguido de `0x00` no host de referencia; arrastar o analogico original
para a frente gerou `horizontal=1.000` seguido de zero. Nenhum overlay adicional foi
usado. A instrumentacao preserva pulsos curtos entre dois envios de 33 ms e traduz as
oito direcoes do analogico para o protocolo.

Este resultado comprova uma arena local sem IA e entrada bidirecional Android/UDP no
MuMu. Ele ainda nao comprova duas simulacoes sincronizadas nem uma luta completa entre
dois Androids.

### Ativacao segura da sessao no prototipo 34

O estado bilateral `localReady && remoteReady` agora atravessa o JNI por
`nativeSetSessionReady`. O hook usa esse sinal em tres pontos:

- pacotes `INPUT` so sao enviados e aceitos depois do handshake completo;
- `FightScene.CreateFight` so prepara `FightPVP` e `DualMode` quando a sessao esta pronta;
- timeout ou desconexao zeram eixos, botoes e latches, liberando comandos que estavam ativos.

No MuMu, o peer Python com `--mirror-profile` devolveu o perfil recebido e enviou
`READY`. O status registrou `localReady=true`, `remoteReady=true` e
`sessionReady=true`; o log nativo registrou `Sessao LAN nativa: pronta`. Ao abrir um
duelo, o log mostrou `sessaoPronta=1`, modo original 7 e preparacao `FightPVP`. Depois
de encerrar o peer, o hook registrou a sessao como inativa. O mesmo duelo aberto sem
peer mostrou `sessaoPronta=0`, manteve a configuracao original e nao chamou a
preparacao PVP.

Essa validacao comprova o bloqueio de seguranca e a transicao de sessao. Nos
prototipos 36 a 39, o `PROFILE` remoto tambem passou a ser resolvido para objetos de
equipamento IL2CPP. O teste espelhado com o conjunto do Lynx comprovou que os cinco
itens chegam ao perfil adversario usado pela luta.

### Perfil remoto e arena de Torneio nos prototipos 38 e 39

O gerenciador interno de itens resolveu `BODY_LYNX`, `HEAD_LYNX`,
`WEAPON_LYNX_CLAWS`, `NoRanged` e `NoMagic`. O clone remoto foi inserido na lista de
adversarios antes de `FightScene.CreateFight`, e o perfil armazenado pela luta manteve
os mesmos cinco ponteiros.

O modo Duelo nao serve para validar equipamentos individuais: sua regra nativa de
equipamento aleatorio aplica o mesmo conjunto aos dois lados. Numa luta de Torneio,
o jogador local conservou o corpo e a espada do Titã enquanto o adversario recebeu o
conjunto do Lynx. Em seguida, o peer enviou eixo horizontal positivo por tres segundos.
O hook registrou `remote=1`, `control=3`, `index=1` e o ponteiro do adversario; a
captura seguinte mostrou o segundo lutador na borda direita.

Isso e evidencia de perfil remoto e controle independente dentro de uma cena real de
luta. Ainda nao comprova duas simulacoes Android sincronizadas, nem garante que a IA
esteja completamente desligada durante toda a rodada.

### Validacao de entrada do prototipo 11

- ponte Java inicializada dentro do processo `com.sf2.de` por um provider nao exportado;
- cliente Android no MuMu conectado ao host Python no computador por UDP;
- handshake autenticado e heartbeat continuo confirmados nos dois lados;
- encerramento do host detectado pelo Android sem encerrar o jogo;
- status gravado em `sf2lan-status.json` para diagnostico via ADB;
- eixos, botoes e sequencia do pacote `INPUT` conferidos no MuMu;
- comandos atrasados sao descartados e um timeout restaura entrada neutra;
- `libsf2lanhook.so` localizou `libil2cpp.so` e instalou sete hooks ARM64;
- os pacotes remotos chegaram aos leitores preparados para `GamePad 1`;
- nao houve, nessa etapa, prova de movimento ou ataque de um segundo lutador;
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
o save nem a simulacao da luta no segundo aparelho. A luta continua sendo executada no anfitriao,
e, nessa etapa historica, a ligacao entre esse controle e o adversario ainda nao existia.

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

A etapa LAN nao depende mais de recuperar `ModulePvp`: o caminho atual reutiliza uma
`FightScene` ja presente. No prototipo 34, a IA foi retirada da rodada de teste, a
entrada recebida controla o adversario, os controles nativos locais sao transmitidos
e essa alteracao so ocorre depois do handshake bilateral.

## Proxima integracao

O perfil remoto, o roteamento de entrada, o transporte de estado, o calculo de
divergencia e o frame pacing ja funcionam numa `FightScene` real entre duas VMs
Android. Vida do HUD e cronometro ja foram localizados. O proximo marco e adicionar
autoridade de estado sem mascarar divergencias. A camada LAN devera:

1. neutralizar a IA do segundo lutador durante toda a rodada LAN;
2. selecionar uma arena sem a regra de equipamento aleatorio do Duelo;
3. localizar a transformacao jogavel real ou sincronizar eventos/animacoes sem depender dela;
4. escolher e aplicar uma autoridade para vida, posicao e resultado da rodada;
5. testar golpes, bloqueios, quedas e fim de round com saves sem vida infinita;
6. validar reconexao e uma segunda rodada sem reiniciar os aplicativos;
7. expor criar sala e entrar por IP dentro da interface.

O APK original nao declara permissao de internet. O prototipo 15 acrescenta `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` e `CHANGE_WIFI_MULTICAST_STATE` apenas na copia de trabalho. O manifesto do APK original permanece imutavel.

### Dependencia nativa

O hook e compilado com o Android NDK r30 LTS. Para preparar `.tools/` sem versionar o pacote de aproximadamente 700 MB:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\lan-multiplayer\scripts\install-android-ndk.ps1
```

O instalador confere o SHA-1 oficial antes de extrair o compilador ARM64.

Para iterar somente a biblioteca nativa sem decompilar novamente o APK de cerca de
1 GB, use uma build anterior como base imutavel:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\lan-multiplayer\scripts\repack-lan-native.ps1 `
  -BaseApk .\dist\Shadow-Fight-2-Definitive-Edition-LAN-prototype-66.apk `
  -WorkingDirectory .\build\lan-native-next `
  -OutputApk .\dist\Shadow-Fight-2-Definitive-Edition-LAN-prototype-next.apk
```

O script recompila apenas `libsf2lanhook.so`, atualiza uma copia do APK, realinha,
assina, verifica as assinaturas v2/v3 e remove os intermediarios ao terminar.

Cada APK completo ocupa aproximadamente 1,05 GiB. Depois de validar uma nova build,
preserve somente o APK atual em `dist/` e remova as copias de instalacao antigas com:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\lan-multiplayer\scripts\cleanup-lan-builds.ps1
# Confira a simulacao e, em seguida:
powershell -NoProfile -ExecutionPolicy Bypass -File .\lan-multiplayer\scripts\cleanup-lan-builds.ps1 -Apply
```

A limpeza nunca toca no APK original, em `game/safety-backups/`, nem nos saves/XMLs.

## Autoridade espacial: prototipos 59 a 66

O prototipo 59 confirmou que `Fighter.BCJBEELDLAB` (`RVA 0x339FB0C`) retorna o
vetor fisico individual do esqueleto. Os componentes Unity examinados antes disso
apontavam para uma raiz compartilhada e nao serviam para distinguir os lutadores.
As coordenadas tambem confirmaram a inversao de identidades entre os aparelhos:
o jogador local do peer 1 corresponde ao adversario do peer 2.

O prototipo 60 enviou as seis coordenadas em `FIGHT_AUTHORITY`, calibrou as duas
origens e tentou corrigir o peer 2 antes de `FightScene.FixedUpdate`. A calibracao
funcionou, mas a fisica sobrescreveu a posicao no mesmo frame. Os prototipos 61 a
65 isolaram por que a escrita posterior nao era executada:

- `FightScene.FixedUpdate` (`0x3234C50`) e apenas um encaminhador que termina com
  `b 0x34EC2E4`; por ser um tail-branch, o trampoline nao retorna ao codigo que
  vem depois da chamada original;
- a rotina real `0x34EC2E4` possui `ret`, mas os pontos externos continuaram sem
  oferecer uma fase posterior estavel para a escrita;
- chamar o setter IL2CPP e gravar diretamente os floats `+0x10/+0x14/+0x18`
  ficou preparado com leitura de volta, sem alterar dano ou resultado da rodada.

O prototipo 66 adicionou um hook em `CPPLNBDPBGM.NAKIMBDKMKM`
(`0x34EEE70`), chamado por `bl` dentro do passo real. A build manteve handshake,
mesma rodada, cronometro com diferenca de poucos frames e vida identica, mas esse
ponto tambem nao publicou uma fase posterior confiavel no fluxo observado. Nao
houve crash, corrupcao de save nem instalacao em aparelho fisico.

A varredura completa de referencias encontrou somente dois chamadores de
`NAKIMBDKMKM`: `0x314392C`, pertencente a um despachante de diagnostico, e
`0x34EC3AC`, dentro do laco real iniciado em `0x34EC2E4`. A chamada real acontece
antes do decremento/repeticao do laco (`0x34EC3B0`/`0x34EC3B4`), portanto nao e uma
fronteira de fim de frame. Isso explica por que usa-la como callback pos-fisica nao
produziu um estado visual/canonico persistente.

## Configuracao autoritativa de equipamentos

Antes de continuar a autoridade espacial, o protocolo passou a negociar uma unica
configuracao de partida. O host direto ou o relay escolhe um seed, o menor nivel dos
dois perfis (1..52) e dois presets independentes entre `initial`, `lynx`, `hermit`,
`butcher`, `wasp`, `widow`, `shogun` e `titan`. O mesmo preset pode sair para os dois.

Os Androids derivam o perfil local/remoto a partir de `PLAYER_ROLE`, aplicam os cinco
slots pelo `PlayerProfile.SetItem` e confirmam o fingerprint SHA-256. Entrada e criacao
da luta ficam bloqueadas ate os dois confirmarem. A ponte Java e o hook ARM64 compilam,
e os testes do protocolo cobrem round-trip, sorteio reproduzivel e confirmacao bilateral
pelo relay. Ainda falta validar visualmente esta etapa nas duas VMs e localizar uma
forma segura de escrever o nivel interno; ate la, `matchLevel` e metadado comum, nao
uma afirmacao de normalizacao completa dos atributos.

### Validacao dos prototipos 67 e 68

O prototipo 67 confirmou em duas VMs MuMu que o relay cria uma configuracao unica,
os dois Androids devolvem o mesmo fingerprint e `sessionReady` permanece bloqueado
ate dois `MATCH_CONFIG_ACK`. O primeiro teste revelou reaplicacao do perfil em todo
ciclo da ponte Java; nao houve crash, mas o log recebia milhares de cargas repetidas.

O prototipo 68 tornou a aplicacao idempotente e passou pela validacao completa:

- APK assinado com v2/v3 e instalado nas duas VMs sem alterar os hashes dos saves;
- P1/P2 receberam o mesmo fingerprint e confirmaram a configuracao em 2/2;
- cada VM publicou `matchConfigApplied=true`, `matchConfigConfirmed=true` e
  `sessionReady=true`;
- cada perfil local/remoto foi enviado ao nativo exatamente uma vez;
- o sorteio de teste escolheu Titan para P1 e Hermit para P2;
- no P1, a luta continha `WEAPON_TITAN_GIANT_SWORD` no jogador e
  `WEAPON_HERMIT_SWORDS` no adversario; no P2, a relacao foi invertida;
- os cinco slots de ambos os perfis foram resolvidos pelo item manager e os ponteiros
  de perfil local/remoto permaneceram distintos dentro da `FightScene`;
- as duas VMs carregaram a mesma arena, receberam o mesmo `roundId` e confirmaram os
  dois `START_ACK`, sem `FATAL`, `SIGSEGV` ou corrupcao do save.

Essa validacao comprova a selecao autoritativa de equipamentos. Ela nao resolve a
sincronizacao de posicao, orientacao, dano, animacao ou fim de rodada, que continua
sendo o proximo marco do multiplayer.

O proximo prototipo deve aplicar a autoridade depois do retorno completo da rotina
`0x34EC2E4`, ou sincronizar diretamente o estado/animacao que alimenta colisao. A
validacao sera incremental: primeiro posicao e orientacao com `target/readback`;
depois dano e resultado; por fim animacao, queda e transicao de round. O teste deve
exigir no log as linhas `Autoridade espacial escrita` e comparar `target` com
`readback` antes de considerar a posicao sincronizada.

## Posicao visual e lados canonicos: prototipos 69 a 77

Os prototipos 69 a 72 testaram pontos intermediarios da apresentacao sem produzir
uma escrita visual persistente. O prototipo 73 encontrou
`ILLIPBFOIJK.LODMPBBLOPJ` (`RVA 0x3321460`), executado quando a posicao calculada do
lutador e copiada para o no apresentado. A escrita nesse ponto permaneceu ativa,
produziu `target/readback` identicos e fez o segundo MuMu acompanhar deslocamentos
recebidos pela rede. O prototipo 74 acrescentou a orientacao pelo vetor de escala
do `Fighter`, fazendo o lutador da esquerda olhar para o da direita.

As capturas manuais seguintes revelaram um problema diferente: o transporte estava
correto, mas cada processo ainda tratava seu jogador local como o lado esquerdo.
Assim, P1 controlava a esquerda em sua tela e a direita na tela de P2; P2 fazia o
inverso. O desenho em X era evidencia de entrada bilateral real, mas nao era uma
composicao aceitavel para o duelo.

O prototipo 76 tentou resolver o X apenas trocando ancoras e sinal das coordenadas.
O teste rejeitou essa abordagem: os eventos recebidos ainda eram criados com
`index=1`, portanto P1 continuava movendo o lado direito no segundo processo.

O prototipo 77 corrigiu a origem do problema e estabeleceu lados canonicos:

- no jogador 1, o toque local continua em `index=0` e a entrada recebida de P2 usa
  `index=1`;
- no jogador 2, a entrada recebida de P1 usa `index=0`, enquanto o toque local e
  recriado em `index=1` antes de chegar a `FightInput`;
- os perfis sao aplicados na mesma ordem visual: jogador 1 a esquerda e jogador 2
  a direita;
- posicao e autoridade de combate usam a ordem direta `{0, 1}` em ambos os processos.

A validacao no MuMu moveu somente o analogico de cima para a esquerda. O segundo
processo registrou `Comando LAN ... index=0`, e o lutador esquerdo se afastou para a
esquerda nas duas telas. Em seguida, somente o analogico de baixo foi movido para a
direita. O segundo processo registrou o redirecionamento local em `index=1`, o
primeiro recebeu `Comando LAN ... index=1`, e o lutador direito se afastou para a
direita nas duas telas. Nao houve `SIGSEGV` ou `FATAL`.

Artefato validado localmente:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-77.apk`

SHA-256:

`A25A0CFD4C9AA7881D9D57322C4FAB76E24629DA1A4062BCA62BD9B76EE47426`

Essa validacao encerra apenas o marco de lados e movimento. Animacao, colisao,
dano, vida, cronometro, rounds e vitoria ainda precisam de autoridade compartilhada.

## Facing nativo do Duelo: prototipos 78 a 83

Os prototipos 78 e 79 tentaram inverter a escala do no visual e o parametro de
inicio da animacao. Os dois testes foram rejeitados: os personagens continuaram
olhando para a mesma direcao. A leitura de memoria mostrou ainda que o campo
`Fighter + 0x198` distingue o lado criado, mas altera-lo depois da construcao nao
vira o modelo.

A causa estava na configuracao usada desde os primeiros prototipos: o catalogo
`PvpBattle|pvp|1`, em `mode=17`, cria dois Fighters com identidade de jogador. O
prototipo 80 voltou ao `mode=7` do Duelo, mas ainda clonava o perfil do jogador para
o adversario e, por isso, conservou o erro visual. O prototipo 81 passou a clonar o
oponente original do Duelo e aplicar apenas o equipamento remoto. As duas VMs entao
criaram corretamente um `Player` e um `Opponent`, ambos frente a frente.

Esse oponente nativo depende de um primeiro callback do planejador da IA para sair
da apresentacao e iniciar o cronometro. Bloquear todos os callbacks deixou o HUD em
150. O prototipo 82 de diagnostico confirmou a relacao ao liberar a IA completa. O
prototipo 83 adotou a solucao restrita: permite exatamente o primeiro callback e
suprime todos os seguintes. O relogio passou a correr, a IA deixou de tomar novas
decisoes e a entrada LAN continuou chegando aos indices canonicos.

Validacao do prototipo 83 em duas VMs MuMu:

- ambas carregaram `mode=7`, o mesmo fingerprint de arena e o mesmo `START`;
- os dois lados exibiram o lutador esquerdo olhando para a direita e o direito
  olhando para a esquerda;
- depois de um salto que trocou a ordem horizontal no jogador 1, o motor fez a
  virada automaticamente e manteve os lutadores frente a frente;
- o log registrou um unico `evento unico de inicializacao liberado` e depois apenas
  `IA remota suprimida`;
- nao houve `SIGSEGV`, `FATAL` nem alteracao do save durante a instalacao.

Artefato local validado nessa etapa:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-83.apk`

SHA-256:

`4C4EFBBAFA670C05C59175B1C914DE3808B6077C5BDFC7C46D11E50601987F96`

A apresentacao espacial do segundo aparelho ainda pode sair da camera depois de
deslocamentos longos e a transicao de round permanece dessincronizada. Esses pontos
sao o proximo marco; o prototipo 83 resolve o facing, nao o estado completo da luta.

## Equipamento canônico temporário: protótipos 84 a 87

Para retirar a divergencia de armas e armaduras dos testes de sincronizacao, o
prototipo 84 passou a aplicar temporariamente o preset canonico de P1 aos dois perfis antes de
`FightScene.CreateFight`. No aparelho de papel 1, esse preset vem do perfil local;
no aparelho de papel 2, ele vem do perfil remoto recebido pela rede. A identidade
nativa `Player/Opponent` introduzida no prototipo 83 permanece inalterada.

O primeiro teste comprovou que igualar apenas os cinco ponteiros de itens nao era
suficiente: o motor ainda escolhia a apresentacao a partir do avatar antigo de cada
perfil. O prototipo 85 tentou incluir a aparencia, mas foi rejeitado porque escreveu
num offset de texto incorreto. O prototipo 86 mapeou os campos em execucao e confirmou
`nome=0x188` e `avatar=0x198`. O prototipo 87 escreve somente o avatar correto em
`0x198`, sem tocar no nome nem na identidade que determina o lado do Fighter.

Validacao em duas VMs MuMu:

- no teste final, papel 1 e papel 2 resolveram os mesmos IDs: `ARMOR_WASP`,
  `HEAD_WASP`, `WEAPON_WASP_NAGINATA`, `NoRanged` e `NoMagic`;
- em cada VM, os perfis esquerdo e direito aplicaram `5/5` itens;
- os perfis armazenados pela luta conservaram os cinco ponteiros aplicados;
- as duas telas exibiram o mesmo retrato/avatar da Vespa e o mesmo loadout nos dois
  lados, mantendo os Fighters frente a frente;
- os 32 testes automatizados do protocolo e do relay passaram;
- a instalacao foi limitada a `emulator-5554` e `emulator-5556`; o Poco X7 Pro nao
  foi acessado;
- o enquadramento/posicao da segunda tela continua sendo uma pendencia independente
  e pode deixar os lutadores fora da camera.

Este comportamento e intencionalmente transitorio. Depois da convergencia de estado,
P1 e P2 voltarao a receber presets independentes, escolhidos pelo sorteio autoritativo
ja existente.

### Regressao conhecida depois da validacao visual

Um teste manual posterior mostrou que a IA remota ainda pode executar uma acao. Os
logs confirmam que a identificacao do lutador remoto funciona e que apenas o primeiro
callback original e liberado; os seguintes sao suprimidos. A regressao ocorre porque
esse primeiro callback, introduzido para encerrar a apresentacao do Duelo e iniciar o
relogio, tambem pode armar o planejador ou deixar uma ordem enfileirada. Portanto, a
build 87 e um checkpoint reproduzivel dos equipamentos canonicos, mas nao representa
isolamento completo da IA. Antes de avancar para vida e rounds, o proximo prototipo
deve reproduzir somente a transicao inicial necessaria, sem chamar o planejador, ou
neutralizar explicitamente o controlador e qualquer comando pendente.

Artefato local atual:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-87.apk`

SHA-256:

`7249F4F197B2D4C4FBD4F6A3C9BB633E0235E9D6AF5DACBF41D01ED3CF260F65`
