# CrossInk PW1 — validação de hardware 0.1.0

Este primeiro pacote valida o executável ARM, o framebuffer e o touchscreen do
Kindle Paperwhite de primeira geração. Ainda não abre EPUBs nem contém a interface
completa do CrossInk. A integração com o leitor é a etapa seguinte.

## Instalação pelo USB

1. Extraia `CrossInk-PW1-KUAL-validacao-0.1.0.zip` no computador.
2. Copie a pasta `crossink` para a raiz do armazenamento do Kindle.
3. Copie `extensions/crossink` para a pasta `extensions` existente no Kindle.
   Mescle as pastas; preserve as outras extensões.
4. Ejete o Kindle com segurança e abra o KUAL.
5. Selecione **CrossInk PW1 → Testar tela e toque (90 segundos)**.

Os caminhos finais são `/mnt/us/crossink/bin/crossink-pw1` e
`/mnt/us/extensions/crossink/menu.json`. O pacote usa `/bin/sh` para os scripts e
um ELF ARM EABI com ABI soft-float para o programa.

## O que observar

A tela deve ficar preta, depois branca, e mostrar o título CrossInk PW1, uma
rampa de cinza, quatro cantos numerados e um botão SAIR. Toque em **1, 2, 3, 4**,
nesta ordem, e depois no centro. Observe se a marca acompanha o ponto tocado.
Se houver orientação ou coordenadas invertidas, isso é um resultado útil para a
calibração: envie uma foto e o log.

Toque em SAIR para encerrar. O programa também sai automaticamente em até
90 segundos. Um supervisor independente retoma a interface em aproximadamente
120 segundos se o programa travar. Não conecte USB enquanto a tela de teste
estiver ativa; o teste termina rapidamente e não inibe o modo USB do sistema.

O retorno normal restaura os pixels anteriores, libera o touchscreen e retoma
os processos da interface que o lançador pausou. Uma interrupção forçada pode
precisar de uma nova pintura pela interface do Kindle. Se necessário, use
**CrossInk PW1 → Restaurar interface do Kindle** no KUAL.

Depois do teste, copie `/mnt/us/crossink/logs/diagnostic.log` pelo USB e envie junto
com a foto. O log contém dimensões, stride, rotação, limites dos eixos, atualização
da tela, coordenadas de toque e o resultado da saída. O log anterior fica em
`diagnostic.previous.log`. Se o teste não abrir, envie também `launcher.log`.

## Compilação reproduzível

Use a toolchain **kindle5** de KOReader/koxtoolchain, release `2026.08`:
https://github.com/koreader/koxtoolchain/releases/tag/2026.08

O perfil kindle5 atende Kindle 4, Touch e PW1. O perfil kindlepw2 e o perfil
kindlehf atendem outros alvos. O pacote não substitui bibliotecas do Kindle:
este primeiro diagnóstico é estático e incorpora suas bibliotecas. O empacotador
verifica ARM EABI soft-float, ausência de dependências externas e requisito de
kernel Linux até 2.6.31 antes de criar o ZIP. A integração futura do leitor poderá
usar `/lib/ld-linux.so.3` e bibliotecas compatíveis com a glibc 2.12 do aparelho.

```sh
python3 scripts/kindle/build-package.py --toolchain /caminho/x-tools/arm-kindle5-linux-gnueabi
```

Saída: `build/kindle-pw1/`. A auditoria ELF e os hashes ficam dentro do pacote.
A toolchain fornece glibc 2.12.2, incorporada apenas ao executável de diagnóstico.
As bibliotecas instaladas no Kindle não são substituídas.

Verificações locais dos limites de framebuffer e da decodificação de eventos:

```sh
g++ -std=c++17 -Wall -Wextra -Werror platform/kindle/KindleBackends.cpp platform/kindle/BackendChecks.cpp -o /tmp/pw1-checks
/tmp/pw1-checks
python3 platform/kindle/check-launcher.py
```

Os backends ainda são independentes do HAL do leitor. A interação com Linux
fica em `KindleBackends.cpp`; `Diagnostic.cpp` é apenas seu cliente de validação.
Não existe uma alegação de teste em hardware antes do usuário executar o pacote.

## Referências da implementação

- Toolchain: https://github.com/koreader/koxtoolchain/blob/master/README.md
- ABI dos ioctls: https://github.com/koreader/koreader-base/blob/master/ffi/mxcfb_kindle_h.lua
- Atualização Pearl: https://github.com/koreader/koreader-base/blob/master/ffi/framebuffer_mxcfb.lua
- Modelo PW1: https://github.com/koreader/koreader/blob/master/frontend/device/kindle/device.lua
- Formato KUAL: https://github.com/koreader/koreader/blob/master/platform/kindle/extensions/koreader/menu.json

Este código foi escrito para CrossInk-Kindle. As referências acima documentam
interfaces e comportamento do sistema; não foi copiado um lançador do KOReader.
