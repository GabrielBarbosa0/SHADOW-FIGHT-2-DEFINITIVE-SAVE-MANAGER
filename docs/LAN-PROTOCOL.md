# Protocolo SF2 LAN

## Marco atual

O protótipo 13 acrescenta uma ponte UDP, um hook ARM64 e um controle multitouch ao processo Android do jogo. Ele já executa:

- configuração de host ou cliente;
- autenticação por token compartilhado;
- handshake `HELLO` / `WELCOME`;
- heartbeat `PING` / `PONG`;
- detecção de desconexão;
- serialização do estado de movimento e botões;
- validação, ordenação e armazenamento dos pacotes `INPUT` no Android;
- neutralização automática da entrada quando a conexão expira;
- aplicação dos eixos e botões ao `GamePad 1` do PVP;
- captura de joystick e botões num segundo Android em modo controle;
- transmissão da entrada a aproximadamente 30 Hz, com retenção de toques curtos;
- status legível fora do processo do jogo.

A ponte Android e a implementação de referência em Python usam o mesmo protocolo. O teste MuMu-to-PC foi concluído com conexão e heartbeat contínuo, sem falha fatal no jogo.

Movimento e ataque do jogador 2 já funcionam por LAN numa única simulação de combate, e o segundo Android já pode fornecer esses comandos. Ainda falta sincronizar a luta entre duas simulações; portanto, o protótipo 13 ainda não representa um duelo completo renderizado nos dois celulares.

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

No anfitriao, omita a propriedade ou use `controller=false`. Nesta etapa, somente
o anfitriao executa e mostra a luta; o segundo aparelho funciona como controle LAN.

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

Para iniciar um host de referência:

```powershell
python -m lan_bridge host --token troque-este-token --port 24190 --name Meu-PC
```

Para iniciar um cliente de referência:

```powershell
python -m lan_bridge client --peer 192.168.1.50 --token troque-este-token --port 24190 --name Meu-PC
```

Para enviar um unico estado assim que a conexao for aceita:

```powershell
python -m lan_bridge host --token troque-este-token --send-input 0.75 -0.25 0x05
```

Nesse exemplo, `0x05` pressiona soco e arma de longo alcance. O arquivo
`sf2lan-status.json` mostra os eixos, botoes, sequencia e horario do ultimo
comando aceito pelo Android. O receptor ignora sequencias repetidas ou
atrasadas e volta ao estado neutro quando a sessao e encerrada.

Esse utilitário serve para validar rede, firewall, token e heartbeat sem depender ainda do combate.

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
- `6`: `DISCONNECT`.

O payload de `INPUT` possui eixo horizontal `float32`, eixo vertical `float32` e máscara `uint16` de botões. Os bits atuais são soco, chute, arma de longo alcance, magia, habilidade de chefe e pausa.

O token impede que salas diferentes aceitem pacotes umas das outras, mas não oferece criptografia. Esta primeira versão deve ser usada apenas em uma rede local confiável.
