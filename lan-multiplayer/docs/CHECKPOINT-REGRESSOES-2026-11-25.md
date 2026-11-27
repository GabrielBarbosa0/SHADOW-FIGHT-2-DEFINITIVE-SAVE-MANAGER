# Checkpoint antes da reversão de regressões — 25/11/2026

Este documento congela o contexto técnico da etapa interrompida em 25/11/2026.
Ele não declara o multiplayer funcional e não promove nenhuma build experimental
posterior à 88. O objetivo é permitir retomar a pesquisa ou reverter as mudanças
sem depender do histórico da conversa.

## Decisão de produto confirmada

- A base continua sendo exclusivamente o APK ARM64 de **Shadow Fight 2:
  Definitive Edition 2.41.9**, pacote `com.sf2.de`.
- O SF2Recomp serve apenas como referência; ele não é a base do jogo.
- O modo LAN deve usar lutadores humanos normais. Corpos, esqueletos, poses e
  presets de chefes (Titã, Açougueiro, Vespa etc.) não devem participar da luta.
- Durante a estabilização, os quatro modelos visíveis — P1 e P2 nas duas telas —
  podem usar o mesmo conjunto humano inicial. O sorteio independente fica para
  depois.
- O controle local de cada aparelho deve comandar seu jogador e o resultado deve
  ser reproduzido nas duas telas. IA, câmera, vida, rounds e resultado precisam
  permanecer sincronizados.
- Testes são permitidos somente nos MuMu `emulator-5554` e `emulator-5556`.
  **Não acessar nem instalar no Poco X7 Pro.**
- O APK original e as builds anteriores devem continuar preservados; toda build
  nova deve ser outra cópia.

## Último estado versionado confiável

- Branch: `main`.
- HEAD: `fc8ff9ab3e1d7033f9e791d74597a06adff59cbb`
  (`fc8ff9a Fully suppress remote fighter AI`).
- Esse commit corresponde ao checkpoint da build 88: a IA remota fica totalmente
  bloqueada, mas o relógio permanece em 150 e a apresentação inicial não termina.
- Commit anterior relevante: `a42e249 Add canonical LAN fighters and match
  equipment sync`.
- Nenhuma mudança das builds 89–94 foi commitada.

## Mudanças locais ainda não commitadas

No momento deste checkpoint, somente estes três arquivos de código estão
modificados em relação a `fc8ff9a`:

- `lan-multiplayer/lan_bridge/android/native/sf2_lan_hook.c`
- `lan-multiplayer/lan_bridge/android/src/com/sf2/lan/LanEquipmentPreset.java`
- `lan-multiplayer/lan_bridge/equipment.py`

Identificador do diff binário completo no momento do checkpoint:
`3d5f4e036137e129645ac49bb6980806ad74f201`.

As mudanças locais incluem:

1. inclusão do slot `Skeleton` no perfil IL2CPP, além de `Armor`, `Helm`,
   `Weapon`, `Ranged` e `Magic`;
2. liberação de exatamente um callback de inicialização visual da IA remota,
   bloqueando os comandos de ataque/movimento produzidos durante esse callback;
3. reaplicação do perfil humano no construtor de `Fighter`;
4. hook de `PreFight.ViewerInit` para reaplicar os dois perfis;
5. hook ainda não validado de `PreFight.CreateVS`, criado para tentar alcançar o
   modelo antes de ele armazenar a arma antiga;
6. restrição do sorteio Python e Java ao preset `initial`. Os presets antigos de
   chefes continuam reconhecidos no catálogo apenas por compatibilidade, mas não
   são selecionados para uma partida LAN.

## Evidência obtida pela engenharia reversa

O dump IL2CPP confirmou a classe de perfil `EFPINIIBPLJ` e o método `SetItem`
(`RVA 0x316F7E8`). No ARM64, os slots usados por esse método são:

| Slot | Offset no perfil |
| --- | ---: |
| Skeleton | `0xB8` |
| Weapon | `0xC0` |
| Armor | `0xC8` |
| Helm | `0xD0` |
| Ranged | `0xD8` |
| Magic | `0xE0` |
| Avatar (string) | `0x198` |

Também foram localizados:

- `Fighter..ctor(EFPINIIBPLJ)` em `RVA 0x33987C0`;
- `PreFight.ViewerInit(config, player, opponent, bool)` em `RVA 0x31E2E08`;
- `PreFight.CreateVS(player, opponents, int, bool, bool, bool, bool)` em
  `RVA 0x31E36DC`;
- `FightScene.CreateFight(config, player, opponents)` em `RVA 0x3234A0C`.

O teste da build 93 mostrou a ordem real observada no log:

1. `FightScene.CreateFight` e os construtores de `Fighter` criam os lutadores;
2. `PreFight.ViewerInit` é chamado depois;
3. portanto, alterar o perfil somente no `ViewerInit` é tarde demais para alguns
   caches visuais, especialmente o lutador da esquerda.

## Histórico das builds desta etapa

### Build 88 — checkpoint versionado

- IA remota completamente suprimida.
- Sem movimento autônomo.
- Regressão conhecida: relógio em 150 e lutadores presos na apresentação.
- É a referência segura para começar uma reversão, não uma build jogável.

### Build 89 — não aprovada

- Adicionou o slot `Skeleton` em `0xB8` e passou a aplicar seis slots.
- A divergência visual continuou.
- Não foi commitada.

### Build 90 — avanço funcional, equipamento incorreto

- Liberou uma única chamada de inicialização visual do oponente.
- Durante essa chamada, `FightInput` é bloqueado para não deixar a IA gerar
  comandos.
- O relógio começou a contar e o movimento voltou a responder.
- As chamadas posteriores da IA remota continuaram suprimidas.
- O equipamento/pose ainda divergia entre as duas cenas.
- SHA-256: `4DC14F77F2355D5C74CE5450F9D7E62C178674EE43A132E62705C7A02D3DE874`.

### Build 91 — melhora parcial, não aprovada

- Reaplicou o preset humano no construtor de cada `Fighter`.
- Melhorou o lutador da direita, mas o da esquerda continuou retendo a arma
  anterior.
- Movimento e relógio continuaram funcionando.
- SHA-256: `84247062551BF8628248A720C2C310AAEED3A1FBCA26A55920812BCA99EADD08`.

### Build 92 — inválida e perigosa

- Primeiro teste do hook `PreFight.ViewerInit`.
- O primeiro argumento, que é um objeto `GNCICMKMPFD`, foi tipado incorretamente
  como `int32_t` e seu endereço foi truncado ao chamar o método original.
- Resultado: `SIGSEGV` no `emulator-5556` (`fault addr 0xdead1007`).
- **Não instalar nem usar como base.**
- SHA-256: `3822BCD4AE51789D49AC52EE360B8692A6B7E22E1584FA7C03AA09AF138BD956`.

### Build 93 — executa, mas ainda visualmente incorreta

- Corrigiu a assinatura do `ViewerInit` para receber um ponteiro de objeto.
- Foi instalada e executada nos dois MuMu sem queda.
- O relógio chegou a 143 e a simulação estava ativa.
- Logs registraram `itens=6/6`, `Weapon=Fists`, corpo humano e um único evento de
  inicialização visual sem comandos de IA.
- Apesar disso, a imagem ainda mostrou arma/pose divergente: o modelo da esquerda
  preservou o sai antigo enquanto outro modelo apareceu desarmado/com pose
  diferente. Isso confirmou que o cache visual nasce antes do `ViewerInit`.
- As duas instâncias permaneciam com a build 93 no momento da interrupção.
- SHA-256: `41B3EEDBB8B26098E0032140E7F85AFBCA20A26E8F85556BBE4E62225C375E5A`.

### Build 94 — compilada, nunca instalada ou testada

- Adicionou o hook de `PreFight.CreateVS` para aplicar o perfil humano ao jogador
  e a todos os perfis da lista de adversários antes da criação da luta.
- A compilação e assinatura terminaram depois que a execução da tarefa foi
  interrompida.
- **Não há qualquer validação de estabilidade, movimento, IA ou imagem.**
- Não instalar como se fosse uma build aprovada.
- SHA-256:
  `6C79341379ADCCD1B43850E253505AA0DB3E3EEC3B52D6E34D4198BF01C392F1`.

## Regressões e problemas ainda abertos

1. **Equipamento/pose divergentes:** perfil, arma renderizada e controlador de
   animação podem vir de momentos diferentes. Foram vistos sai, soco-inglês e
   pose de chefe em combinações incompatíveis.
2. **IA:** a supressão total trava a apresentação; liberar o callback inteiro
   reativa a IA. A build 90 introduziu um meio-termo promissor, mas ele ainda não
   foi versionado como estável.
3. **Câmera do segundo MuMu:** em testes anteriores ela deixou de acompanhar os
   lutadores e chegou a enquadrar uma arena vazia. A regressão ainda não foi
   isolada por commit.
4. **Facing:** houve uma etapa em que os personagens olhavam para o mesmo lado.
   A identidade nativa `Player/Opponent` do modo Duelo deve ser preservada para
   que eles se encarem e virem ao cruzar.
5. **Movimento cruzado em X:** foi corrigido antes desta etapa e não deve ser
   reintroduzido. O controle da instância superior deve representar um lado e o
   da inferior o outro, com ambos reproduzidos nas duas telas.
6. **Vida, rounds, relógio e derrota:** ainda não formam um estado autoritativo
   único. Uma derrota em uma tela não encerra a outra.
7. **Retratos e nomes:** apareceram combinações como retrato de chefe com nomes
   `Shadow`/`Ninja`; devem usar identidades humanas coerentes.

## Testes registrados

- `python -m unittest discover -s tests -q`: **32 testes passaram** antes das
  builds 92, 93 e 94.
- Relay observado ativo na interrupção: processo Python PID `37996`, portas
  `24190/24191`. Esse PID é apenas informação da sessão e não deve ser assumido
  numa retomada.
- Dispositivos ADB autorizados na interrupção: somente `emulator-5554` e
  `emulator-5556`.
- Capturas da build 93 ficaram fora do repositório em
  `%TEMP%\sf2-lan-p93\fight-emulator-5554.png` e
  `%TEMP%\sf2-lan-p93\fight-emulator-5556.png`.

## Caminho seguro para retomar ou reverter

Antes de qualquer nova implementação:

1. preservar as mudanças locais 89–94 em uma branch/commit experimental separado
   ou como patch; não misturá-las ao `main` como se estivessem validadas;
2. restaurar os três arquivos modificados ao commit `fc8ff9a` caso a decisão seja
   realmente voltar ao checkpoint 88;
3. reinstalar somente uma cópia conhecida no MuMu e repetir um teste mínimo de
   relógio, movimento local/remoto, ausência de IA e câmera;
4. reintroduzir uma alteração por vez, criando commit apenas quando o teste visual
   nas duas instâncias passar;
5. não avançar para vida/rounds enquanto os dois modelos humanos não tiverem o
   mesmo equipamento, pose e orientação nas duas telas.

Este checkpoint deliberadamente não executa a reversão. Ele apenas registra o
estado para que a próxima sessão possa escolher entre testar a 94 como experimento
ou voltar ao código versionado da 88 sem perder o raciocínio obtido.

## Retomada validada em 27/11/2026: prototipo nativo p104

A retomada isolou a diferenca entre liberar a apresentacao e permitir decisoes da
IA. `EAGJIMCGDJI` ainda precisa ser executado uma vez no planejador remoto para o
Duelo sair da introducao. A escolha efetiva da proxima acao ocorre em
`NDABNAKKEFE.NAKIMBDKMKM`, RVA `0x302C034`.

O p104 inicializa o planejador remoto uma vez, cancela-o imediatamente e retorna
`null` em toda selecao autonoma posterior, preservando apenas `FightInput` humano.

Validacao visual e por log nos dois MuMu:

- o relogio saiu de 150 e passou a correr;
- as tentativas de selecao autonoma foram registradas e bloqueadas;
- nenhuma barra de vida mudou sem comando de ataque;
- `emulator-5554` moveu o lutador esquerdo nas duas telas;
- `emulator-5556` chegou como `index=1` e moveu o lutador direito nas duas telas;
- nomes e retratos aparecem como `Shadow` nos quatro HUDs;
- nenhum aparelho fisico foi acessado.

Pendencia: a configuracao ainda possui `150=RANDOM_EQUIPMENT`. O motor materializa
arma, esqueleto e pose em caches anteriores aos perfis canonicos, produzindo armas
diferentes entre cenas. O proximo trabalho deve neutralizar essa regra na origem.

SO testada diretamente, sem empacotar APK:

`%TEMP%\libsf2lanhook-p104.so`

SHA-256: `61BF2BD2722C12C455DBA480DF16947C2A381D19DBE3CA46E71DA32008DC8B1D`.

## Retomada de 27/11/2026: perfis independentes e luta liberada (p123)

Os prototipos p120 a p122 provaram que clonar os dois lados a partir do mesmo
perfil humano deixa os quatro modelos visualmente iguais, mas compartilha estado
interno de animacao: `FightScene.StartFight` entra na rotina original e nao
retorna. O HUD permanece em 150 e nenhum comando consegue mover os lutadores.

O p123 voltou a usar duas bases independentes para o jogador e o adversario,
mantendo por cima o mesmo nome `Shadow`, avatar humano e os mesmos IDs canonicos
de armadura, capacete, arma, ranged e magia. Tambem foi corrigido o agendador:
frame pacing nao e mais aplicado antes da primeira execucao de `StartFight`.

Validacao somente em `emulator-5554` e `emulator-5556`:

- `StartFight` retorna e os dois relogios contam normalmente;
- P1 chega ao lado esquerdo nas duas cenas;
- P2 chega ao outro processo como `Touch 1`/indice 1;
- as tentativas de selecao autonoma continuam bloqueadas pelo hook do planejador;
- nomes, retratos e ponteiros de itens registrados sao iguais entre as duas VMs;
- nenhum aparelho fisico foi acessado.

Pendencias visuais preservadas de proposito neste checkpoint:

- os dois lutadores ainda podem olhar para a mesma direcao, em vez de um para o
  outro;
- o modelo esquerdo conserva corpo/pose e cache de arma de uma base de chefe,
  enquanto o direito conserva a pose de sai, apesar de o perfil registrar o
  mesmo item canonico nos dois lados;
- a arma pode aparecer com uma pose incompativel, como ocorreu anteriormente com
  o sai usando a pose dos cutelos do Acougueiro;
- o preset da katana inicial foi anotado como proximo instrumento de diagnostico,
  pois sua silhueta e postura tornam divergencias mais faceis de identificar. Ele
  ainda nao foi aplicado.

Capturas locais deste teste ficaram fora do repositorio em
`%TEMP%\sf2-lan-p123` e `%TEMP%\sf2-lan-p123-controls`.
