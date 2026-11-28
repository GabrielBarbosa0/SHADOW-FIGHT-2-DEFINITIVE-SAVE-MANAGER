# Playbook para concluir o multiplayer LAN

Este documento transforma as pendências do protótipo LAN em oito etapas
independentes. Ele também define como alternar entre um modelo de raciocínio
mais forte para engenharia reversa e um modelo médio para integração, build,
testes, documentação e commit.

O objetivo não é apenas fazer uma demonstração visual. A etapa somente termina
quando o mesmo evento pode ser reproduzido nas duas instâncias, os logs
confirmam o estado esperado e existe um commit que permita retornar ao último
marco funcional.

## Ponto de partida confirmado

Atualizações verificadas:

- p149: [medição do frame real](ACTION-PHASE-P149.md), opcional e somente leitura.
  Tick da simulação não é frame de animação. Ensaio com pausa do processo não
  reproduziu grande deriva; correção de fase continua pendente. Runtime p148
  restaurado após a medição, sem novo APK completo.
- p148: [seleção autoritativa de ações](ACTION-AUTHORITY-P148.md). P1 publicou
  40 eventos e P2 publicou 39; cada sequência foi aplicada uma vez no peer.
  Recuo, salto e golpes repetidos testados nos dois MuMu. Checkpoint parcial:
  fase da animação, recuperação de atraso e deriva ainda não estão resolvidas.
- Equipamentos canônicos: `77a710f`, p146; ver
  [relatório de equipamentos](EQUIPMENT-CANONICALIZATION-P146.md).
- p147: [observação das ações reais](ACTION-OBSERVATION-P147.md). Recuo, salto
  e ataque simples coincidiram em ensaio isolado, com diferença de até dois
  frames. Há comparador reproduzível, mas a autoridade de ações da etapa 1
  continua pendente. O observador não muda a simulação nem resolve a deriva.

Referência anterior à padronização de equipamentos:

- Commit de referência: `57e06ae` (`Restore split LAN fighter authority`).
- APK de referência: `dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-141.apk`.
- SHA-256: `C9C9C7AF1235BB5BF2B42A16B376946D75655016EE32DB53F7EBEE74594CEE5C`.
- O analógico superior controla o lutador esquerdo nas duas telas.
- O analógico inferior controla horizontalmente o lutador direito nas duas telas.
- A autoridade espacial está dividida entre P1 e P2.
- A IA autônoma permanece desativada durante a luta LAN.
- Handshake, início coordenado, transporte de entrada e troca de estado funcionam.
- A build continua experimental: animação, equipamento, dano, rounds e resultado
  ainda não formam uma simulação única e autoritativa.

## Estratégia de modelos e checkpoints

As etapas 1 a 6 contêm o trabalho de engenharia reversa mais difícil e são boas
candidatas para uma sessão curta e delimitada no Astra. As etapas 7 e 8 são
principalmente produto, automação e validação e devem começar no Sol médio.

Cada sessão deve obedecer a estas regras:

1. Trabalhar somente na etapa pedida. Não antecipar as etapas seguintes.
2. Começar do commit funcional indicado neste documento ou do último commit
   aprovado, nunca de uma build sem procedência.
3. Não acessar, instalar ou testar no POCO X7 Pro sem autorização expressa.
   Usar somente as duas instâncias do MuMu durante o desenvolvimento normal.
4. Nunca modificar o APK original nem o `users.xml` original. Toda alteração é
   feita numa cópia e todo teste que substitua dados exige backup.
5. Evitar reconstruir APKs de aproximadamente 1,1 GiB durante a investigação.
   Primeiro provar offsets, hooks e protocolo com testes menores e logs.
6. Não tratar o bot original, a prévia do dojo ou dois personagens visíveis como
   prova de controle multiplayer.
7. Registrar offsets, métodos, hipótese, evidência positiva e evidência negativa.
8. Ao obter um resultado funcional, atualizar os documentos, executar os testes,
   remover builds descartadas e criar um commit de checkpoint.
9. Se os créditos terminarem, deixar código compilável, anotações objetivas e um
   resumo do que foi provado. O modelo seguinte não deve refazer toda a análise.

### Prompt de retomada no Sol médio

Use este prompt depois de uma sessão Astra que tenha encontrado ou iniciado a
solução. Substitua os campos entre colchetes:

```text
Continue a etapa [NOME E NÚMERO] do playbook
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

O trabalho de engenharia reversa anterior deixou estas evidências:
[RESUMO CURTO, OFFSETS, MÉTODOS E ARQUIVOS ALTERADOS].

Não amplie o escopo. Revise o diff atual, conclua a implementação iniciada,
compile apenas quando o código estiver consistente, teste exclusivamente nas
duas instâncias do MuMu e compare as duas telas e os logs. Não acesse o POCO X7
Pro. Preserve o APK e o users.xml originais. Se o critério de conclusão da etapa
for atendido, atualize a documentação, remova builds intermediárias descartadas
e crie um commit de checkpoint. Se ainda falhar, documente exatamente a falha e
preserve o último commit funcional.
```

## 1. Sincronização de ações e animações

**Meta:** as duas instâncias devem reproduzir a mesma ação do mesmo lutador, não
somente coordenadas parecidas. O primeiro recorte inclui caminhada, salto e um
ataque simples de cada jogador.

**Modelo recomendado:** Astra para localizar a autoridade IL2CPP; Sol médio para
integração, build, repetição dos testes e commit.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 1 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Parta do último commit funcional e localize, na Definitive Edition ARM64/IL2CPP,
o estado autoritativo de ação e animação dos dois objetos Fighter. Prove quais
campos ou métodos controlam a pose e o moveset do lutador direito sem reativar a
IA. Implemente a sincronização mínima de caminhada, salto e um ataque simples
para P1 e P2 nas duas instâncias do MuMu. Não trabalhe em vida, rounds, interface
ou seleção de equipamento nesta etapa. Não acesse o POCO X7 Pro e não altere os
arquivos originais.

Evite rebuilds completos durante a descoberta. Pare depois de: registrar os
offsets e métodos confirmados; deixar um hook testável; demonstrar por logs que
cada ação recebida foi aplicada ao Fighter correto; e documentar claramente o
que ainda não foi reproduzido. Se houver sucesso visual nas duas telas, prepare
um checkpoint, mas não avance para dano ou rounds.
```

**Critério de conclusão:** P1 e P2 caminham, saltam e executam o ataque escolhido
com pose equivalente nas duas telas; a IA continua desligada.

## 2. Equipamento e moveset canônicos

**Meta:** cada lutador deve ter a mesma arma, armadura, capacete, geometria,
animações e moveset nas duas instâncias. Inicialmente, ambos podem usar o mesmo
conjunto simples do Shadow, sem encantamento, magia ou arma de longo alcance.

**Modelo recomendado:** Astra para identificar caches e cópias internas do
`Fighter`; Sol médio para consolidar presets e testes.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 2 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

A sincronização de entrada e a base de animação já devem estar preservadas.
Localize todos os pontos em que a Definitive Edition copia o equipamento do
Profile para o Fighter, para o Viewer e para o controlador de animação. Descubra
por que uma tela ainda pode mostrar sai, adaga, punhos ou outra geometria enquanto
a outra usa a espada ninja, mesmo quando o perfil negociado é igual.

Implemente um preset canônico temporário para os quatro avatares visuais: Shadow,
mesma arma simples, mesma armadura e mesmo capacete, sem encantamentos, ranged ou
magia, com o moveset correspondente à arma. Não introduza bosses nem sorteio
nesta etapa. Teste somente no MuMu, preserve os originais e não trabalhe em dano,
rounds ou menu. Registre cada cache/ponteiro corrigido e pare ao provar que arma,
pose e ataques coincidem nas duas telas.
```

**Critério de conclusão:** a arma e o conjunto visível de cada lado são idênticos
nas duas instâncias, e nenhuma ação usa a pose de outra classe de arma.

## 3. Combate autoritativo: golpes, dano e vida

**Meta:** um golpe confirmado deve produzir o mesmo dano, reação e valor de vida
nas duas instâncias.

**Modelo recomendado:** Astra para localizar a cadeia de hit/damage/health; Sol
médio para protocolo, testes repetitivos e tratamento de casos simples.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 3 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Com movimento, animação e equipamento já estáveis, descubra a cadeia IL2CPP que
confirma colisão de golpe, bloqueio, dano, reação e vida do Fighter na Definitive
Edition. Defina uma única autoridade de combate adequada para a LAN e impeça que
as duas instâncias apliquem o mesmo golpe duas vezes. Implemente o recorte mínimo:
um ataque simples acertando, um ataque errando e um ataque bloqueado, com vida e
reação idênticas nas duas telas.

Não implemente ainda KO definitivo, troca de round, menu ou reconexão. Não use
IA como substituto do jogador remoto. Teste somente no MuMu, preserve os arquivos
originais e registre offsets, IDs de evento, sequência de pacotes e regras de
deduplicação. Pare quando os três casos tiverem evidência em logs e HUD.
```

**Critério de conclusão:** acerto, erro e bloqueio têm uma única decisão e
produzem a mesma vida e reação nos dois aparelhos, sem dano duplicado.

## 4. Ciclo de rounds, cronômetro, KO e resultado

**Meta:** as duas instâncias devem começar e terminar o mesmo round, mostrar o
mesmo cronômetro e placar e chegar ao mesmo resultado de luta.

**Modelo recomendado:** Astra para mapear a máquina de estados da luta; Sol médio
para integrar mensagens, temporização e testes de repetição.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 4 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Parta do combate autoritativo já funcional. Mapeie a máquina de estados IL2CPP de
início do round, contagem regressiva, pausa, timeout, KO, rounds perdidos, reset
dos Fighters e resultado final. Defina qual peer publica cada transição e faça as
duas instâncias consumirem o mesmo identificador de round e o mesmo evento final.

Implemente e prove três caminhos: KO, vitória por tempo e início do round seguinte.
Não trabalhe em menu de sala ou descoberta de rede. Não reative a IA. Teste
somente no MuMu, preserve os originais e registre as transições e condições que
evitam reset duplo ou uma tela vazia. Pare depois que os três caminhos terminarem
com HUD, placar e estado equivalentes nas duas instâncias.
```

**Critério de conclusão:** cronômetro, KO/timeout, placar, reinício e resultado
final permanecem equivalentes nas duas telas.

## 5. Colisão, direção, troca de lado e câmera

**Meta:** os lutadores devem se encarar, cruzar de lado corretamente, respeitar
colisão e limites e permanecer enquadrados nas duas telas.

**Modelo recomendado:** Astra para separar transformação visual, física e câmera;
Sol médio para ajustar limites e executar a matriz de cenários.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 5 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Com ações e combate já sincronizados, identifique as fontes IL2CPP distintas de
posição física, posição visual, facing, colisão e alvo da câmera. Corrija sem
voltar à movimentação cruzada em X: o analógico superior continua sendo P1 à
esquerda e o inferior continua sendo P2 à direita, nas duas telas.

Implemente e prove aproximação, afastamento, salto por cima/troca de lado, contato
no limite da arena e deslocamento até as duas extremidades. Os lutadores devem se
virar para o adversário e a câmera deve enquadrá-los de forma estável nas duas
instâncias. Não mexa em menu ou presets. Teste somente no MuMu, preserve os
originais e pare com uma tabela de resultados e os campos/métodos confirmados.
```

**Critério de conclusão:** os cinco cenários mantêm lado, facing, colisão e câmera
coerentes nas duas instâncias, sem reaparecer o controle em X.

## 6. Robustez de rede e reconciliação

**Meta:** a partida deve continuar jogável com latência, jitter, perda moderada de
pacotes, pausa curta e reconexão controlada.

**Modelo recomendado:** Astra para desenhar e revisar a reconciliação; Sol médio
para implementar simuladores de falha, métricas e testes prolongados.

**Prompt para o Astra:**

```text
Trabalhe somente na etapa 6 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Revise o protocolo LAN e a autoridade já implementada. Proponha e implemente a
menor camada robusta que preserve a jogabilidade sob atraso, jitter e perda de
pacotes: números de sequência, descarte de estado antigo, buffer curto,
interpolação, correção de divergência e neutralização segura de entrada quando o
peer some. Evite rollback complexo se interpolação e reconciliação limitada forem
suficientes para este protótipo.

Crie testes reproduzíveis para 50/100/200 ms de atraso, jitter, 5% de perda,
pausa do aplicativo e reconexão. Não trabalhe na interface de sala. Teste somente
no MuMu, não acesse o POCO X7 Pro e preserve os originais. Pare depois de deixar
métricas objetivas, limites aceitos e comportamento de falha definido, sem gerar
várias cópias completas do APK durante a investigação.
```

**Critério de conclusão:** nenhuma entrada antiga assume o controle, divergências
são corrigidas suavemente e desconexão/reconexão não corrompe a luta.

## 7. Interface de criar e entrar em sala

**Meta:** o usuário deve iniciar uma partida sem editar propriedades, executar
scripts manuais ou entrar separadamente no modo Duelo em cada instância.

**Modelo recomendado:** Sol médio. Usar Astra somente se surgir um novo bloqueio
de engenharia reversa na navegação nativa do jogo.

**Prompt para o Sol médio:**

```text
Trabalhe somente na etapa 7 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Implemente um fluxo de usuário para multiplayer LAN com: criar sala, mostrar
endereço/código, entrar em sala, indicar conexão e prontidão, escolher preset
aleatório inicialmente, iniciar partida e exibir erros recuperáveis. Reutilize o
transporte e o handshake existentes; não altere a autoridade de combate que já
foi validada. A interface pode começar como overlay integrado, desde que seja
operável sem editar sf2lan.properties ou usar ADB manualmente.

Teste o fluxo completo nas duas instâncias do MuMu. Não acesse o POCO X7 Pro.
Atualize documentação e testes e crie um commit quando criar/entrar/iniciar/sair
funcionarem sem comandos externos.
```

**Critério de conclusão:** duas instâncias criam, encontram/entram e iniciam uma
partida pela interface, com feedback claro de erro e saída segura.

## 8. Validação, empacotamento e lançamento experimental

**Meta:** produzir um APK experimental rastreável, recuperável e testado, com
documentação suficiente para outra pessoa repetir a instalação e os testes.

**Modelo recomendado:** Sol médio. Um modelo mais forte só deve revisar uma falha
nova e específica encontrada durante a validação.

**Prompt para o Sol médio:**

```text
Trabalhe somente na etapa 8 do arquivo
lan-multiplayer/docs/MULTIPLAYER-COMPLETION-PLAYBOOK.md.

Prepare a validação final do multiplayer LAN sem alterar o design já aprovado.
Crie uma matriz de testes para duas instâncias MuMu e, somente após autorização
expressa, para dois aparelhos Android físicos. Cubra instalação limpa, atualização
com backup, diferentes resoluções, luta completa, três rounds, timeout, perda de
rede, reconexão, pausa/retorno e uma sessão prolongada. Registre versão, commit,
SHA-256, logs e resultado de cada cenário.

Gere apenas o APK experimental final, preserve o APK e o users.xml originais,
remova builds intermediárias descartadas e confirme que nenhum arquivo de jogo,
save, chave ou artefato de gigabytes será publicado pelo Git. Atualize o README,
as limitações conhecidas, o procedimento de recuperação e crie o commit/tag de
checkpoint. Não declare o multiplayer pronto se algum teste obrigatório falhar.
```

**Critério de conclusão:** matriz obrigatória aprovada, APK identificado por
commit e hash, instalação/recuperação documentadas e repositório sem artefatos
desnecessários.

## Ordem obrigatória de execução

As etapas devem ser feitas em sequência: `1 → 2 → 3 → 4 → 5 → 6 → 7 → 8`.
Se uma etapa revelar regressão numa etapa anterior, retornar ao último commit
funcional, corrigir a regressão e criar novo checkpoint antes de continuar.

Não é suficiente observar dois lutadores se movendo. O marco final exige que
entrada, animação, equipamento, física, combate, vida, rounds e resultado sejam
uma única partida reproduzida pelas duas instâncias.
