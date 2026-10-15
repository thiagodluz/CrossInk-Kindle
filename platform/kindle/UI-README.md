# CrossInk PW1 — prévia TXT 0.3.7

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

A leitura nesta versão pode continuar sem limite fixo de duração. Um supervisor
acompanha a entrada e o desenho das páginas e tenta recuperar a interface se
algum deles parar de responder. Ficar na mesma página sem tocar é normal e
não aciona essa recuperação. O diagnóstico de tela mantém seu limite anterior.

Para sair, toque em **Sair** ou segure o canto superior direito por cinco
segundos. A opção KUAL → CrossInk PW1 → **Restaurar interface do Kindle**
continua disponível. Saia antes de conectar o cabo USB.

## Atualização e teste prolongado da 0.3.7

O ciclo de repouso, reabertura, página salva, gesto do canto, saída por tempo
limite e aparência da barra/tela foi validado na 0.3.6. A 0.3.7 substitui os
limites da leitura por supervisão e precisa de novo teste no PW1.

1. Instale mesclando as pastas e preservando `crossink/data`. Feche a versão
   anterior antes de copiar. Ao atualizar da 0.3.5 diretamente, reinicie uma
   vez para descartar descritores antigos; não apague `state/operation.lock`.
2. Leia por **15 a 20 minutos**, virando páginas regularmente. O aplicativo
   deve permanecer aberto além dos antigos cinco/seis minutos.
3. Pressione o botão de repouso, desperte e reabra pelo KUAL. Selecione o
   livro e confira a última página salva.
4. Deixe uma sessão sem tocar e observe o repouso automático do Kindle.
   Anote quanto tempo levou. O temporizador é renovado somente por toques;
   esta versão não altera a configuração de tempo do sistema.
5. Teste **Sair**, reabertura e o gesto de cinco segundos no canto.
6. Confira a barra durante leitura/saída e a imagem de repouso. Envie
   `crossink/logs/launcher.log`, `ui.log` e `ui.previous.log` após os testes.

O TXT Boas-vindas foi atualizado. Sua posição pode reiniciar porque o conteúdo
mudou; os demais livros mantêm suas posições. Na abertura, procure
`reader_identity ... registered=1` e `launcher_identity ... valid=1`.
O launcher.log inclui `supervisor=started`; toques podem gerar
`user_activity=reset exit=0`. Uma falha nesse comando fica registrada e o
repouso padrão continua possível. Não é necessário provocar um travamento.

## Comportamento do repouso

O CrossInk salva, encerra e libera tela/toque para o sistema da Amazon.
Ele **não permanece aberto em memória durante a suspensão** nesta etapa.
Depois de acordar, é preciso abri-lo novamente pelo KUAL. A saída por evento de
energia foi confirmada na 0.3.4, 0.3.5 e 0.3.6. Na 0.3.6, os logs confirmam
a reabertura pelo KUAL; o usuário confirmou que selecionar o livro retoma a
página salva. Abrir na lista de arquivos é o comportamento atual da prévia.
A recuperação forçada da 0.3.7 e as sessões prolongadas ainda precisam de
validação física.

O monitor escuta `powerd` por `/usr/bin/lipc-wait-event`, sem alterar energia ou
luz frontal. Se o monitor falhar, a sessão termina. Se o comando estiver ausente,
a abertura é recusada antes de ocultar a barra. O auxiliar é encerrado e aguardado
na saída normal e recebe um sinal de morte do kernel se o leitor for morto.

`cvm` e `awesome` permanecem ativos. A barra de status é ocultada temporariamente
por chamadas Pillow com limite de um segundo. Se a ocultação falhar ou o framework
já estiver pausado, a prévia não abre. Ao sair, a barra é restaurada e `volumd`
é retomado. Falhas na restauração preservam o registro para nova tentativa pelo
KUAL. A recuperação pede a tela inicial somente quando `powerd` está ativo;
em repouso, preserva a tela da Amazon. O usuário confirmou a aparência correta da barra e da tela de repouso
no teste da 0.3.6; a 0.3.7 precisa de nova observação.

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
na 0.3.6 no PW1; o usuário confirmou a retomada da página, o gesto do canto,
o encerramento por cinco minutos e a aparência da barra/tela de descanso.
A 0.3.7 precisa de teste físico das sessões prolongadas, do repouso por
inatividade e da recuperação. O watchdog fixo de seis minutos foi substituído
pela supervisão da entrada/desenho.
