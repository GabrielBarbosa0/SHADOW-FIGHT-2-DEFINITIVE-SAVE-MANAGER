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

## Uso permitido neste projeto

O protocolo lockstep, a descoberta LAN e algumas decisoes de interface podem servir de
referencia. Os hooks e enderecos nativos nao sao portaveis. Esse caminho continuaria
exigindo recuperar ou reconstruir uma cena de dois jogadores para o APK e, portanto,
nao remove o bloqueio descoberto nos prototipos 17 a 19.

## Decisao atual

O produto final e, obrigatoriamente, uma modificacao da **Definitive Edition Android**.
O SF2Recomp nao sera compilado, nao recebera os saves deste projeto e nao sera usado como
base do aplicativo. Ele permanece apenas como referencia externa para estudar lockstep,
descoberta LAN e validacao de divergencia.

A engenharia reversa prossegue sobre `com.sf2.de` com o REA. O marco seguinte e adaptar
uma cena ja incluida no APK para instanciar dois lutadores controlaveis. Ate esse marco,
nao deve ser criado outro APK com alegacao de multiplayer jogavel.

## Verificacao local do upstream

O repositorio foi clonado integralmente e permaneceu limpo. A suite tentou executar 146
testes neste Windows; os testes puramente Python avancaram, enquanto 76 casos que compilam
codigo nativo falharam por falta do ambiente exigido pelo projeto, principalmente Clang,
`pkg-config` e SDL3 3.4.0. A maquina possui Python 3.11.9, e o guia upstream recomenda
Python 3.12 ou superior. Esse resultado nao valida nem invalida o gameplay; apenas confirma
que o toolchain do upstream ainda precisa ser preparado antes de uma build local.
