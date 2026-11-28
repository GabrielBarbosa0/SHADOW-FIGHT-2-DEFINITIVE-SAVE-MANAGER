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

## Observacoes adicionais do teste de 27/11/2026

### Pose permanece desacoplada da arma

- O lutador da direita pode receber facas, mas continua usando a pose de
  soco-ingles.
- O lutador da esquerda pode aparecer com espada e armadura do Tita, mas usa a
  pose de combate sem arma/apenas punhos.
- Portanto, o item exibido na mao e o controlador de animacao ainda nascem de
  fontes ou caches diferentes. Sortear ou sobrescrever somente o ID da arma nao
  corrige a postura.

### Orientacao e preset de diagnostico

- Depois da padronizacao visual, os dois lutadores voltaram a ficar de costas um
  para o outro.
- A postura tambem nao corresponde ao armamento, repetindo o caso anterior do
  sai exibido com a pose dos cutelos do Acougueiro.
- Foi sugerido usar temporariamente a katana inicial nos dois lados. A silhueta
  e a postura da katana tornam mais facil distinguir arma correta, pose correta
  e orientacao correta. Esta sugestao esta apenas registrada; ainda nao foi
  implementada.

### Experimento de troca do save para Shadow: resultado invalido

Em 27/11 foi aplicado por engano o perfil `fresh-start-cheats` completo nas duas
instancias. Embora ele use `NAME_SHADOW`, `avatar_hero`, corpo humano e punhos,
ele tambem possui `Tutorial=MOVE` e `CurrentZone=ZONE_1`. O resultado foi:

- retorno das duas instancias ao tutorial/inicio da campanha;
- personagem invisivel durante parte da apresentacao do tutorial;
- na luta contra Carlos, nome e retrato do adversario tambem apareceram como
  Shadow, produzindo visualmente `Shadow vs. Shadow`;
- o experimento nao serve como base de teste do modulo LAN.

Os saves anteriores foram preservados antes dessa troca. A correcao deve partir
deles, mantendo `Tutorial=END`, zonas, batalhas e desbloqueios, e alterando
somente o equipamento/identidade visual do jogador de Tita para Shadow normal.
Nao se deve substituir novamente o save inteiro por um perfil de campanha nova.

A correcao foi aplicada em seguida nas duas instancias a partir de seus backups
independentes. A verificacao posterior confirmou `Tutorial=END`,
`CurrentZone=ZONE_2`, Duelo disponivel e o mapa anterior restaurado. Nos dois
saves ativos, somente a apresentacao do jogador foi normalizada para
`NAME_SHADOW`, `avatar_hero`, `Skeleton`, `Body`, `Head`, `Fists`, `NoRanged` e
`NoMagic`. O progresso, as batalhas e os desbloqueios nao foram reiniciados.

### Espada Ninja visivel com moveset de soco-ingles

Depois da recuperacao, `WEAPON_NINJA_SWORD` foi equipado manualmente nos dois
saves. A espada passou a aparecer corretamente na mao do Shadow nas duas
instancias, mas a pose e os ataques continuaram sendo os de soco-ingles. O log
explicou a divergencia: o save ativo informava `WEAPON_NINJA_SWORD`, enquanto o
preset `initial` do protocolo e o perfil canonico nativo ainda injetavam
`Fists`/`WEAPON_KNUCKLES` durante a criacao do controlador de animacao.

A proxima build deve usar `WEAPON_NINJA_SWORD` de forma identica no preset
Python, no preset Java e no perfil canonico nativo. Essa alteracao e um teste
direto da hipotese do cache de moveset; ela nao declara a pose corrigida antes
da validacao visual nas duas instancias.

### Validacao p124: Espada Ninja e moveset coerentes

O prototipo p124 unificou o preset `initial` em `Body`, `Head` e
`WEAPON_NINJA_SWORD` nas camadas Python, Java e nativa. A biblioteca foi
instalada diretamente nas duas VMs, preservando a p123 antes da troca.

Resultados observados somente em `emulator-5554` e `emulator-5556`:

- o log registrou `WEAPON_NINJA_SWORD` durante a fabrica visual, construcao da
  luta e consultas posteriores dos perfis;
- os quatro modelos passaram a usar a pose e os ataques da Espada Ninja;
- os lutadores passaram a usar guarda de espada e ficaram voltados um para o
  outro;
- o comando de ataque do jogador 1 produziu um golpe de espada visivel, em vez
  da pose/ataque de soco-ingles;
- `StartFight` retornou, os relogios correram e as selecoes autonomas da IA
  continuaram suprimidas;
- nao ocorreu `SIGSEGV` durante o teste.

Este resultado valida somente a coerencia entre arma e moveset. As capturas das
duas VMs ainda ocorreram em instantes diferentes e nao provam sincronizacao
completa de animacao, vida, rounds ou resultado.

Uma observacao visual posterior qualificou esse resultado: a arma do lutador
direito ainda nao era a mesma nas duas telas. Na instancia superior parecia
uma faca ou lamina longa, enquanto na inferior era uma adaga. As duas usavam
corretamente a pose da Espada Ninja. Portanto, a p124 corrigiu o moveset, mas
nao a geometria da arma direita que o `Fighter` havia materializado a partir
do perfil aleatorio do Duelo.

O proximo experimento deve preservar os perfis independentes e reaplicar apenas
o item de arma canonico depois de `FightScene.CreateFight`, quando o cache
visual dos dois `Fighter` ja existe. Essa etapa nao deve alterar o moveset nem
reativar IA.

Biblioteca experimental testada sem substituir o APK-base:

`%TEMP%\libsf2lanhook-p124-ninja-sword.so`

SHA-256: `C5B7F1F2EE7796CAD73B9D06DF6034940398835DA047881E26E25C0CC29A3B31`.

### Validacao p125: mesma geometria de arma nas duas VMs

A p125 preservou a Espada Ninja como preset canonico e acrescentou uma
atualizacao visual tardia: depois de `FightScene.CreateFight`, quando os dois
objetos `Fighter` ja existem, o item `WEAPON_NINJA_SWORD` de cada perfil e
reaplicado ao respectivo lutador. Essa chamada atualiza somente a malha da arma;
os perfis esquerdo e direito continuam independentes e nao sao recriados.

Resultados observados em `emulator-5554` e `emulator-5556`:

- os logs confirmaram a reaplicacao da mesma `WEAPON_NINJA_SWORD` nos lados
  esquerdo e direito das duas VMs;
- a adaga que ainda aparecia no lutador direito da VM inferior desapareceu;
- os quatro modelos passaram a mostrar a mesma lamina longa, com a mesma pose
  de Espada Ninja;
- nome, retrato, armadura e capacete continuaram iguais;
- o comando de movimento do jogador 1 continuou aparecendo nas duas telas;
- os dois processos permaneceram ativos e nao houve `SIGSEGV`;
- o APK-base e os saves nao foram substituidos.

Foram executados 33 testes Python com sucesso. A p124 de cada VM foi preservada
em `backups/native-libraries/p124-before-weapon-refresh/` antes da troca.

Biblioteca experimental testada:

`%TEMP%\libsf2lanhook-p125-weapon-refresh.so`

SHA-256: `AE379E8E13284D533AB96FE075C7C327BA4F946720BED3D83B3ADD4866C9B7E9`.

### Diagnostico p129-p137: comando do jogador 2 chega ao Fighter

Esta etapa manteve os dois Shadow com Espada Ninja, IA autonoma bloqueada e
orientacao frente a frente. O modo da configuracao foi alterado para
`FightPVP` (`17`), o que tambem ativou a barra de vida azul do jogador 1 sem
regredir os perfis ou as armas.

O rastreio demonstrou, nas duas VMs, que o comando do jogador 2:

- chega pela ponte LAN com indice `1` na tela do jogador 1;
- resolve o `Fighter` direito correto;
- encontra `stage=2`, entrada habilitada e um controlador nao nulo;
- era recusado antes de entrar na lista de acoes, enquanto o mesmo soco do
  jogador esquerdo mudava o estado da acao de `0` para `1`;
- com o fallback restrito as rotas `rede` e `local-p2`, passa a mudar o estado
  do controlador direito de `0` para `1` nas duas VMs;
- dispara `GFPBHHBHBMA` e chega a `HPDIKFPFIEI.FIOGOHHINKH` no `Fighter`
  direito com um objeto de acao valido (`listas=1/1/0`).

Apesar disso, a animacao visivel do lutador direito ainda nao inicia. Logo, o
bloqueio restante esta depois de `FIOGOHHINKH`, no despachante de animacao do
`Fighter` direito. Nao se deve voltar a investigar UDP, indices, resolucao do
lutador ou criacao da fila antes de concluir esse ponto.

Build com fallback de roteamento testada nas duas VMs:

`%TEMP%\libsf2lanhook-p135-routed-fallback.so`

SHA-256: `683E48C51AFE2F5A88DB171D74860B66466879AFC71AD9F2858EA8A6CF8E5FBB`.

Build diagnostica mais recente, instalada somente em `emulator-5556` para
confirmar o evento final (a `emulator-5554` permaneceu na p135):

`%TEMP%\libsf2lanhook-p137-action-lists.so`

SHA-256: `264CFF851D18A31D67E8EF560044F89CDC5D7AE114BCF6FC675F8EA899690BC8`.

Nenhum teste desta etapa acessou ou instalou arquivos no Poco X7 Pro. O APK e
os saves originais continuam preservados.

### Marco p138-p141: controle independente e autoridade espacial dividida

A analise ARM64 mostrou que `HPDIKFPFIEI.ILPMDKNIEFN` (`0x339FA94`) nao e
apenas uma validacao na entrada. O mesmo predicado decide, a cada frame, se
`GFPBHHBHBMA.NAKIMBDKMKM` (`0x31D09F0`) atualiza o controlador do `Fighter`.
Por isso, inserir o comando diretamente na fila nao era suficiente. O hook
agora retorna humano somente para os dois `Fighter` vivos da luta LAN, sem
copiar o perfil completo nem alterar o identificador de lado em `+0x5C`.

O teste p139 confirmou que o analogico inferior passou a movimentar e fazer o
lutador direito saltar na propria VM. O evento tambem chegava ao controlador
direito da VM superior, mas sua posicao permanecia parada. A causa seguinte
nao estava no roteamento: o sincronizador tratava P1 como autoridade dos dois
lutadores e a ponte Java enviava `FIGHT_AUTHORITY` somente de P1 para P2.

Na p140/p141 a autoridade foi dividida:

- P1 controla e publica somente o lutador esquerdo, indice `0`;
- P2 controla e publica somente o lutador direito, indice `1`;
- os dois Androids enviam e recebem `FIGHT_AUTHORITY`;
- cada processo aplica apenas a posicao, dano e rounds do lutador pertencente
  ao peer, sem sobrescrever o lutador local;
- os perfis transitorios efetivos agora sao obtidos de `Fighter+0x148`; os
  antigos campos `Fight+0x98/+0xA0` nao sao tratados como um par de perfis.

Validacao visual em `emulator-5554` e `emulator-5556`:

- o analogico inferior moveu o lutador direito para a esquerda nas duas telas;
- o analogico superior moveu o lutador esquerdo para a direita nas duas telas;
- os relogios observados durante os testes ficaram iguais ou com um segundo de
  diferenca (`95/95`, `71/71`, `125/124`);
- depois da liberacao dos analogicos, nenhum lutador continuou andando sozinho;
- nao houve falha nova do processo, `SIGSEGV` ou `FATAL EXCEPTION`;
- os 33 testes Python passaram;
- o Poco X7 Pro nao foi acessado;
- APK original e saves nao foram substituidos.

Limitacoes ainda visiveis: a posicao horizontal esta replicada, mas o salto
remoto ainda nao reproduz a mesma pose na outra tela. A geometria/moveset da
arma direita tambem continua divergente em alguns inicios de luta. Animacao,
camera, vida, rounds e resultado ainda nao formam um multiplayer final.

APK completo p141, gerado e assinado depois da validacao nas duas VMs:

`dist/Shadow-Fight-2-Definitive-Edition-LAN-prototype-141.apk`

SHA-256: `C9C9C7AF1235BB5BF2B42A16B376946D75655016EE32DB53F7EBEE74594CEE5C`.

A p140 intermediaria foi removida depois da geracao da p141 para nao manter
duas copias de 1,12 GB. A p141 e o unico APK completo desta etapa.

Biblioteca p141 testada nas duas VMs:

`tmp/libsf2lanhook-p141-fighter-profiles.so`

SHA-256: `302282AF2C52208F5280792A7987E44B694219FFD79E2EBDBE0C4207AD7A8C0B`.
