# Protocolo SLOW - Cliente Interativo

## Autores

- **Jesus Sena Fernandes** - 12697470  
- **Letícia Raddatz Jönck** - 14589066
- **Nicolas Carreiro Rodrigues** - 14600801  

## Requisitos

Antes de compilar, verifique de que os seguintes itens estão instalados no seu sistema:

- **g++** com suporte a C++17
- **make**
- Sistema Linux ou macOS (criado em macOS 15.5)

Você pode instalar os requisitos com:

```bash
sudo apt update
sudo apt install g++ make
```

## O que o código implementa

O cliente cobre **todas** as transições descritas no PDF. Na partida ele executa o three‑way handshake completo: manda um CONNECT (SYN), recebe o SETUP (SYN‑ACK) com `ACCEPT`, confirma janela e sequência remota e passa a utilizar o `sid` fornecido pelo servidor. Todo cabeçalho trocado é mostrado numa moldura unicode para facilitar depuração.

Durante a transferência o envio respeita uma janela deslizante. Cada pacote em voo é registado numa fila interna; quando chega um ACK o pacote é removido e `bytesInFlight` é descontado, permitindo que a janela se abra novamente. Se um ACK nunca vier, um cronômetro de 1s por entrada aciona até três retransmissões, depois o fragmento é descartado - assim mantemos o canal vivo mesmo sob perda.

Quando apenas precisamos confirmar recepção, o cliente produz um **pure‑ACK**: o campo `flags` leva só `ACK`, não há payload nem incremento de `seqnum` (o valor fica igual a `acknum`), exatamente como a página 4 das especificações do projeto exige.

O envio de dados decide entre modo direto e fragmentado. Se a mensagem couber na janela e em `MAX_DATA`, vai num único datagrama e o terminal exibe “Enviando mensagem sem fragmentar”. Se ultrapassar qualquer limite, a rotina parte o payload em blocos de até 1440B, atribui um `FID` único e incrementa `FO` a cada fragmento, marcando `MOREBITS` até o último bloco. Cada fragmento respeita a janela corrente; se ela fechar o processo estaciona, transmite um pure‑ACK para acelerar a liberação e só prossegue depois de espaço disponível. A cada passo prints exibem qual fragmento está saindo e um trecho do conteúdo.

O disconnect emprega a combinação `CONNECT|REVIVE|ACK` com janela 0 para encerrar a sessão de forma limpa. O zero‑way revive aceita uma mensagem opcional do utilizador, envia‑a com `REVIVE|ACK`, atualiza a sequência local com o ACK do servidor e restaura todos os contadores, reatando a conversa sem novo handshake.

Por fim, há timeout de recepção global de 5 s – se não houver actividade o `select()` retorna e a app pode decidir retransmitir, abortar ou apenas avisar o utilizador. Todos os eventos relevantes (“retransmitindo”, “janela atualizada”, “fragmentação concluída”) aparecem no log.

## Compilação

Para compilar o cliente, utilize o `make`:

```bash
make clean && make
```

Isso gerará o executável `bin/slow_peripheral`.

## Primeira Execução

Para executar o cliente pela primeira vez:

```bash
./slow_peripheral
```

Você verá o prompt interativo:

```
================= S L O W   C L I E N T =================
  d) data     x) disconnect     r) revive     ? ) status
  h) help     q) quit
=========================================================

```

Digite `d` para enviar dados, `x` para desconectar, `r` para revive, `?` para status, `h` para ajuda e `q` para sair.

## Teste com Fragmentação

No diretório principal do projeto há um arquivo chamado `test.in`, criado para testar o cliente em condições que exigem fragmentação. Esse arquivo contém uma mensagem muito longa suficiente para ultrapassar os limites de `MAX_DATA` e da janela de envio, forçando o cliente a dividir o conteúdo em múltiplos fragmentos numerados com `FID` fixo e `FO` incremental. Após o envio da mensagem, o arquivo também comanda a desconexão (`x`) e o encerramento do programa (`q`), cobrindo o fluxo completo da aplicação.

Para executar esse teste  utilize o comando:

```
./bin/slow_peripheral < test.in
```

## Exemplos de Execução

Suponha um arquivo exemplo.in com o seguinte conteúdo:

```
d
hello slow world
x
q
```

E que você execute:

```
./slow_peripheral < exemplo.in
```

## Explicação detalhada da saída

1) **Three-way handshake**  
╔ TX ═════════════════════════════════════╗  
║ SID  : 00000000000000000000000000000000 ║  ← session ID vazio (cliente inicia)  
║ FLG  : CONNECT                          ║  ← só SYN (CONNECT)  
║ SEQ  : 0   ACK : 0                      ║  ← seq inicial do cliente, sem nada a confirmar  
║ WIN  : 7200   FID/FO: 0/0               ║  ← minha janela de recebimento  
╚═════════════════════════════════════════╝  
CONNECT: é o SYN inicial.  
SEQ=0 / ACK=0: seq inicial do cliente, sem nada a confirmar.  
WIN=7200: janela que declaro ao servidor.

Logo em seguida o servidor responde:  

╔ RX ═════════════════════════════════════╗  
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║  ← session ID gerado pelo servidor  
║ FLG  : ACCEPT                           ║  ← SYN-ACK  
║ SEQ  : 2692   ACK : 0                   ║  ← seq do servidor; ack do nosso SYN  
║ WIN  : 1024   FID/FO: 0/0               ║  ← janela do servidor  
╚═════════════════════════════════════════╝  
ACCEPT: o servidor aceitou; é o SYN-ACK.  
SEQ=2692: seq inicial do servidor (aleatório).  
ACK=0: confirma nosso SYN com ack=0.  
Atualizamos `sess.remoteWindow = 1024`.

2) **Envio de dados (d)**  
Como estamos redirecionando um `.in`, não vemos o prompt `# Mensagem:`, mas logo sai:

╔ TX ═════════════════════════════════════╗  
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║  ← usamos a mesma SID do servidor  
║ FLG  : ACK                              ║  ← ACK-only + payload  
║ SEQ  : 2694   ACK : 2692                ║  
║ WIN  : 7200   FID/FO: 0/0               ║  
╚═════════════════════════════════════════╝

✉  DATA (16 B): "hello slow world"  
ACK: aqui o flag inclui ACK para carregar dados.  
SEQ=2694: após o handshake, `sess.seqnum` foi inicializado em 2693 `(setup.seqnum+1)`, então `++sess.seqnum` → 2694.  
ACK=2692: estamos confirmando o último seq do servidor.  
FID/FO=0/0: como o payload cabe inteiro, não fragmentamos.

O servidor responde o ACK do dado:

╔ RX ═════════════════════════════════════╗  
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║  
║ FLG  : ACK                              ║  
║ SEQ  : 2694   ACK : 2694                ║  
║ WIN  : 1024   FID/FO: 0/0               ║  
╚═════════════════════════════════════════╝  
[debug] Janela atualizada: 1024  
[sucesso] Mensagem enviada (16 B)

3) **Desconexão (x)**  
Quando chega o comando `x`, fazemos:

╔ TX ═════════════════════════════════════╗  
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║  
║ FLG  : CONNECT\|REVIVE\|ACK            ║  
║ SEQ  : 2695   ACK : 2694                ║  
║ WIN  : 0   FID/FO: 0/0                  ║  
╚═════════════════════════════════════════╝  
CONNECT\|REVIVE\|ACK: usamos os flags para sinalizar teardown.  
SEQ=2695: incrementamos seq mais uma vez.  
ACK=2694: confirmamos o último do servidor.  
WIN=0: sinaliza “sem janela” (teardown).

O servidor então:

╔ RX ═════════════════════════════════════╗  
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║  
║ FLG  : ACK                              ║  
║ SEQ  : 0   ACK : 0                      ║  
║ WIN  : 1024   FID/FO: 0/0               ║  
╚═════════════════════════════════════════╝  
[sucesso] Desconectado.  
ACK: confirma o teardown.  
SEQ=0 ACK=0: após disconnect bem‑sucedido, resetamos estado.

4) **Resumo dos campos**  
- **SID**: identificador único da sessão (cliente: `0000…`, depois o do servidor).  
- **FLG**: conjunto de flags que indicam tipo do pacote (`CONNECT`, `ACK`, `ACCEPT`, `REVIVE`, `MOREBITS`).  
- **STTL**: “TTL” interno (não usamos aqui, sempre 0 no cliente).  
- **SEQ**: número de sequência do pacote (incrementa a cada envio de dados ou controle).  
- **ACK**: confirma até qual número de sequência já recebemos do outro lado.  
- **WIN**: janela de recebimento anunciada.  
- **FID/FO**: fragment ID e offset, usados para fragmentação.

Cada valor segue o que o PDF especifica para handshake, transferência de dados sem fragmentação e teardown.

A saída total será:
```
➜  bin git:(main) ✗ ./slow_peripheral < example.in 
╔ TX ═════════════════════════════════════╗
║ SID  : 00000000000000000000000000000000 ║
║ FLG  : CONNECT  STTL: 0                 ║
║ SEQ  : 0   ACK : 0                      ║
║ WIN  : 7200   FID/FO: 0/0               ║
╚═════════════════════════════════════════╝

╔ RX ═════════════════════════════════════╗
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║
║ FLG  : ACCEPT  STTL: 599                ║
║ SEQ  : 2692   ACK : 0                   ║
║ WIN  : 1024   FID/FO: 0/0               ║
╚═════════════════════════════════════════╝

[HANDSHAKE] concluído! janela=1024 B
[sucesso] Conectado.

================= S L O W   C L I E N T =================
  d) data     x) disconnect     r) revive     ? ) status
  h) help     q) quit
=========================================================
> # Mensagem:
╔ TX ═════════════════════════════════════╗
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║
║ FLG  : ACK  STTL: 0                     ║
║ SEQ  : 2694   ACK : 2692                ║
║ WIN  : 7200   FID/FO: 0/0               ║
╚═════════════════════════════════════════╝

✉  DATA (16 B): "hello slow world"

╔ RX ═════════════════════════════════════╗
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║
║ FLG  : ACK  STTL: 599                   ║
║ SEQ  : 2694   ACK : 2694                ║
║ WIN  : 1024   FID/FO: 0/0               ║
╚═════════════════════════════════════════╝

[debug] Janela atualizada: 1024
[sucesso] Mensagem enviada (16 B)

================= S L O W   C L I E N T =================
  d) data     x) disconnect     r) revive     ? ) status
  h) help     q) quit
=========================================================
>
╔ TX ═════════════════════════════════════╗
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║
║ FLG  : CONNECT|REVIVE|ACK  STTL: 0      ║
║ SEQ  : 2695   ACK : 2694                ║
║ WIN  : 0   FID/FO: 0/0                  ║
╚═════════════════════════════════════════╝

╔ RX ═════════════════════════════════════╗
║ SID  : df3bfbcbd4f58ac1a53cdd3465fce114 ║
║ FLG  : ACK  STTL: 599                   ║
║ SEQ  : 0   ACK : 0                      ║
║ WIN  : 1024   FID/FO: 0/0               ║
╚═════════════════════════════════════════╝

[sucesso] Desconectado.

================= S L O W   C L I E N T =================
  d) data     x) disconnect     r) revive     ? ) status
  h) help     q) quit
=========================================================
> [tchau] sessão encerrada!
➜  bin git:(main) ✗ 
```
