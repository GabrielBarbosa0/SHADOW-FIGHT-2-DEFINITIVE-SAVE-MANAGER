# Protocolo SF2 LAN

## Marco atual

O protótipo atual acrescenta uma ponte UDP, um hook ARM64, negociação de lutadores e leitura de estado ao processo Android do jogo. Ele já executa:

- configuração de host ou cliente;
- autenticação por token compartilhado;
- handshake `HELLO` / `WELCOME`;
- heartbeat `PING` / `PONG`;
- detecção de desconexão;
- serialização do estado de movimento e botões;
- validação, ordenação e armazenamento dos pacotes `INPUT` no Android;
- neutralização automática da entrada quando a conexão expira;
- aplicação dos eixos e botões diretamente ao objeto do adversário por descritores de toque com `index=1`;
- captura de joystick e botões num segundo Android em modo controle;
- transmissão da entrada a aproximadamente 30 Hz, com retenção de toques curtos;
- captura dos botões e do analógico nativos pelo mesmo caminho de toque usado pela luta;
- status legível fora do processo do jogo;
- troca autenticada de perfis `PROFILE` e confirmação bilateral `READY`;
- resolução dos cinco IDs de equipamento remoto pelo gerenciador IL2CPP do jogo;
- substituição do perfil adversário antes da criação da luta;
- identificação do segundo lutador pelo ponteiro e pelo perfil clonados;
- supressão dos eventos de decisão da IA somente nesse adversário enquanto a sessão LAN está pronta;
- confirmação separada de que a arena e os dois lutadores foram criados com `FIGHT_READY`;
- agendamento confiável do início com `START` e confirmação `START_ACK`;
- bloqueio do `FixedUpdate` da luta até o instante de início combinado, com liberação de segurança após 15 segundos;
- liberação de `INPUT` e de `FightPVP` somente depois que os dois lados ficam prontos;
- retorno imediato da entrada remota ao estado neutro em timeout ou desconexão;
- leitura validada do cronômetro, das duas barras do HUD e dos contadores de frames no thread da Unity;
- transmissão e armazenamento de snapshots `STATE` a aproximadamente 20 Hz;
- comparação de snapshots na composição canônica P1 à esquerda/P2 à direita nos
  dois aparelhos, publicando deltas e tolerância em `sf2lan-status.json`;
- relay opcional de dois jogadores para clientes separados por NAT;
- coordenação de `START` pelo relay quando os dois Androids operam como clientes;
- frame pacing nativo que segura somente a simulação adiantada quando o estado remoto
  recente indica uma diferença superior a oito frames.

A ponte Android e a implementação de referência em Python usam o mesmo protocolo. O teste MuMu-to-PC foi concluído com conexão e heartbeat contínuo, sem falha fatal no jogo.

A ponte já transporta os comandos dos dois lados. A `FightScene` só é preparada como `FightPVP` depois de `PROFILE`/`READY`. Depois da criação da arena, o Android envia `FIGHT_READY`; em NAT de emulador, o relay espera os dois clientes, emite `START` e consome os dois `START_ACK`. No protótipo 77, P1 usa `index=0` e permanece à esquerda nas duas telas; P2 usa `index=1` e permanece à direita. O hook segue `FightScene -> PreFight -> ViewerFight -> PlayerLifeBar` e envia cronômetro, barras esquerda/direita e contadores de frames em `STATE` a 20 Hz. Em duas VMs Android 15, o frame pacing manteve a deriva em 8–9 frames depois de mais de um minuto. A build 88 bloqueia todos os callbacks do planejador da IA remota, inclusive o primeiro que a build 87 ainda liberava. Isso elimina o movimento autônomo, mas deixa o cronômetro em 150 e a apresentação inicial ativa. Sem peer, a luta permanece no modo original. Ainda faltam uma transição de início independente da IA e autoridade consistente de modelo/animação, câmera, colisão, vida, rounds e resultado; portanto, o APK ainda não deve ser apresentado como multiplayer final.

## Configuração Android

O arquivo fica em:

`/sdcard/Android/data/com.sf2.de/files/sf2lan.properties`

Para transformar o segundo Android em controle do jogador 2, use o modo cliente e
adicione `controller=true`. O overlay exibe um joystick e os botoes de combate e
transmite o estado aproximadamente 30 vezes por segundo:

```properties
mode=client
peer=192.168.0.10
port=24190
token=troque-este-token
name=Jogador-2
controller=true
```

No anfitriao, omita a propriedade ou use `controller=false`. Esse modo permanece
como diagnostico de transporte e entrada; ele nao produz uma luta PVP completa.

O modo em desenvolvimento para dois jogos completos usa os controles originais do
Shadow Fight 2. Em cada aparelho, configure `nativeControls=true` e nao ative
`controller`. O hook observa o `GamePad 0` sem alterar seu retorno, transmite essa
entrada e continua aplicando a entrada recebida ao `GamePad 1`:

```properties
mode=host
port=24190
token=troque-este-token
name=Jogador-1
nativeControls=true
```

No segundo aparelho, use a mesma propriedade com `mode=client` e o IP do anfitriao.
O overlay criado no prototipo 13 permanece apenas como diagnostico por meio de
`controller=true` e nao faz parte da interface final.

Exemplo de host:

```properties
mode=host
port=24190
token=troque-este-token
name=Jogador-1
```

Exemplo de cliente:

```properties
mode=client
peer=192.168.1.50
port=24190
token=troque-este-token
name=Jogador-2
```

Os dois lados precisam usar a mesma porta e o mesmo token. Depois de alterar o arquivo, o aplicativo deve ser encerrado e iniciado novamente.

O estado atual é gravado em:

`/sdcard/Android/data/com.sf2.de/files/sf2lan-status.json`

O campo `nativeHook` informa se os leitores do `GamePad 1` foram redirecionados com sucesso.

## Diagnóstico no computador

Entre na pasta do componente antes de executar o módulo Python:

```powershell
Set-Location .\lan-multiplayer
```

Para iniciar um host de referência:

```powershell
python -m lan_bridge host --token troque-este-token --port 24190 --name Meu-PC
```

Para completar automaticamente `PROFILE`/`READY` durante um teste com um unico
Android, use `--mirror-profile`. O peer devolve o perfil recebido; isso valida o
handshake, mas nao substitui o teste futuro com um segundo Android:

```powershell
python -m lan_bridge host --token troque-este-token --port 24190 --name Meu-PC --mirror-profile
```

Para provar visualmente que o perfil remoto nao e apenas uma copia do perfil local,
o diagnostico pode substituir somente o conjunto devolvido. O exemplo abaixo recebe o
perfil do Android, devolve os equipamentos do Lynx e completa `READY`:

```powershell
python -m lan_bridge host --token troque-este-token --port 24190 --name Meu-PC --mirror-profile --mirror-equipment lynx
```

Os valores aceitos correspondem aos presets do gerenciador de save. Essa opcao e
somente de diagnostico e exige `--mirror-profile`.

Para iniciar um cliente de referência:

```powershell
python -m lan_bridge client --peer 192.168.1.50 --token troque-este-token --port 24190 --name Meu-PC
```

Para enviar um unico estado assim que a conexao for aceita:

```powershell
python -m lan_bridge host --token troque-este-token --send-input 0.75 -0.25 0x05
```

Para um diagnostico de golpes repetidos, `--input-pulse-hz` alterna os bits de
botao entre pressionado e solto sem alterar os eixos. Ele exige duracao positiva:

```powershell
python -m lan_bridge host --token troque-este-token --send-input 0 0 0x01 `
  --input-duration 30 --input-hz 30 --input-pulse-hz 1.5
```

Esse modo existe apenas para identificar transicoes internas durante a engenharia
reversa. Ele nao substitui os controles nativos nem representa gameplay final.

Nesse exemplo, `0x05` pressiona soco e arma de longo alcance. O arquivo
`sf2lan-status.json` mostra os eixos, botoes, sequencia e horario do ultimo
comando aceito pelo Android. O receptor ignora sequencias repetidas ou
atrasadas e volta ao estado neutro quando a sessao e encerrada.

Esse utilitário serve para validar rede, firewall, token e heartbeat sem depender ainda do combate.

### Relay para emuladores

Quando dois emuladores ficam isolados por NAT, ambos podem usar o computador como
endereco `peer`. O relay aceita exatamente dois jogadores e encaminha os pacotes de
sessao e entrada sem alterar o conteudo:

```powershell
python -m lan_bridge relay --token troque-este-token --port 24190 --name Meu-PC
```

Alguns emuladores apresentam a mesma origem UDP para mais de uma VM. Nesse caso, use
uma porta de entrada por instancia:

```powershell
python -m lan_bridge relay --token troque-este-token --port 24190 --second-port 24191 `
  --name Meu-PC --auto-start-delay-ms 2000
```

O primeiro Android usa `port=24190` e o segundo usa `port=24191`.
`--auto-start-delay-ms` faz o relay assumir a autoridade de início: ele aguarda
`FIGHT_READY` dos dois endpoints, envia o mesmo `roundId`/atraso aos dois e confirma
os dois `START_ACK`. Sem essa opção, o relay permanece genérico e apenas encaminha
os pacotes. Em celulares na mesma rede local, o modo direto `host`/`client` continua
sendo o caminho preferido.

## Formato binário v1

Todos os inteiros usam ordem de bytes de rede, big-endian.

| Campo | Tamanho | Descrição |
| --- | ---: | --- |
| Magic | 4 bytes | ASCII `SF2L` |
| Versão | 1 byte | `1` |
| Tipo | 1 byte | Tipo da mensagem |
| Payload | 2 bytes | Tamanho do payload |
| Sequência | 4 bytes | Contador `uint32` |
| Timestamp | 8 bytes | Unix em milissegundos |
| Sessão | 16 bytes | Primeiros 16 bytes de `SHA-256(token)` |

Tipos definidos:

- `1`: `HELLO`;
- `2`: `WELCOME`;
- `3`: `PING`;
- `4`: `PONG`;
- `5`: `INPUT`;
- `6`: `DISCONNECT`;
- `7`: `PROFILE`;
- `8`: `READY`;
- `9`: `FIGHT_READY`;
- `10`: `START`;
- `11`: `START_ACK`;
- `12`: `STATE`;
- `13`: `PLAYER_ROLE`;
- `14`: `FIGHT_CONFIG`;
- `15`: `FIGHT_AUTHORITY`;
- `16`: `MATCH_CONFIG`;
- `17`: `MATCH_CONFIG_ACK`.

O payload de `INPUT` possui eixo horizontal `float32`, eixo vertical `float32` e máscara `uint16` de botões. Os bits atuais são soco, chute, arma de longo alcance, magia, habilidade de chefe e pausa.

O payload de `PROFILE` começa com versão `uint8` e nível `uint8`. Em seguida,
cada texto usa tamanho `uint8` e UTF-8, na ordem: nome, avatar, voz, esqueleto,
armadura, elmo, arma, arma de longo alcance e magia. Cada texto aceita no máximo
96 bytes. Recursos, moedas e progresso da campanha nunca fazem parte desse pacote.

`READY` tem payload vazio. Os perfis são retransmitidos até cada lado receber a
confirmação do outro. `sessionReady` no status só fica verdadeiro quando ambos
possuem e confirmaram o perfil adversário.

A sessão também exige `MATCH_CONFIG`: versão `uint8`, tier `uint8`, seed `uint64`,
tamanho do preset P1 `uint8`, tamanho do preset P2 `uint8` e os dois nomes UTF-8.
O fingerprint é formado pelos primeiros oito bytes de `SHA-256(payload)`. Cada
Android aplica o preset correspondente ao seu `PLAYER_ROLE` e responde com
`MATCH_CONFIG_ACK`, cujo payload é esse fingerprint `uint64`. `sessionReady` só fica
verdadeiro quando perfis e configuração de partida foram aplicados e confirmados.

No modo direto, o host cria a configuração. No modo relay coordenado, o relay espera
os dois `PROFILE`, usa o menor nível (limitado a 1..52), sorteia P1/P2 de forma
independente e envia a mesma configuração aos dois clientes. Presets repetidos são
válidos. A versão atual aplica armadura, capacete, arma, arma de longo alcance e
magia; o tier ainda não altera o campo interno de nível do perfil IL2CPP.

`FIGHT_READY` também tem payload vazio, mas só é enviado depois que o hook nativo
termina de criar a arena e os dois lutadores. `START` contém um identificador de
rodada `uint32` e um atraso `uint32` em milissegundos. O cliente agenda esse início
no relógio local e responde com `START_ACK`, cujo payload contém o mesmo identificador.
O host retransmite `START` a cada 250 ms até receber a confirmação, sem reagendar
duplicatas no cliente. A propriedade opcional `startDelayMs` aceita valores de 500
a 10000 e usa 2000 por padrão.

O payload de `STATE` possui 40 bytes: identificador da rodada `uint32`, contador
local `uint32` e oito valores `float32`, nesta ordem: segundos restantes, frames
restantes, vida esquerda, barra de impacto esquerda, vida direita, barra de impacto
direita, frames transcorridos e total de frames da rodada. O emissor publica esse
snapshot a aproximadamente 20 Hz.
O receptor valida tamanho, rodada e ordenação, guarda o estado mais recente e o
expõe em `sf2lan-status.json`. A seção `comparison` compara esquerda com esquerda e
direita com direita, preservando P1/P2 em ambos os aparelhos. Ela informa idade do
snapshot, deltas de frames e vida e `withinTolerance`; as tolerâncias atuais são 12
frames, `0.01` de vida e 500 ms de idade. O frame pacing usa somente
`frames transcorridos`: se o estado remoto tiver no máximo 500 ms e a simulação
local estiver mais de oito frames adiante, o próximo `FixedUpdate` é adiado até o
peer alcançar. Vida e posição ainda não são escritas no lutador; isso evita
confundir limitação de deriva de relógio com sincronização de combate concluída.

O token impede que salas diferentes aceitem pacotes umas das outras, mas não oferece criptografia. Esta primeira versão deve ser usada apenas em uma rede local confiável.
