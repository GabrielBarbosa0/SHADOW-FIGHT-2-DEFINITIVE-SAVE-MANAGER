# Protocolo SF2 LAN

## Marco atual

O protótipo 7 acrescenta uma ponte UDP ao processo Android do jogo. Ela já executa:

- configuração de host ou cliente;
- autenticação por token compartilhado;
- handshake `HELLO` / `WELCOME`;
- heartbeat `PING` / `PONG`;
- detecção de desconexão;
- serialização do estado de movimento e botões;
- status legível fora do processo do jogo.

A ponte Android e a implementação de referência em Python usam o mesmo protocolo. O teste MuMu-to-PC foi concluído com conexão e heartbeat contínuo, sem falha fatal no jogo.

Ainda falta conectar os pacotes `INPUT` recebidos ao objeto `PvpScene.PlayerTwoInput`. Portanto, o protótipo 7 valida o transporte de rede, mas ainda não é uma partida jogável por LAN.

## Configuração Android

O arquivo fica em:

`/sdcard/Android/data/com.sf2.de/files/sf2lan.properties`

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

## Diagnóstico no computador

Para iniciar um host de referência:

```powershell
python -m lan_bridge host --token troque-este-token --port 24190 --name Meu-PC
```

Para iniciar um cliente de referência:

```powershell
python -m lan_bridge client --peer 192.168.1.50 --token troque-este-token --port 24190 --name Meu-PC
```

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
