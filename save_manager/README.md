# Gerenciador de save

Aplicativo desktop local para configurar saves do pacote `com.sf2.de` e aplicá-los por ADB.

## Executar

Na raiz do projeto:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-save-manager.ps1
```

Também é possível usar:

```powershell
python -m save_manager.app
```

O programa usa apenas a biblioteca padrão do Python 3: Tkinter para a interface, `ElementTree` para XML e `subprocess` para ADB.

## Segurança

- O XML é gerado primeiro em `generated/users.xml`.
- O arquivo gerado é relido e validado antes da aplicação.
- A aplicação exige confirmação explícita na interface.
- Um backup completo da pasta `userdata` é salvo em `backups/` antes de substituir qualquer arquivo.
- Hashes existentes no Android são renomeados com data e hora, não apagados.

## Escopo da primeira versão

- campanha nova ou save completo original;
- nível de 1 a 52;
- moedas, gemas, energia e orbes configuráveis de forma independente;
- equipamento inicial, conjuntos de Lynx, Eremita, Açougueiro, Vespa, Viúva, Shogun ou Titã;
- presets;
- detecção de aparelhos ADB;
- backup e aplicação do save.

Os conjuntos dos seis chefes principais foram mapeados diretamente dos arquivos internos `stages.xml` e `list.xml` da Definitive Edition. Eles alteram aparência e equipamento, mas preservam a tática controlável do jogador.
