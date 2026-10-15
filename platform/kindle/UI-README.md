# CrossInk PW1 — prévia TXT 0.3.6

Esta etapa conecta o GfxRenderer, as fontes, os componentes FreeInkUI e o módulo
Txt do CrossInk aos backends nativos do Paperwhite 1. Permite navegar por pastas,
abrir TXT, virar páginas e retomar a posição salva. A interface experimental é
própria desta etapa; ActivityManager, UITheme, atividades completas e EPUB ainda
não foram integrados. O teste 0.1.0 de tela e toque permanece disponível.

## Instalação pelo USB

1. Feche o KOReader e conecte o Kindle ao computador.
2. Extraia este ZIP. Copie as pastas `crossink` e `extensions` para a raiz da
   unidade do Kindle. **Mescle com as pastas existentes**, substituindo somente
   os arquivos deste pacote. Não apague a pasta `crossink/data`.
3. Ejete a unidade e desconecte o cabo USB.
4. Abra o KUAL → CrossInk PW1 → **Abrir CrossInk (previa TXT)**.

Não é necessário abrir a shell. A instalação não altera o sistema raiz,
os livros da Amazon ou os dados do KOReader. Esta versão abre somente os
arquivos de `crossink/data/books`, usando armazenamento privado com proteção
contra caminhos que escapem dessa pasta de dados.

## Primeiro teste no Kindle

1. Toque em **Boas-vindas.txt**.
2. Use **Próxima** e **Anterior**. Verifique o texto, os acentos e o contador de
   páginas, além da resposta ao toque.
3. Na página 2, toque em **Voltar** e abra o mesmo arquivo. Ele deve retomar na
   página 2. Também teste essa retomada depois de sair e abrir novamente pelo KUAL.
4. Toque em **Sair**. A interface do Kindle deve reaparecer e funcionar.
5. Envie uma foto da leitura e o arquivo `crossink/logs/ui.log`, disponível
   quando reconectar pelo USB. O log anterior fica em `ui.previous.log`.

Cada sessão desta prévia termina automaticamente após **5 minutos**. O
supervisor encerra uma execução travada após **6 minutos** e retoma os processos
que ele pausou. Para sair durante a leitura, segure o canto superior direito
por cinco segundos, ou toque em **Sair**. A opção KUAL → CrossInk PW1 →
**Restaurar interface do Kindle** tenta recuperar uma sessão interrompida.
Não conecte o USB durante a sessão; saia antes.

## Atualização e reteste da 0.3.6

Na 0.3.5, a abertura e o repouso funcionaram no PW1, e o log confirmou que o
sinal chegou ao lançador. A reabertura falhou e `launcher.log` estava vazio.
Um defeito de herança da trava foi reproduzido localmente; o ponto exato da
falha no aparelho ainda não foi identificado.

A 0.3.6 libera a trava explicitamente, impede que auxiliares herdem descritores,
registra o leitor real mesmo com um shell intermediário e registra a tentativa
antes da preparação. Aguarda até cinco segundos se a abertura coincidir com a
limpeza anterior. Esses caminhos passaram nos testes locais e ARM emulados;
os logs da 0.3.6 confirmam repouso, reabertura pelo KUAL e saída normal no PW1.
O usuário também confirmou a retomada da página ao selecionar o livro.

1. Instale conforme as instruções acima, mesclando as pastas e preservando
   `crossink/data`.
2. **Reinicie o Kindle uma vez após atualizar da 0.3.5.** Isso descarta
   descritores que processos anteriores à atualização possam ter herdado.
   Não apague `state/operation.lock` para tentar destravar.
3. Abra o TXT, avance uma página e pressione o botão de repouso uma vez.
4. Desperte e reabra pelo KUAL. Confira a página salva. Repita esse ciclo.
5. Teste **Sair**, nova abertura, gesto do canto e encerramento por cinco minutos.
6. Confira visualmente a tela de descanso e a barra de status. Copie
   `crossink/logs/launcher.log`, `ui.log` e `ui.previous.log` depois do teste,
   inclusive se alguma abertura falhar.

Na abertura, procure `reader_identity ... registered=1` e
`launcher_identity ... valid=1`. No repouso, procure `launcher_signal=12 sent=1`,
`power_handoff=launcher grace_seconds=2` e o salvamento da página. No gesto,
o sinal é 10. Não é necessário provocar um travamento no aparelho.
Uma tentativa de Restaurar durante a preparação pode retornar
`session_guard=busy`; aguarde alguns segundos e tente novamente.

## Comportamento do repouso

O CrossInk salva, encerra e libera tela/toque para o sistema da Amazon.
Ele **não permanece aberto em memória durante a suspensão** nesta etapa.
Depois de acordar, é preciso abri-lo novamente pelo KUAL. A saída por evento de
energia foi confirmada na 0.3.4, 0.3.5 e 0.3.6. Na 0.3.6, os logs confirmam
a reabertura pelo KUAL; o usuário confirmou que selecionar o livro retoma a
página salva. Abrir na lista de arquivos é o comportamento atual da prévia.
A recuperação forçada de um travamento real continua pendente.

O monitor escuta `powerd` por `/usr/bin/lipc-wait-event`, sem alterar energia ou
luz frontal. Se o monitor falhar, a sessão termina. Se o comando estiver ausente,
a abertura é recusada antes de ocultar a barra. O auxiliar é encerrado e aguardado
na saída normal e recebe um sinal de morte do kernel se o leitor for morto.

`cvm` e `awesome` permanecem ativos. A barra de status é ocultada temporariamente
por chamadas Pillow com limite de um segundo. Se a ocultação falhar ou o framework
já estiver pausado, a prévia não abre. Ao sair, a barra é restaurada e `volumd`
é retomado. Falhas na restauração preservam o registro para nova tentativa pelo
KUAL. A recuperação pede a tela inicial somente quando `powerd` está ativo;
em repouso, preserva a tela da Amazon. A aparência dessa transição precisa de
observação no aparelho.

## Arquivos e limites atuais

- Coloque seus TXT e subpastas em `crossink/data/books`.
- Use UTF-8, com ou sem BOM; quebras de linha LF e CRLF são aceitas.
  TXT com UTF-8 inválido recebe uma mensagem pedindo a conversão antes de abrir.
  A prévia não converte automaticamente Latin-1 ou Windows-1252.
- Limite de 256 KiB por TXT, 512 páginas e 200 entradas por pasta.
- A posição fica em `crossink/data/progress`, salva ao voltar, sair e durante
  a leitura. Alterar o conteúdo do livro reinicia a posição.
- Arquivos de outros formatos recebem uma mensagem; EPUB, imagens, ajustes de
  fonte, biblioteca completa, luz frontal e bateria estão pendentes. A saída
  para suspensão é experimental; a retomada dentro do próprio app está pendente.
- A sessão mantém a interface Amazon executando, oculta temporariamente sua
  barra e pausa somente `volumd`, quando presente. Ao sair, restaura a barra e
  retoma somente o serviço pausado pela sessão. O diagnóstico de tela mantém
  seu comportamento anterior de pausa/retomada da interface.

## Compilação e verificações

Inicialize o SDK com `git submodule update --init freeink-sdk`.
Use koxtoolchain kindle5 2026.08, com glibc 2.12.2 e ABI mínima Linux 2.6.31:

```sh
python3 scripts/kindle/package-ui.py --toolchain /caminho/arm-kindle5-linux-gnueabi
python3 scripts/kindle/build-ui.py --host
python3 scripts/kindle/check-host.py
```

O empacotamento exige um checkout sem alterações locais. O CI executa
`check-host.py`, incluindo backends, recuperação, TXT, armazenamento e tarefas
nativas. Esse comando prepara automaticamente a pasta de teste de NativeChecks.
Os relatórios do KUAL ficam em `crossink/logs/crossink-pw1-diagnostics-<data-hora>`.

O pacote inclui auditoria ELF, fontes/licenças aplicáveis e objetos para relink
em `crossink/development`. A versão ARM é estática, ARMv7 Cortex-A8 softfp;
não depende das bibliotecas instaladas no Kindle. Os testes locais e a emulação
validam a lógica. Repouso, reabertura pelo KUAL e saída normal foram registrados
na 0.3.6 no PW1; a retomada da página foi confirmada pelo usuário. Recuperação
forçada, gesto/limite nesta versão e aparência da barra/tela de descanso ainda
precisam de validações separadas.
