# p147 — medição das ações executadas

Base: checkpoint de equipamentos `77a710f`, documentação `c82983e`.
Teste: 2026-11-28, somente MuMu `emulator-5554` e `emulator-5556`.

## Resultado e limites

A p147 acrescenta um observador somente de leitura e uma comparação automática.
O transporte e a seleção nativa de ações continuam os da p146. Não foi
implementado um novo protocolo de ações autoritativas nesta etapa.

No ensaio isolado, cada jogador recuou, saltou e atacou com a Espada Ninja.
As duas simulações escolheram a mesma sequência de ações e os mesmos arquivos
de animação para cada jogador. A IA permaneceu desativada; a auditoria dos
equipamentos continuou com `match=1 left=7 right=7` nos dois processos.

| Jogador | Ações comparadas por instância | Maior diferença de início |
| --- | --- | --- |
| P1, esquerda | 9 transições | 1 frame |
| P2, direita | 9 transições | 2 frames |

Isso confirma a reprodução do recuo, salto vertical e ataque simples no ensaio.
Não prova reconciliação espacial, fase exata da animação, ataques simultâneos,
colisão, dano, KO ou resistência à perda de pacotes. A etapa 1 do playbook ainda
precisa de autoridade de ações e validação desses comandos sob divergência;
não deve ser marcada como integralmente concluída por este resultado.

## Descoberta IL2CPP

- `Fighter+0x290`: controlador `FOEDLNHCMNF`.
- `FOEDLNHCMNF.ICGMKPFLAKM`, RVA `0x347E334`: recebe a ação `IADCIOMJMEI`,
  parâmetro inteiro e flags; prepara os frames e retorna sucesso/falha.
- Em `0x347E588`, esse método limpa `controller+0x88`.
- Em `0x347E608`, grava a ação recebida em `controller+0x158`.
- `IADCIOMJMEI+0x68`: nome da ação; `+0x70`: arquivo de animação.
- O getter `0x347F6A8` lê `+0x88`; não representa a ação em execução neste teste.
  A primeira tentativa de observação desse campo permaneceu vazia e foi corrigida.

A leitura ocorre após `FightScene.FixedUpdate`, no thread do jogo, somente
durante luta LAN. Registra alterações de ponteiro da ação, com jogador, sequência
e tick. Não conta reinícios da mesma ação quando o ponteiro permanece igual e
não mede avanço interno de frames. Essa limitação importa antes de transformar
o observador em emissor de eventos de rede.

## Reprodução

Com as duas instâncias prontas no duelo, aguardar a apresentação terminar.
Executar um comando de cada vez, deixando voltar à postura de descanso:

1. P1: recuar com o analógico superior, saltar verticalmente, atacar uma vez.
2. P2: recuar com o analógico inferior, saltar verticalmente, atacar uma vez.
3. Capturar o log `SF2LAN-NATIVE` do PID atual de cada jogo, em arquivos separados.

Evidências preservadas: [P1](evidence/p147-actions-p1.txt) e
[P2](evidence/p147-actions-p2.txt). No ensaio salvo, o recuo de P1 foi repetido
depois do ataque porque a primeira tentativa ocorreu durante a apresentação.

Executar a partir de `lan-multiplayer`:

```powershell
python -m lan_bridge.action_audit docs/evidence/p147-actions-p1.txt docs/evidence/p147-actions-p2.txt
python -m unittest discover -s tests -q
```

O comparador exige os dois jogadores, as três ações, a sequência completa igual
e diferença máxima de oito frames. Rejeita logs vazios, ações ausentes, papéis
invertidos e sequências reiniciadas/misturadas. Os oito frames são um limite
diagnóstico explícito, não uma garantia de qualidade de rede.

## Artefatos e retomada

Biblioteca testada: `tmp/libsf2lanhook-p147-action-observer.so`.
SHA-256: `2181E66960407B22C5128D0A856BAB7EABA481726BE0119E099C905195AE7C6B`.
Somente essa biblioteca foi substituída nas instalações MuMu. Nenhum APK
completo novo foi gerado. APK e users.xml originais preservados.

Próximo recorte: instrumentar a entrada/saída de `0x347E334` para capturar
reinícios da mesma ação, parâmetros e retorno; então transportar eventos
identificados por jogador, rodada e sequência. Resolver ações por ID local,
nunca copiar ponteiros entre processos. Validar a aplicação no thread do jogo
sem duplicar a seleção decorrente dos comandos de entrada existentes.
