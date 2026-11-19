# Avaliacao do SF2Recomp

## Projeto analisado

- Repositorio: <https://github.com/bipinkrish/sf2recomp>
- Revisao analisada: `0924cf02663e04c80b7a2df77ba394ca7ced4474`
- Copia local isolada: `D:\REPOSITORIOS-GITHUB\sf2recomp-upstream`
- Origem suportada: Shadow Fight 2 de Nintendo Switch, versao 1.1.0, update `v131072`.

A copia upstream fica fora deste repositorio. Ela nao deve ser incorporada como pasta
aninhada, e nenhum arquivo do jogo, chave, NSP, codigo convertido ou build gerada deve
ser publicado.

## O que o projeto ja resolve

O SF2Recomp faz recompilacao estatica do executavel ARM64 de Switch e fornece um runtime
nativo com SDL3 e Vulkan. A implementacao inclui:

- cena Sparring/PVP existente na edicao de Switch;
- menu interno `Normal / LAN Multiplayer`;
- criacao de sala, entrada e descoberta do anfitriao na rede local;
- selecao sincronizada de lutadores e arena;
- dois jogadores humanos usando os controles locais de cada aparelho;
- simulacao lockstep a 60 passos por segundo, com atraso de entrada;
- verificacao de divergencia de estado, sincronizacao de carregamento e revanche;
- alvos para Windows, Linux, macOS e Android ARM64/x86-64.

Isso e substancialmente mais completo que a ponte UDP experimental deste repositorio.
O ponto mais importante e que o SF2Recomp parte de uma edicao que realmente contem a
cena PVP, enquanto a Definitive Edition Android analisada nao possui `ModulePvp`.

## Limite de compatibilidade

O conversor valida os build IDs dos modulos `main` e `sdk`, o metadata IL2CPP e os
arquivos RomFS da edicao de Switch. Ele exige uma copia propria e legalmente obtida do
NSP combinado com a atualizacao 1.1.0, ou os arquivos atualizados ja extraidos.

O APK `com.sf2.de` 2.41.9 nao pode substituir essa entrada. Ele usa outro empacotamento,
outro binario IL2CPP, outros enderecos e nao contem a cena PVP. Copiar apenas o protocolo,
os menus ou os hooks do SF2Recomp para o APK nao cria a cena que esta faltando.

## Caminhos possiveis

### Usar o SF2Recomp como base

Este e o caminho com maior chance de produzir multiplayer visivel. Ele preserva a
Definitive Edition e seu save como projeto separado, mas gera outro aplicativo Android
a partir dos arquivos da edicao de Switch.

Marco de viabilidade:

1. fornecer uma entrada Switch 1.1.0 compativel sem adiciona-la ao Git;
2. executar a conversao para Android x86-64;
3. instalar somente no MuMu;
4. abrir Sparring e registrar visualmente os menus Normal/LAN e Host/Join;
5. executar uma luta local antes de qualquer adaptacao ao gerenciador de saves.

Sem os arquivos da edicao de Switch, o codigo pode ser estudado e testado parcialmente,
mas nao existe como gerar o jogo nem demonstrar a luta.

### Transplantar ideias para a Definitive Edition

O protocolo lockstep, a descoberta LAN e algumas decisoes de interface podem servir de
referencia. Os hooks e enderecos nativos nao sao portaveis. Esse caminho continuaria
exigindo recuperar ou reconstruir uma cena de dois jogadores para o APK e, portanto,
nao remove o bloqueio descoberto nos prototipos 17 a 19.

## Decisao atual

O SF2Recomp sera avaliado como uma base independente, nao como uma biblioteca que pode
ser simplesmente ligada ao APK. A proxima implementacao fica condicionada a uma entrada
Switch compativel e ao marco visual no MuMu. Ate esse marco, nao devem ser criados novos
APKs da Definitive Edition com alegacao de multiplayer jogavel.

## Verificacao local do upstream

O repositorio foi clonado integralmente e permaneceu limpo. A suite tentou executar 146
testes neste Windows; os testes puramente Python avancaram, enquanto 76 casos que compilam
codigo nativo falharam por falta do ambiente exigido pelo projeto, principalmente Clang,
`pkg-config` e SDL3 3.4.0. A maquina possui Python 3.11.9, e o guia upstream recomenda
Python 3.12 ou superior. Esse resultado nao valida nem invalida o gameplay; apenas confirma
que o toolchain do upstream ainda precisa ser preparado antes de uma build local.
