# CrossInk PW1 — prévia TXT 0.3.4

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
que ele pausou. A saída por tempo limite e o gesto foram confirmados na 0.2.1.
Na 0.3.4, o botão de repouso, o salvamento e a saída normal por evento de energia
foram confirmados no aparelho. A recuperação de um travamento real ainda precisa
ser validada; o log desse teste rejeitou o sinal enviado ao lançador.
Se necessário, use KUAL → CrossInk PW1 →
**Restaurar interface do Kindle**. Uma trava remanescente de um reinício pode
ser removida por essa opção, mesmo que o PID tenha sido reutilizado.

Para sair durante a sessão, **segure o canto superior direito por 5 segundos**
(na área de aproximadamente 10% da largura e da altura). Soltar o dedo ou sair
do canto cancela o gesto. O lançador tenta encerrar o aplicativo e força o
encerramento após 2 segundos caso ele não responda; a interface do Kindle é
retomada. Essa recuperação pode perder a posição ainda não salva. O gesto foi
confirmado na 0.2.1; o encerramento forçado de um travamento real ainda precisa
ser validado no PW1.

Não conecte o USB durante a sessão; saia antes.

## Teste do botão liga/desliga — saída validada na 0.3.4

1. Abra o TXT de exemplo e avance até a página 2.
2. Pressione e solte o botão liga/desliga uma vez.
3. O CrossInk deve salvar a página, encerrar e liberar a tela e o toque para o
   sistema do Kindle. A tela de descanso é responsabilidade da Amazon.
4. Aguarde alguns segundos e pressione o botão novamente para acordar.
5. Abra KUAL → CrossInk PW1 → **Abrir CrossInk (previa TXT)**, reabra o mesmo
   livro e confirme a retomada na página 2.
6. Saia pelo botão Sair e envie `crossink/logs/ui.log` e `ui.previous.log`.

O CrossInk não permanece aberto durante a suspensão nesta etapa. O monitor
escuta eventos `powerd` com `/usr/bin/lipc-wait-event`, sem mudar as configurações
de energia ou da luz frontal. Se o monitor falhar, a sessão termina e devolve
os recursos ao Kindle. Se o comando estiver ausente, o lançador recusa a abertura
antes de ocultar a barra; envie `crossink/logs/ui.log` nesse caso.

O log deve mostrar `power_event=goingToScreenSaver ...`, seguido do salvamento,
`reason=power` e das linhas `resumed`. Caso o aplicativo não responda ao pedido de
saída, a proteção prevista é o encerramento forçado pelo lançador após 2 segundos;
a última página pode não ter sido salva nesse caso. Essa proteção ainda não está
validada no aparelho: o teste abaixo rejeitou o sinal destinado ao lançador.
O auxiliar de eventos é encerrado e aguardado
na saída normal e recebe um sinal de morte do kernel se o leitor for morto.
A 0.3.3 abriu normalmente, mas o log recebeu zero bytes de eventos e mostrou
`powerd` em `active` antes de devolver o controle ao Kindle. Isso direcionou a
investigação para a pausa dos processos do sistema, sem provar a causa isolada.

Na 0.3.4, `cvm` e `awesome` permanecem ativos. A barra de status fica oculta
somente durante a sessão, pelo mecanismo de interface do Pillow usado no PW1.
O estado de energia e a luz frontal continuam sob controle do Kindle. As
chamadas de ocultar/restaurar a barra têm limite de um segundo. Se a ocultação
falhar ou o framework já estiver pausado por outro aplicativo, a prévia não abre.

O log deve incluir `framework_policy=cooperative`, `status_bar=hide`,
`power_state stage=before-prepare`, `after-prepare` e `before-release` (esta
última na saída normal). Ao sair, procure `status_bar=show`. A tela é entregue
sem repor a imagem antiga quando o evento de energia provoca a saída. Uma falha
ao restaurar a barra mantém o registro para tentar novamente pelo KUAL →
**Restaurar interface do Kindle**.

Em 14/10/2026, o usuário confirmou que o repouso funcionou na 0.3.4. O
[`ui(5).log` preservado](../../docs/validation/pw1-0.3.4-power-ui.txt) registra
`goingToScreenSaver`, página 4 salva, worker encerrado, tela liberada,
`exit_status=0`, estado `screenSaver`, `volumd` retomado e `status_bar=show`
com sucesso. Também registra `No matching parent launcher for signal=12`:
a saída normal funcionou, mas a comunicação que ativa a recuperação forçada
continua pendente. O log não comprova suspensão com o aplicativo aberto,
despertar/reabrir na página 4 nem ausência visual de sobreposição da barra.

O teste restante é despertar o Kindle, reabrir o mesmo TXT e conferir a página
4. Observe também se a barra invade a leitura e se volta na saída pelo botão
**Sair**. Envie `ui.log` e `ui.previous.log` para documentar esses caminhos.

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
validam a lógica. A saída por botão de repouso da 0.3.4 foi confirmada no PW1;
despertar/reabrir, recuperação forçada e os demais limites descritos acima
continuam pendentes.
