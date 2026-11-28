# p150 — compensação do início atrasado de ações

28/11/2026. Implementação parcial, não apenas observação. Corrige o frame inicial
de três ações atrasadas da configuração canônica atual. **Não conclui a etapa
de sincronização:** a réplica ainda pode esperar no fim da ação pela seguinte.

## Reprodução e hipótese confirmada

O relay agora aceita `--action-delay-ms 600`: enfileira somente ACTION por
600 ms sem bloquear STATE, INPUT, ACK e heartbeat. Fila limitada, limpa no
reset de luta; destinatários ausentes não recebem dados antigos. Desabilitado
por padrão. Dois testes cobrem isolamento do atraso e descarte no reset.

Com a biblioteca p149 (controle, sem compensação), P1 publicou KatanaSlash às
10:46:48.749 e a réplica iniciou às 10:46:49.407. P2 publicou às 10:46:51.838
e a réplica iniciou às 10:46:52.517. Os timestamps são dos relógios dos MuMu,
não uma medição calibrada de latência entre aparelhos físicos. A injeção e os
frames observados confirmam o início atrasado desde o começo da animação.

## Correção

O evento registra também o relógio de simulação do dono (framesPassed), o frame
inicial efetivo e o período validado da ação. No receptor:

`idade = framesPassed mais recente do dono - framesPassed na seleção`

`frame inicial corrigido = frame inicial do dono + floor(idade / período)`

Nenhuma subtração entre relógios de parede de aparelhos diferentes ou entre
os contadores fight_state_tick. O relógio de parede local serve apenas para
verificar a idade do STATE recebido.

A chamada original de `FOEDLNHCMNF.ICGMKPFLAKM` (0x347E334) recebe esse frame no
último parâmetro inteiro. O próprio jogo prepara o controlador e limita ao fim
da ação; não escrevemos diretamente no ObscuredInt ou no Viewer.
`NKPDMCEOGLD` (0x347EABC) verifica o frame efetivamente selecionado.

Restrições intencionais:

- Apenas KatanaSlash, KatanaStepBack e JumpUp da configuração canônica atual.
- Período de três ticks por frame, **medido para essa configuração**, não uma
  regra universal. A tentativa de usar controller+0x144 foi rejeitada: o campo
  vale 1 neste ensaio e não representa esse período.
- Não compensa início relativo (flag2), relógio recebido há mais de 250 ms,
  idade negativa, idade superior a 120 ticks, NaN ou parâmetros desconhecidos.
  Esses casos mantêm o parâmetro original; não prometem recuperar atraso.
- Sem reexecução contínua do seletor; cada evento continua aplicado uma vez.

Formato ACTION p150, após roundId big-endian:
`seq,tick,parameter,flag1,flag2,last_parameter,name,sourceClock,startFrame,framePeriod`.
**Usar p150 nos dois peers.** p148/p149 não entendem o registro ampliado. Não há
negociação de versão ainda; o receptor p150 registra erro se receber formato
inválido. Java continua transportando o registro opaco, sem novos métodos JNI.

## Resultado com atraso artificial

- P1 KatanaSlash: idade38, frame inicial2 → solicitado14 → efetivo14.
- P2 KatanaSlash: idade36, frame inicial2 → solicitado14 → efetivo14.
- P1 KatanaStepBack: solicitado17 → efetivo16, limitado pelo seletor nativo.
- P2 KatanaStepBack: solicitado15 → efetivo15.
- P1 JumpUp: solicitado14 → efetivo14; P2: solicitado16 → efetivo16.
- P1 seq5: dono tick564/frame14 e réplica tick564/frame14; os registros próximos
  mostram ambos avançando para16,18,20,22,24. Isso prova aproximação da fase
  após a chegada neste ensaio, não igualdade de simulação física ou de relógios.
- Depois a réplica ficou em frame27 esperando o próximo evento: limite ainda
  aberto, explicitamente observado. Não esconder isso como sincronização total.

Evidências: `evidence/p150-before-emulator-5554.txt` e `5556.txt` (controle),
`evidence/p150-b-full-emulator-5554.txt` e `5556.txt` (compensação).
Os arquivos preservam publicação, aplicação, fase dos três movimentos e IA.

## Validação sem atraso e artefatos

Relay reiniciado sem injeção. Biblioteca final nos dois MuMu. Recuo, salto e
ataque exercitados separadamente por P1 e P2. Auditor: 15 ações de P1 e16 de P2,
todas aplicadas uma vez em sequência. Logs `evidence/p150-normal-emulator-*.txt`.
IA suprimida; screenshots locais `tmp/p150-final-emulator-*.png` inspecionadas
mostram Shadow, espada e postura corretas nos quatro avatares. Capturas em
repouso não constituem prova de colisões ou igualdade espacial.

52 testes Python passaram. A regra C utilizada pelo hook foi compilada em
executável ARM64 e executada no MuMu: 13 asserções passaram. Incluem fallback
para valores inválidos, estado velho e ações não elegíveis.

```powershell
# A partir de lan-multiplayer:
python -m unittest discover -s tests -q
python -m lan_bridge.action_delivery_audit docs/evidence/p150-normal-emulator-5554.txt docs/evidence/p150-normal-emulator-5556.txt
```

Teste nativo: `tests/native/test_action_timing.c`, compilado com o clang do NDK
para android24 e executado em `/data/local/tmp/test-action-timing` do MuMu.
Não usa uma cópia Python da fórmula: inclui o mesmo header do hook.

Biblioteca final (instalada e embutida no APK):
`8F984E0C56DFE805820E0C94045A2049D8216A1FCBC5DC24D69B1EDD0E67E8CD`.
APK novo: `dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-150.apk`.
SHA256: `49E6CFE1715E5BA54734ABA1AA59B00769E7D062F97B87D8C3D272ACDE34AC78`.
Assinaturas v2/v3 verificadas; biblioteca extraída do ZIP tem o mesmo hash
instalado nos emuladores. Intermediários grandes da build removidos pelo script.
Base p148 preservada. Não houve alteração do APK original, users.xml original
ou gerenciador de saves. Poco não acessado. Sem mudanças em dano/vida/rounds/UI.

## Próximo recorte

Evidência adicional do usuário: `C:/Users/barbo/Downloads/Design sem nome (1).png`,
com linhas verticais comparando as telas (ambas mostram 135 no cronômetro).
Os dois lutadores aparecem deslocados para a esquerda na tela inferior; o
cenário também aparenta deslocamento. Isso confirma diferença visual, mas não
isola divergência das coordenadas físicas de deslocamento/zoom da câmera.
Antes de corrigir posição, comparar o mesmo jogador por ID nas duas instâncias:
posição física de origem, posição recebida/aplicada, transformação do Viewer,
câmera e projeção normalizada. Comparar também a distância física P1–P2, que
independe da translação da câmera. Não converter diretamente pixels das linhas
vermelhas em coordenadas de mundo ou introduzir um offset fixo por tela.

Tratar conclusão/transição de ação atrasada sem liberar IA nem repetir ataques.
Ainda faltam recuperação contínua de fase, reconciliação física, teste de root
motion e callbacks sob seek, perda/jitter reais e negociação de protocolo.
Os testes foram de movimentos isolados, sem contato: não afirmam que pular
frames preserve todos os eventos de ataque em combate. Não ampliar a lista de
ações antes de validar esses efeitos.
