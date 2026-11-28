# p149 — medição da fase real, sem correção especulativa

Investigação de 28/11/2026 sobre a p148. Esta etapa adiciona instrumentação
opcional e análise reproduzível; **não implementa reconciliação de fase**.
Não confundir este checkpoint de diagnóstico com uma correção da deriva.

## Engenharia reversa

- `FOEDLNHCMNF.NKPDMCEOGLD`, RVA `0x347EABC`, retorna o frame corrente.
  Exige ação válida em controller+0x158. Para ações sem a flag action+0x8C,
  decodifica o ObscuredInt em controller+0x48 e adiciona max(controller+0x70-2,0).
  Não é correto ler +0x48 como um inteiro simples.
- No seletor `0x347E334`, w5 é preservado em w23. Em `0x347E5E0`, valor
  negativo usa o começo definido pela ação; em `0x347E610` é convertido em
  ObscuredInt e gravado em +0x48. Existe também caminho relativo quando flag2
  está ativa e limitação ao fim da ação. Isso ainda não prova que saltar frames
  pelo seletor preserve callbacks, root motion e todas as janelas de ataque.
- `0x347EB04` altera o sinal da orientação em controller+0x34; NÃO é seek.
- `0x3480AE4` armazena argumento inteiro em +0x144 e seu inverso em +0x110.
  Não foi utilizado para alterar velocidade.

## Implementação de diagnóstico

`SF2LAN_TRACE_ACTION_PHASE` habilita leitura no thread do jogo após FixedUpdate,
a cada seis ticks. Registra papel, jogador, sequência autoritativa, tick, frame
real e nome. Não escreve em estruturas IL2CPP. Sem a definição, não há código
de medição no binário. `repack-lan-native.ps1 -TraceActionPhase` habilita a opção.

O analisador `lan_bridge.action_phase_audit` separa ações por sequência e mede
frames por tick. Não declara sucesso de sincronização: não há relógio comum
provado entre processos e taxa igual não implica fase/posição igual.

## Testes executados

Usados somente emulator-5554 e emulator-5556. Substituição temporária da .so,
sem reinstalar APK, editar saves ou acessar o Poco. Testados golpe de espada,
recuo e salto separadamente nos dois controles.

Também suspendemos somente o processo do jogo inferior com SIGSTOP e retomamos
com SIGCONT em `finally`, após espera solicitada de 600 ms e comando de ataque
no superior. A duração total inclui comandos ADB: não é emulação calibrada de
latência de rede. O pacing existente segurou o avanço e Unity executou updates
de recuperação após retomar. Não tratar isso como ensaio de perda de pacotes.

Evidências filtradas, sem linhas de idle repetitivas:
[superior](evidence/p149-phase-emulator-5554.txt),
[inferior](evidence/p149-phase-emulator-5556.txt).
Logs integrais locais: `tmp/p149-full-emulator-5554.log` e equivalente 5556.

- KatanaSlash P2 seq=4: nos dois processos os frames 2,4,...,24 aparecem com
  intervalo de seis ticks; no intervalo amostrado, 22 frames em 66 ticks.
- Após suspensão, KatanaSlash P1 seq=14: superior tick1392/frame4;
  inferior tick1392/frame5. As amostras seguintes mantêm diferença de cerca
  de um frame neste trecho. Os ticks não são garantia de mesmo instante real.
- No superior: JumpUp P1 seq52 avançou 25 frames em78 ticks; P2 seq51,
  24 frames em72 ticks. A discreta diferença inclui amostragem e clamp final.
- Recuos e ataques não apresentaram frame reverso nas amostras analisadas.
- Os controles continuaram produzindo seleções autoritativas e os registros
  de supressão da IA permaneceram presentes.
- 50 testes Python passaram. Build nativa com e sem a opção compilou.

Conclusão limitada: a diferença de ~110 ticks observada na p148 não pode ser
convertida diretamente em ~110 frames de animação. A medição deste ensaio não
reproduziu uma grande deriva de fase; tampouco prova que ela não exista.

## Artefatos e retomada

Probe: `tmp/libsf2lanhook-p149-phase-probe.so`.
SHA256 `03E8C62A09CB1BC6502E5D2940D92037421F5D2A7476A5B7E164741B0EE48585`.
Build sem diagnóstico: hash idêntico à p148,
`A31A5976D922D036646C427DAD75A5975FB0DE51BD9641CA49445DB88352838C`.
Ao encerrar os testes, restaurada a biblioteca p148 nos dois MuMu.
Nenhum APK de ~1 GB adicional foi gerado; originais preservados.

Reprodução a partir de `lan-multiplayer`:

```powershell
python -m lan_bridge.action_phase_audit docs/evidence/p149-phase-emulator-5554.txt
python -m lan_bridge.action_phase_audit docs/evidence/p149-phase-emulator-5556.txt
python -m unittest discover -s tests -q
```

Próximo recorte: injetar atraso apenas nos pacotes ACTION (sem bloquear STATE,
heartbeat ou o processo inteiro), mantendo a instrumentação, para separar
atraso de entrega de ação e pacing da simulação. Depois medir idade/seq/frame
do dono e da réplica antes de escolher seek ou avanço incremental. Não usar
um fator fixo de três ticks para todos os golpes nem reiniciar o seletor a
cada snapshot: isso pode repetir callbacks e criar regressões.
