# p148 — seleção de ações autoritativa por jogador

Checkpoint parcial da etapa 1, testado em 28/11/2026. Não representa multiplayer
completo nem sincronização de fase, posição, colisões, dano ou rounds.

## Mudança efetiva

A p147 apenas observava mudanças de ponteiro. A p148 intercepta o seletor
`FOEDLNHCMNF.ICGMKPFLAKM`, RVA `0x347E334`, e publica cada seleção que retorna
sucesso, incluindo reinícios da mesma ação. Assinatura usada: controller,
action, int, bool, bool, int, MethodInfo; retorno bool.

O dono de P1 é a instância superior e o dono de P2 é a inferior. O receptor
resolve o nome na própria lista `Fighter+0x3A0` e chama o seletor original no
thread da simulação, antes do FixedUpdate original. Nunca transporta ponteiros.
O controlador fica em `Fighter+0x290`, a ação selecionada em controller+0x158,
e seu nome em action+0x68. Depois da primeira aplicação bem-sucedida, novas
seleções locais do lutador remoto são recusadas: ele passa a receber as decisões
do dono, não uma segunda decisão independente baseada apenas no input.

Pacotes novos: ACTION=18 e ACTION_ACK=19, sob as verificações de sessão existentes.
ACTION contém roundId big-endian seguido de ASCII:
`sequence,tick,parameter,flag1,flag2,last_parameter,name`.
ACK contém roundId e sequência, ambos de 32 bits big-endian.
Fila de 128 eventos; somente o primeiro não confirmado é retransmitido.
O receptor aceita apenas a próxima sequência e confirma depois da aplicação.
Duplicatas não reiniciam a animação. Parâmetros de seleção são preservados.

## Evidência do ensaio

APK p148 instalado com sucesso nos dois MuMu. Backup anterior em
`backups/mumu-before-p148-actions/`. Poco não acessado; APK e users.xml
originais não alterados.

Logs completos do PID de cada jogo, um duelo por captura:
[P1](evidence/p148-actions-p1.txt), [P2](evidence/p148-actions-p2.txt).
Intervalo do ensaio de comandos: aproximadamente 10:24:52–10:25:55 nos logs.

- P1 publicou 40 eventos; P2 aplicou exatamente os mesmos 40, em ordem.
- P2 publicou 39 eventos; P1 aplicou exatamente os mesmos 39, em ordem.
- Cada jogador executou KatanaStepBack, JumpUp e três KatanaSlash.
- Os registros `LAN action selected` também mostram as ações nos controladores
  dos receptores; não se trata apenas de pacotes recebidos no Java.
- Os analógicos foram exercitados separadamente, assim como os botões de ataque.
- Os logs mostram IA suprimida e StartFight sem liberação dos planejadores.
- As capturas `tmp/p148-fight-p1.png` e `tmp/p148-fight-p2.png`, inspecionadas,
  mostram os quatro Shadows com espada e postura de espada, frente a frente.
  São capturas sequenciais em repouso, não prova visual de igualdade de fase.
- 45 testes Python passaram, incluindo repetição de evento/ACK no relay e
  rejeição de duplicatas, lacunas e nomes divergentes pelo auditor de evidência.

Reproduzir o auditor a partir de `lan-multiplayer`:

```powershell
python -m lan_bridge.action_delivery_audit docs/evidence/p148-actions-p1.txt docs/evidence/p148-actions-p2.txt
python -m unittest discover -s tests -q
```

O auditor exige todas as sequências completas, nomes e ticks de origem iguais,
sem descartes silenciosos de cauda. Não compara tick local com tick remoto como
se fossem um relógio comum. O comparador p147 de skew continua disponível, mas
seu requisito de oito frames NÃO foi satisfeito por este ensaio: há diferença
de aproximadamente 110 ticks entre simulações após uma pausa local.

## Limites e próximo passo

A transmissão/aplicação está provada neste ensaio local, não sob perda/jitter
injetados. O protocolo usa stop-and-wait; uma ação não resolvida na lista ou
recusada indefinidamente pelo seletor bloqueará as seguintes. Overflow registra
erro e exige reiniciar o duelo; falta recuperação de sessão e diagnóstico
detalhado para esse caso. Usar somente duas builds compatíveis p148.

A ação ainda começa ao chegar, sem compensar a idade do evento. Não afirmar que
a deriva acabou: o próximo recorte é medir/corrigir fase e avanço da animação,
com pausa/atraso controlado, sem mudar dano, vida, rounds ou interface. Não usar
simples cópia de ponteiros ou writes especulativos para corrigir fase.
Transições de round, reconexão, combos, contato físico e reações não foram
validados aqui. A padronização de equipamento p146 foi preservada.

## Artefatos

APK: `dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-148.apk`.
SHA-256: `439E059910BDE72E2548780FF377B6765CD2FB0A361791B49B04621BD403E1E0`.
Biblioteca instalada nas duas instâncias:
`A31A5976D922D036646C427DAD75A5975FB0DE51BD9641CA49445DB88352838C`.
O APK p146 permanece como referência anterior.

O repack incremental agora aceita `-RebuildBridge` para recompilar também
classes8.dex nas bases p141/p146 verificadas por hash. A pasta temporária de
build é limpa pelo script; não foi mantida outra árvore de APK decompilado.
