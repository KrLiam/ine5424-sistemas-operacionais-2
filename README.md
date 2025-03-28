# Sistemas Operacionais II

> Projeto da disciplina de Sistemas Operacionais II (INE5424) do grupo A.

## Requisitos

- C++ 20

## Como compilar

Nosso projeto usa Makefile. Os comandos disponíveis são:
- `make`: Compila o programa de testes e automaticamente o executa para o nó de id 0;
- `make id=<id>`: Compila o programa de testes e automaticamente o executa para o nó de id `id`;
- `make lib`: Compila a biblioteca estática e gera um arquivo `.a`;
- `make test`: Compila o programa de testes e gera um executável `program`;
- `make clean`: Remove os arquivos gerados pela build;
- `make dirs`: Cria os diretórios de build;

## Como testar

Primeiro, compile o programa de testes

```
make test
```

Isso criará um executável `program`. Cada nó é representado por um processo diferente. Por exemplo,

```
./program 0
```

Inicia o processo do nó 0 (definido no arquivo de configuração). Você deverá ver algo do tipo

```
21:32:38 INFO [lib/pipeline/channel/channel_layer.cpp:20] (0): Initialized receiver thread.
21:32:38 INFO [test/process_runner.cpp:232] (1): Local node endpoint is 127.0.0.1:3000.
>
```

Isso significa que 0 está pronto para se comunicar. Por exemplo, digite o seguinte comando no terminal e aperte Enter para fazer com que o nó 0 envie `Hello!` para si mesmo.

```
text "Hello!" -> 0
```
Isso gerará a seguinte sequência de logs:
```
> text "Hello!" -> 0
10:21:05 INFO [test/process_runner.cpp:147] (2): Executing command 'text', sending 'Hello!' to node 0.
10:21:05 INFO [lib/channels/channel.cpp:79] (3): Sent packet SYN 0/0 to 127.0.0.1:3000 (12 bytes).
10:21:06 INFO [lib/pipeline/fault_injection/fault_injection_layer.cpp:81] (4): Received SYN 0/0 from 127.0.0.1:3000 (0 bytes).
10:21:06 INFO [lib/channels/channel.cpp:79] (3): Sent packet SYN+ACK 0/0 to 127.0.0.1:3000 (12 bytes).
10:21:06 INFO [lib/pipeline/fault_injection/fault_injection_layer.cpp:81] (4): Received SYN+ACK 0/0 from 127.0.0.1:3000 (0 bytes).
10:21:06 INFO [lib/communication/connection.cpp:170] (4): syn_received: connection established.
10:21:06 INFO [lib/channels/channel.cpp:79] (3): Sent packet DATA+END 1/0 to 127.0.0.1:3000 (18 bytes).
10:21:07 INFO [lib/pipeline/fault_injection/fault_injection_layer.cpp:81] (4): Received DATA+END 1/0 from 127.0.0.1:3000 (6 bytes).
10:21:07 INFO [lib/channels/channel.cpp:79] (3): Sent packet ACK 1/0 to 127.0.0.1:3000 (12 bytes).
Received 'Hello!' (6 bytes) from 0
Saved message to file [messages/f7673f99-fae8-4139-ad28-8fe23cbd1eea].
10:21:07 INFO [lib/pipeline/fault_injection/fault_injection_layer.cpp:81] (4): Received ACK 1/0 from 127.0.0.1:3000 (0 bytes).
10:21:07 INFO [lib/pipeline/transmission/transmission_queue.cpp:140] (4): Transmission 127.0.0.1:3000 / 1 is completed. Sent 1 fragments, 6 bytes total.
10:21:07 INFO [test/process_runner.cpp:186] (2): Sent message.
>
```

É possível executar múltiplos comandos em paralelo escrevendo-os na mesma linha separados por `;`,

```
> text "Hello!" -> 0; text "Hello too" -> 1
```

Neste caso, o nó 0 enviará `Hello!` para si mesmo **ao mesmo tempo** que envia `Hello too` para 1 (para isso, seria necessário executar `./program 1` em um terminal separado para que a mensagem seja entregue).
Também é possível definir o comando a ser executado assim que o processo inicia execução especificando a flag `-s` com o comando cercado por aspas simples (`'`):

```
./program 1 -s '"hi" -> 0'
```

Isso seria equivalente a executar `"hi" -> 0` manualmente na linha de comando assim que o processo iniciasse.

### Comandos disponíveis
- `text <message> -> <id>`: Envia a string `message` para o nó `id`. A palavra-chave `text` pode ser omitida. Exemplos: `text "Hello world" -> 1`, `"Bye" -> 0`.
- `file <path> -> <id>`: Envia o arquivo em `path` para o nó `id`. Exemplo: `file "teste.png" -> 0` (envia o arquivo `teste.png` para 0).
- `dummy <size> -> <id>`: Envia um texto de teste de tamanho `size` para o nó `id`. Exemplo: `dummy 1 -> 0` (envia 1 byte pra 0), `dummy 50000 -> 1` (envia 50000 bytes a 1).
- `exit`. Encerra o processo.
- `help`. Exibe lista de comandos e flags disponíveis.

### Flags disponíveis
- `-s '<comandos>'`: Executa `comandos` assim que o processo for iniciado.
- `-f <fault-list>`: Define as falhas que devem ocorrer na recepção de cada pacote com base em uma lista de falhas fornecida. Exemplo: `./program 2 -f [0, L, 1000, 500]` fará com que o nó 2 receba o primeiro pacote sem atraso, perca o segundo, receba o terceiro com 1000ms de atraso e o quarto com 500ms de atraso, respectivamente. Obs: Todos os atrasos são relativos ao momento que o pacote é recebido pela porta UDP, logo, não sendo o atraso real do pacote na rede.
