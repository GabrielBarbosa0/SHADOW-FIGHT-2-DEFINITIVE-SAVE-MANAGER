# Módulo multiplayer LAN

Pesquisa e implementação experimental de multiplayer local para o APK Android
ARM64 de Shadow Fight 2: Definitive Edition. Este componente contém a ponte UDP,
o hook IL2CPP, o relay, os scripts de build e a documentação da engenharia reversa.

O protótipo atual negocia os dois perfis, encaminha controles, coordena o início da
luta e transporta telemetria. Ainda faltam autoridade completa de posição,
orientação, vida, animação e resultado; portanto, não é um multiplayer final.

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
