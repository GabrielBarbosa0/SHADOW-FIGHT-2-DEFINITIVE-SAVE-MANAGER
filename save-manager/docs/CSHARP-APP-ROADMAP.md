# Roadmap — aplicativo nativo para Windows

## Visão do produto

Transformar o protótipo atual em Python/Tkinter em um aplicativo nativo e instalável para Windows, com identidade visual própria e experiência semelhante à do **HTServer Nexus**.

Nome de trabalho: **Shadow Fight 2: Definitive Edition — Save Manager**.

O aplicativo continuará sendo uma ferramenta comunitária não oficial. Ele não distribuirá o APK, arquivos proprietários do jogo ou dados pessoais do usuário.

## Objetivos

- oferecer uma interface moderna para criar, revisar, salvar e aplicar perfis;
- gerar um executável Windows 64 bits autocontido;
- disponibilizar um instalador com atalhos no menu Iniciar e, opcionalmente, na Área de Trabalho;
- detectar o aparelho por ADB e apresentar seu estado de conexão com clareza;
- criar um backup verificável antes de qualquer alteração no celular;
- preservar todas as funções já validadas no protótipo Python;
- permitir evolução posterior sem depender da edição manual de XML.

## Base técnica planejada

O padrão inicial seguirá o projeto HTServer Nexus:

- **linguagem:** C#;
- **interface:** WinUI 3;
- **plataforma:** .NET para Windows 64 bits;
- **arquitetura:** MVVM, separando interface, regras de save e comunicação ADB;
- **publicação:** `win-x64`, autocontida, sem exigir instalação prévia do runtime;
- **instalador:** Inno Setup;
- **distribuição:** GitHub Releases;
- **automação:** build e release por tag no formato `vX.Y.Z`;
- **testes:** testes unitários para geração, leitura e validação do XML.

A versão exata do .NET e do Windows App SDK deverá ser fixada no início da implementação, evitando dependências com versão curinga.

## Direção visual

A interface deverá parecer um aplicativo de Windows completo, não um formulário técnico. A referência é o estilo do HTServer Nexus, adaptado ao universo visual de Shadow Fight 2:

- tema escuro com contraste alto;
- cartões com bordas sutis e cantos arredondados;
- tipografia clara e hierarquia visual forte;
- cor de destaque inspirada no fogo e nos menus do jogo;
- barra de título personalizada com nome, ícone e status do aparelho;
- estados visuais para conectado, aguardando autorização, erro e operação concluída;
- botões principais curtos e inequívocos: **Gerar save**, **Criar backup** e **Aplicar save**;
- confirmações específicas antes de substituir dados no Android;
- layout responsivo para diferentes escalas e resoluções do Windows.

### Organização sugerida da tela

1. **Cabeçalho:** identidade do aplicativo e estado geral.
2. **Aparelho:** modelo, número de série, Android, autorização ADB e botão de atualização.
3. **Campanha:** perfil-base, nível e estado inicial ou completo.
4. **Recursos:** moedas, gemas, energia e orbes com controles independentes.
5. **Personagem:** Shadow inicial e conjuntos de Lynx, Eremita, Açougueiro, Vespa, Viúva, Shogun e Titã.
6. **Resumo:** prévia legível do perfil que será criado.
7. **Ações:** gerar, exportar, criar backup e aplicar.
8. **Atividade:** progresso, mensagens relevantes e log técnico recolhível.

## Funcionalidades do primeiro lançamento

O primeiro instalador somente será considerado equivalente ao protótipo quando oferecer:

- campanha nova e perfil completo original;
- níveis de 1 a 52;
- valores configuráveis de moedas, gemas e orbes;
- energia infinita opcional;
- conjuntos de todos os seis chefes principais e do Titã;
- geração determinística de `users.xml`;
- validação estrutural do XML antes de salvar ou aplicar;
- listagem e seleção de aparelhos ADB autorizados;
- backup de `users.xml` e `users_backup.xml` com data e aparelho;
- aplicação segura dos dois arquivos no pacote `com.sf2.de`;
- reinicialização opcional do jogo após a aplicação;
- mensagens de erro compreensíveis, sem ocultar o diagnóstico técnico;
- funcionamento em uma instalação limpa do Windows sem Python instalado.

## Arquitetura proposta

```text
src/
  Sf2DefinitiveSaveManager/
    Assets/
    Models/
    Services/
    ViewModels/
    Views/
    App.xaml
tests/
  Sf2DefinitiveSaveManager.Tests/
installer/
scripts/
docs/
images/
```

### Serviços principais

- `SaveGeneratorService`: cria o XML a partir das opções selecionadas.
- `SaveInspectorService`: lê, resume e valida saves existentes.
- `EquipmentCatalogService`: mantém os identificadores oficiais dos conjuntos.
- `AdbService`: detecta aparelhos e executa operações ADB sem bloquear a interface.
- `BackupService`: cria backups locais e registra data, aparelho e hashes.
- `SettingsService`: guarda preferências locais do aplicativo.
- `UpdateService`: poderá consultar novas versões em uma etapa posterior.

Os serviços não deverão depender diretamente dos controles da interface. Isso permite testar as regras de save sem abrir a janela do aplicativo.

## Migração do protótipo Python

O código Python será mantido como implementação de referência até a versão C# atingir paridade funcional.

1. congelar os casos de teste e os XMLs de referência atuais;
2. portar os modelos de configuração e o catálogo de equipamentos;
3. portar geração, inspeção e hashes do save;
4. comparar os XMLs produzidos pelas duas implementações;
5. portar a camada ADB e os backups;
6. construir a interface WinUI 3 sobre os serviços testados;
7. validar no POCO X7 Pro;
8. somente depois da equivalência, decidir se o protótipo Python será arquivado ou removido.

## Empacotamento e instalação

O pipeline de publicação deverá:

1. restaurar dependências com versões fixadas;
2. executar todos os testes;
3. publicar o aplicativo autocontido para `win-x64`;
4. montar o instalador com Inno Setup;
5. gerar hash SHA-256 do instalador;
6. anexar instalador e hash à GitHub Release;
7. futuramente, assinar o executável e o instalador com certificado de assinatura de código.

Enquanto não houver assinatura, a documentação deverá explicar que o Windows pode mostrar o aviso de editor desconhecido e orientar o download somente pela página oficial de Releases.

## Decisões pendentes

- nome comercial definitivo e identidade visual;
- ícone e materiais gráficos originais;
- versão mínima do Windows suportada;
- versão fixada do .NET e Windows App SDK;
- detectar uma instalação existente do ADB ou incluir uma distribuição autorizada do Android Platform Tools;
- permitir importação de saves externos já no primeiro lançamento ou numa versão posterior;
- oferecer atualização automática ou apenas avisar sobre novas releases.

## Etapas sugeridas

### Fase 1 — Fundação

- criar solução C# e projetos de aplicativo e testes;
- definir modelos, serviços e catálogo de equipamentos;
- portar e validar o gerador de XML.

### Fase 2 — Integração Android

- implementar detecção ADB assíncrona;
- criar fluxo de backup, aplicação, confirmação e restauração;
- testar falhas de autorização, desconexão e múltiplos aparelhos.

### Fase 3 — Interface

- estabelecer design system, cores, tipografia e componentes;
- implementar dashboard e estados de operação;
- adicionar acessibilidade, navegação por teclado e escalonamento de tela.

### Fase 4 — Distribuição

- criar ícone e metadados do executável;
- configurar publicação autocontida;
- criar instalador Inno Setup;
- automatizar GitHub Releases e documentar instalação.

### Fase 5 — Validação pública

- testar numa instalação limpa do Windows;
- validar novamente no POCO X7 Pro;
- publicar uma versão beta;
- corrigir problemas antes da versão `1.0.0`.

## Critérios para a versão 1.0.0

- todas as funções atuais possuem equivalente em C#;
- testes automatizados do gerador e inspetor estão aprovados;
- nenhum APK ou dado pessoal é incluído no instalador ou repositório;
- o backup é obrigatório antes da aplicação no celular;
- o aplicativo trata desconexões sem corromper o save;
- o instalador funciona numa máquina Windows limpa;
- desinstalar o aplicativo não remove backups do usuário sem confirmação;
- documentação, capturas de tela, hashes e instruções de recuperação estão publicadas.

## Ideias posteriores à versão 1.0

- editor avançado de inventário e encantamentos;
- importação, comparação e restauração visual de backups;
- perfis personalizados reutilizáveis;
- exportação e importação de configurações sem incluir dados do jogo;
- catálogo visual dos equipamentos;
- atualização do aplicativo pela página de Releases;
- idiomas adicionais.
