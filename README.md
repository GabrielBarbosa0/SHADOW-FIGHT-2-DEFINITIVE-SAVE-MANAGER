# Shadow Fight 2: Definitive Edition no Android moderno

Este projeto usa **Shadow Fight 2: Definitive Edition 2.41.9** no POCO X7 Pro. A edição é ARM64 nativa e funciona diretamente no Android 16.

O objetivo atual é jogar a campanha desde o começo com a aparência normal do Shadow, mantendo apenas facilidades de recursos: nível máximo, dinheiro, gemas, energia e orbes de encantamento praticamente infinitos.

## Estado confirmado

- Aparelho testado: POCO X7 Pro (`2412DPC0AG`, codinome `rodin_global`).
- Android testado: Android 16 / API 36.
- Pacote: `com.sf2.de`.
- Versão: `2.41.9` (`versionCode 30000102`).
- ABI nativa: `arm64-v8a`.
- APK: `game/Shadow Fight 2 DE/Shadow Fight 2_ DE.apk`.
- SHA-256 do APK: `09A5F4906FDE71BA734AB2282D5498C4D03DF9059D6355CA1E4B94CA8C570CCA`.
- Instalação e abertura confirmadas no aparelho real.
- O prólogo/tutorial inicial foi exibido corretamente em português.

O APK tem aproximadamente 1,12 GB e já inclui os dados do jogo. Não é necessário OBB separado.

## Interface gráfica de saves

O projeto inclui um aplicativo desktop para configurar e aplicar saves sem editar XML manualmente. A primeira versão permite:

- escolher entre campanha nova e o save completo original;
- selecionar qualquer nível de 1 a 52;
- ativar e configurar moedas, gemas, energia e orbes separadamente;
- usar o equipamento inicial ou os conjuntos de Lynx, Eremita, Açougueiro, Vespa, Viúva, Shogun e Titã;
- gerar e validar o XML antes da aplicação;
- detectar aparelhos conectados por ADB;
- criar backup automático e aplicar o save no Android.

Execute na raiz do projeto:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-save-manager.ps1
```

Ou diretamente pelo Python:

```powershell
python -m save_manager.app
```

O aplicativo usa somente Python 3 e Tkinter. Os saves gerados ficam em `generated/` e os backups automáticos em `backups/`; ambas as pastas são locais e ignoradas pelo Git.

Esta interface é o protótipo funcional do futuro aplicativo nativo para Windows. A migração planejada para C#, WinUI 3, executável autocontido e instalador está documentada em [`docs/CSHARP-APP-ROADMAP.md`](docs/CSHARP-APP-ROADMAP.md).

## Save ativo: campanha do início com facilidades

O perfil recomendado está em:

`saves/fresh-start-cheats/users.xml`

SHA-256:

`084EFF70DA2D8D10E8FD21CFD47274675BE490FDEBE69EC26415EC616BB3EFD0`

Configuração do perfil:

| Campo | Valor | Efeito |
|---|---:|---|
| `Tutorial` | `MOVE` | Começa no tutorial inicial |
| `CurrentZone` | `ZONE_1` | Começa no primeiro ato, contra Lynx |
| `Avatar` | `avatar_hero` | Aparência normal do Shadow |
| `Armor` / `Helm` | `Body` / `Head` | Corpo e roupa iniciais, sem Titã |
| `Weapon` | `Fists` | Começa lutando com os punhos |
| `Ranged` / `Magic` | `NoRanged` / `NoMagic` | Sem armas avançadas concedidas pelo save |
| `Level` | `52` | Nível máximo desta edição |
| `Experience` | `88.000.002` | Experiência do perfil de nível 52 |
| `Money` | `6.144.458.734.554` | Moedas praticamente infinitas |
| `Bonus` | `10.000.000.000.035` | Gemas praticamente infinitas |
| `ForgeMaterial1` | `2.147.000.000` | Orbes verdes praticamente infinitos |
| `ForgeMaterial2` | `2.147.000.000` | Orbes vermelhos praticamente infinitos |
| `ForgeMaterial3` | `2.147.000.000` | Orbes roxos praticamente infinitos |
| Item `Unlimited_Energy` | presente | Energia infinita |

O save contém somente os itens iniciais e a entrada inicial da batalha de Lynx. Ele não importa a coleção de 626 itens, as 143 batalhas concluídas, os equipamentos do Titã ou o progresso de mapas do perfil completo.

Observação: o nível 52 pode liberar sistemas e itens da loja mais cedo que numa campanha totalmente vanilla, mas não marca as lutas da história como concluídas.

## Save alternativo: perfil completo do Titã

O `users.xml` recebido com a Definitive Edition foi preservado sem alterações em dois lugares:

- `game/Shadow Fight 2 DE/users.xml`
- `saves/titan-complete/users.xml`

SHA-256:

`124A6FD2701C9FA757B0C611ED7A347FDB411BDF26146AB5BF27BD8C3F0EEA20`

Esse perfil tem nível 52, campanha avançada/concluída, 626 itens, 143 registros de batalha, corpo e equipamentos do Titã, mapas liberados e grandes quantidades de recursos. Ele não é o perfil recomendado para começar a história.

## Instalação no POCO X7 Pro

Requisitos:

- Depuração USB ativada.
- Opção **Instalar via USB** ativada nas Opções do desenvolvedor do HyperOS.
- `adb` disponível no computador.
- Telefone desbloqueado para confirmar a instalação, caso o HyperOS solicite.

Instale o APK:

```powershell
adb install -r -g ".\game\Shadow Fight 2 DE\Shadow Fight 2_ DE.apk"
```

Abra uma vez para o jogo criar os dados externos:

```powershell
adb shell monkey -p com.sf2.de -c android.intent.category.LAUNCHER 1
```

Os arquivos ativos ficam em:

`/sdcard/Android/data/com.sf2.de/files/userdata/`

## Aplicar um dos saves

Antes de trocar o perfil, faça backup do save ativo:

```powershell
adb shell am force-stop com.sf2.de
adb pull /sdcard/Android/data/com.sf2.de/files/userdata .\backup-userdata
```

Para aplicar o perfil de campanha inicial:

```powershell
adb shell am force-stop com.sf2.de
adb shell mv /sdcard/Android/data/com.sf2.de/files/userdata/users.xml.hash /sdcard/Android/data/com.sf2.de/files/userdata/users.xml.hash.pre-import
adb shell mv /sdcard/Android/data/com.sf2.de/files/userdata/users_backup.xml.hash /sdcard/Android/data/com.sf2.de/files/userdata/users_backup.xml.hash.pre-import
adb push ".\saves\fresh-start-cheats\users.xml" /sdcard/Android/data/com.sf2.de/files/userdata/users.xml
adb push ".\saves\fresh-start-cheats\users.xml" /sdcard/Android/data/com.sf2.de/files/userdata/users_backup.xml
adb shell monkey -p com.sf2.de -c android.intent.category.LAUNCHER 1
```

Para usar o perfil completo do Titã, substitua `fresh-start-cheats` por `titan-complete` nos dois comandos `adb push`.

Os comandos `mv` preservam os hashes anteriores em vez de apagá-los. Se o arquivo `.pre-import` já existir, renomeie-o ou guarde um backup antes de repetir o procedimento.

## APKs e GitHub

Arquivos `*.apk` e toda a pasta `game/` estão ignorados pelo Git. O APK da Definitive Edition continua disponível apenas localmente e não faz parte deste repositório. Backups do aparelho, saves gerados, ferramentas temporárias, caches e resultados de build também não são versionados.

## Estrutura principal

- `game/Shadow Fight 2 DE/`: APK local da Definitive Edition e save completo originalmente recebido.
- `saves/fresh-start-cheats/`: campanha desde o início com facilidades.
- `saves/titan-complete/`: cópia preservada do save completo com Titã.
- `saves/README.md`: detalhes e comparação dos perfis.
- `save_manager/`: aplicativo gráfico, gerador de XML e integração ADB.
- `scripts/run-save-manager.ps1`: inicializador da interface gráfica.
- `tests/`: testes automatizados do gerador de save.
- `docs/CSHARP-APP-ROADMAP.md`: plano da migração para o aplicativo nativo em C#.

As pastas locais `game/`, `backups/` e `generated/` são criadas ou utilizadas durante o uso, mas permanecem fora do Git.
