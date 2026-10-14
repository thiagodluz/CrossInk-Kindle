# CrossInk PW1 — prévia TXT 0.2.0

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
que ele pausou. A recuperação forçada e o tempo limite ainda precisam ser
validados no aparelho. Se necessário, use KUAL → CrossInk PW1 →
**Restaurar interface do Kindle**. Não conecte o USB durante a sessão; saia antes.

## Arquivos e limites atuais

- Coloque seus TXT e subpastas em `crossink/data/books`.
- Use UTF-8, com ou sem BOM; quebras de linha LF e CRLF são aceitas.
- Limite de 256 KiB por TXT, 512 páginas e 200 entradas por pasta.
- A posição fica em `crossink/data/progress`, salva ao voltar, sair e durante
  a leitura. Alterar o conteúdo do livro reinicia a posição.
- Arquivos de outros formatos recebem uma mensagem; EPUB, imagens, ajustes de
  fonte, biblioteca completa, luz frontal, bateria e suspensão estão pendentes.
- A sessão pausa a interface Amazon e, quando presente, o serviço `volumd`.
  Ao sair, somente os processos pausados pela sessão são retomados.

## Compilação e verificações

Inicialize o SDK com `git submodule update --init freeink-sdk`.
Use koxtoolchain kindle5 2026.08, com glibc 2.12.2 e ABI mínima Linux 2.6.31:

```sh
python3 scripts/kindle/package-ui.py --toolchain /caminho/arm-kindle5-linux-gnueabi
python3 scripts/kindle/build-ui.py --host
python3 platform/kindle/check-launcher.py
```

O pacote inclui auditoria ELF, fontes/licenças aplicáveis e objetos para relink
em `crossink/development`. A versão ARM é estática, ARMv7 Cortex-A8 softfp;
não depende das bibliotecas instaladas no Kindle. Os testes locais e a emulação
validam a lógica, mas a prévia 0.2.0 ainda requer o teste físico descrito acima.
