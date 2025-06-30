# SLOW - Cliente Interativo

## Autores

- **Jesus Sena Fernandes** - 12697470  
- **Letícia Raddatz Jönck** - 14609637  
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

## O que o código implementou

O cliente implementa o handshake de três vias conforme especificado no PDF: envia um pacote de CONNECT, aguarda o pacote de ACCEPT do servidor, atualiza número de sequência (seqnum) e janela remota, e então entra em modo conectado.

Para envio de dados, o cliente fragmenta automaticamente mensagens maiores que MAX_DATA, atribuindo a cada fragmento um identificador de fragmento (FID) e offset (FO), e marca o flag MOREBITS enquanto houver continuação. A cada fragmento, aguarda um ACK correspondente antes de prosseguir, e mantém controle de bytes em voo e janela do peer.

O cliente também suporta “pure ACK”: caso seja necessário apenas confirmar pacotes sem enviar dados, o seqnum **não** é incrementado (seqnum == acknum), respeitando a recomendação de não misturar ACKs puros com incremento de sequência.

As operações de disconnect (x) e revive (r) seguem o protocolo descrito na página 4 do PDF: o disconnect envia flags CONNECT|REVIVE|ACK sem payload, e revive zero-way envia REVIVE|AC` com payload opcional (ou “revive” por padrão), restabelecendo a sessão sem novo handshake completo.

Além disso, o cliente traz tela interativa com caixa Unicode para visualizar cabeçalhos de pacotes, payload preview e comandos auxiliares (?, h).

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