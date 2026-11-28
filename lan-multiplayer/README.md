# Módulo multiplayer LAN

Pesquisa e implementação experimental de multiplayer local para o APK Android
ARM64 de Shadow Fight 2: Definitive Edition. Este componente contém a ponte UDP,
o hook IL2CPP, o relay, os scripts de build e a documentação da engenharia reversa.

O protótipo atual negocia os dois perfis, encaminha controles, coordena o início da
luta e transporta telemetria. O protótipo 77 fixa P1 à esquerda e P2 à direita sem
o antigo cruzamento em X. O protótipo 83 preserva a identidade nativa
`Player/Opponent` do modo Duelo: os lutadores nascem frente a frente e voltam a se
encarar depois de cruzar. Um único evento de inicialização libera o cronômetro e os
eventos seguintes da IA remota permanecem bloqueados. O protótipo 87 fixa
temporariamente o preset canônico de P1 nos dois lutadores das duas telas, incluindo
avatar, armadura, elmo e armas. Ainda faltam autoridade
completa de animação, câmera/posição no segundo aparelho, vida, rounds e resultado;
portanto, não é um multiplayer final.

A build 88 remove o evento nativo que a build 87 liberava uma vez para encerrar a
apresentação. Isso volta a isolar completamente a IA remota: os dois MuMu registram
somente eventos suprimidos e nenhum movimento autônomo. Como consequência conhecida,
o cronômetro permanece em 150 e os lutadores ficam na apresentação inicial. O próximo
protótipo deve reproduzir apenas a transição de início da luta, sem chamar o planejador
da IA. Divergências de modelo, arma/animação e câmera continuam pendentes e não devem
ser tratadas como multiplayer funcional.

O prototipo nativo p104 substitui esse bloqueio total por uma transicao controlada:
inicializa o planejador remoto uma unica vez para encerrar a apresentacao, cancela-o
imediatamente e intercepta `NDABNAKKEFE.NAKIMBDKMKM` antes que qualquer acao autonoma
seja escolhida. Em duas instancias MuMu, o relogio iniciou, nenhuma acao da IA foi
executada, o controle superior moveu o lutador esquerdo nas duas telas e o controle
inferior moveu o lutador direito nas duas telas. O equipamento visual ainda diverge
porque a regra nativa `RANDOM_EQUIPMENT` do Duelo cria caches antes da substituicao
dos perfis; p104 e um checkpoint funcional de controle/IA, nao um multiplayer final.

O marco p141 restaura esse controle bidirecional sobre uma base mais previsivel.
O predicado humano nativo do `Fighter` e liberado somente para os dois lutadores
LAN, e a autoridade espacial passa a ser dividida: P1 publica somente o Shadow
esquerdo e P2 publica somente o Shadow direito. O pacote `FIGHT_AUTHORITY` agora
circula nos dois sentidos. Em duas VMs MuMu, o analogico superior moveu a esquerda
nas duas telas e o analogico inferior moveu a direita nas duas telas, sem movimento
autonomo durante a pausa. Relogio, posicao horizontal e lados ficaram alinhados no
teste. A pose/animacao completa de salto, a geometria da arma direita, vida, rounds e
resultado ainda precisam de sincronizacao propria; p141 continua experimental.

A configuracao de partida agora tambem e autoritativa. No modo direto, o host
sorteia um preset para P1 e outro para P2; no modo relay, o computador faz o
sorteio. Os dois Androids recebem o mesmo seed, tier e pares de presets e precisam
confirmar o mesmo fingerprint antes de liberar a sessao. Repeticoes sao permitidas.
Os cinco slots de equipamento sao aplicados aos perfis local e remoto antes da
criacao da luta. Na build 87, os presets independentes continuam sendo negociados,
mas a camada nativa usa deliberadamente o preset de P1 nos quatro modelos. O sorteio
independente sera reativado depois que ambas as telas reproduzirem o mesmo estado de
combate. O nivel negociado ainda e metadado de matchmaking: a escrita segura
do nivel interno do perfil IL2CPP permanece pendente.

## Estrutura

- `lan_bridge/`: protocolo Python e código Android/ARM64 injetado no APK;
- `scripts/`: análise REA, build, repack incremental e limpeza de APKs antigos;
- `tests/`: testes automatizados do protocolo e do relay;
- `docs/`: protocolo, histórico técnico e conclusões da engenharia reversa.

## Testes

Execute dentro desta pasta:

```powershell
python -m unittest discover -s tests -v
```

Consulte `docs/LAN-MODE-DEVELOPMENT.md` antes de gerar uma nova build. O APK
original e os artefatos locais permanecem fora do Git.
