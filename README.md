# Sistemas Operacionais II

> Projeto da disciplina de Sistemas Operacionais II (INE5424) do grupo A.

## Requisitos

- C++ 20

## Como compilar

Nosso projeto usa Makefile. Os comandos disponíveis são:
- `make`: Compila o programa de testes e automaticamente executa o shell para o nó de id 0;
- `make id=<id>`: Compila o programa de testes e automaticamente executa o shell para o nó de id `id`;
- `make lib`: Compila a biblioteca estática e gera um arquivo `.a`;
- `make program`: Compila o programa de testes e gera um executável `program`;
- `make test case=<case-file-path>`: Compila o programa de testes e automaticamente execute o caso de teste do arquivo especificado`;
- `make clean`: Remove os arquivos gerados pela build;
- `make dirs`: Cria os diretórios de build;

## Como testar

Primeiro, compile o programa de testes.

```
make program
```

Isso criará um executável `program`. A seguir, será mostrado alguns jeitos de como testar
o funcionamento da biblioteca.

### Executar o benchmark

O benchmark serve para testar a taxa de transmissão da biblioteca em diversas condições. Pode-se
executar o teste padrão da seguinte forma:

```
./program benchmark
```

O programa irá, por padrão, criar 5 nós que pertencem a um mesmo grupo e fará com que cada nó
transmita 100MiB de mensagens com o BEB. Isso produz a seguinte saída:

```
Starting benchmark.
Groups=1
Nodes=5 per group (5 total)
Bytes=100.000000MiB sent per node (500.000000MiB total)
Interval=0ms
Max Message Size=65418
Max Read Operations=0
Max Write Operations=4294967295
Message Mode=BEB
Encryption=enabled
Checksum=enabled
Alive=250
Max Inactivity Time=5000

0.0s | 0B/500.000MiB (0.00000%), time_left=infs, avg_in=0B/s, total_out=0B/s
0.1s | 127.770KiB/500.000MiB (0.02495%), time_left=556.9s, avg_in=0B/s, total_out=127.770KiB/s
0.2s | 191.654KiB/500.000MiB (0.03743%), time_left=646.3s, avg_in=114.992KiB/s, total_out=191.654KiB/s
0.3s | 191.654KiB/500.000MiB (0.03743%), time_left=921.3s, avg_in=255.539KiB/s, total_out=191.654KiB/s
0.4s | 319.424KiB/500.000MiB (0.06239%), time_left=712.8s, avg_in=319.424KiB/s, total_out=319.424KiB/s
0.5s | 319.424KiB/500.000MiB (0.06239%), time_left=873.0s, avg_in=332.200KiB/s, total_out=319.424KiB/s
0.6s | 447.193KiB/500.000MiB (0.08734%), time_left=737.8s, avg_in=383.309KiB/s, total_out=447.193KiB/s
0.7s | 447.193KiB/500.000MiB (0.08734%), time_left=852.2s, avg_in=408.862KiB/s, total_out=447.193KiB/s
0.8s | 511.078KiB/500.000MiB (0.09982%), time_left=846.7s, avg_in=472.747KiB/s, total_out=511.078KiB/s
0.9s | 511.078KiB/500.000MiB (0.09982%), time_left=946.8s, avg_in=511.078KiB/s, total_out=511.078KiB/s
1.0s | 511.078KiB/500.000MiB (0.09982%), time_left=1046.8s, avg_in=523.854KiB/s, total_out=511.078KiB/s
...
```

O teste encerra quando todos os nós enviarem todas as mensagens pendentes. É possível alterar o número de grupos,
número de nós e quantidade de bytes enviados, entre outras diversos parâmetros passados como argumentos do programa.
Abaixo, está uma lista de todos os possíveis argumentos.
- `--num-groups <int>`: Define o número de grupos que o programa terá.
- `--num-nodes <int>`: Define o número de nós que existirão em cada um dos grupos. Por exemplo, para `--num-groups 5 --num-nodes 3`, 15 nós serão criados no total.
- `--bytes-per-node <int><unit>`: Define o número de bytes que cada nó transmitirá. Além do número, pode ser especificado a unidade de medida utilizada. Exemplos: `1kb`, `10mb`, `1gb`.
- `--mode beb|urb|ab`: Seleciona qual dos modos de broadcast a hashmap distribuída utilizará.
- `--no-encryption`: Desabilita a criptografia.
- `--no-checksum`: Desabilita o checksum.
- `--max-message-size <int>`: Tamanho máximo das mensagens enviadas em bytes. Por exemplo, `--bytes-per-node 1kb --max-message-size 100` fará com que um nó envie 10 mensagens de 100 bytes.
- `--out-file [path]`: Define o caminho do arquivo em que os resultados do benchmark serão salvos, se essa flag não for passada, os resultados não serão armazenados. O arquivo gerado pelo benchmark estará no formato JSON.
- `--alive <int>`: Define o número de milissegundos entre o envio dos heartbeats. É necessário especificar por argumento, pois o programa de benchmark não lê arquivos de configuração convencionais (e.g `nodes.conf`). Por padrão, possui o 250.
- `--node-interval <min>..<max>`: Define o intervalo mínimo e máximo em milissegundos entre o envio de duas mensagens (se só um número for passado, ele será considerado como mínimo e máximo). Por exemplo: 50..200 define que os nós enviarão suas mensagens com um intervalo aleatório de 50 milissegundos a 200 milissegundos.
- `--read <int>`: Define o número máximo de operações de leitura da hash table (por send). Por padrão, possui valor 0.
- `--write <int>`: Define o número máximo de operações de escrita da hash table (por broadcast). Por padrão, possui valor máximo.

Para executar uma rotina de testes de benchmark pré-definida, execute o script `./bench/execute.sh`. Isto gerará diversos
arquivos `.json` em `bench/results/`. Para realizar a plotagem destes dados, execute `./bench/plot.sh`. Os plots serão
salvos em `./bench/plots/`.


### Executar o shell

Cada nó é representado por um processo diferente. Por exemplo,

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

É possível executar múltiplos comandos em sequência escrevendo-os na mesma linha separados por `;`,

```
> text "Hello!" -> 0; text "Hello too" -> 1
```

Neste caso, o nó 0 enviará `Hello!` para si mesmo e, após a confirmação da entrega, envia `Hello too` para 1 (para isso, seria necessário executar `./program 1` em um terminal separado para que a mensagem seja entregue).
Também é possível definir o comando a ser executado assim que o processo inicia execução especificando a flag `-s` com o comando cercado por aspas simples (`'`):

```
./program 1 -s '"hi" -> 0'
```

Isso seria equivalente a executar `"hi" -> 0` manualmente na linha de comando assim que o processo iniciasse.

### Comandos disponíveis
- `text <message> -> <id> [on <group>]`: Envia a string `message` para o nó `id` através do grupo `group`. A palavra-chave `text` pode ser omitida. Caso o grupo não seja informado, a mensagem é enviada através do grupo público. Exemplos: `text "Hello world" -> 1`, `"Bye" -> 0 on A`.
- `file <path> -> <id> [on <group>]`: Envia o arquivo em `path` para o nó `id`. Exemplo: `file "teste.png" -> 0` (envia o arquivo `teste.png` para 0).
- `dummy <size> -> <id> [on <group>]`: Envia um texto de teste de tamanho `size` para o nó `id`. Exemplo: `dummy 1 -> 0` (envia 1 byte pra 0), `dummy 50000 -> 1` (envia 50000 bytes a 1).
- `kill`: Mata o nó local.
- `init`: Inicia o nó local caso esteja morto.
- `sleep <ms>`: Faz com que o nó durma por `ms` milésimos.
- `repeat <n> <subcommand>`: Faz com que um subcomando seja executado `n` vezes.
- `[<subcommand_1>, <subcommand_2>, ..., <subcommand_n>]`: Executa os comandos sequencialmente. Aguarda a finalização de todos os subcomandos, mesmo que assíncronos (`async`). Útil como subcomando de outros comandos, como, por exemplo, `repeat 3 ["a" -> 0; "b" -> 1]`, `async [sleep 1000; "c" -> 3]`. 
- `async <subcommand>`: Executa um subcomando em paralelo. Por exemplo, `async "a" -> 0; "b" -> 1` fará o envio de `a` para `0`
ao mesmo tempo que envia `b` para `1`. Pode-se usar a sintaxe `[...]` para sincronizar um conjunto de subcomandos assíncronos.
Por exemplo, `[async "c" -> 2; async "d" -> 3]; kill` fará com que o nó envie `c` para `2` ao mesmo tempo que envia `d` a `3`, entretanto o comando `kill` só será executado quando os dois comandos `async` encerrarem.
- `fault {drop <number>[/<fragment>][u|b|a] [<n>% | <n>x],}`: Injeta falhas no nó local. Uma lista de falhas pode ser especificada. No caso da falha `drop`, fará com que a mensagem de número `number`, fragmento `fragment` de tipo `u`, `b` ou `a` (todos, se omitido), será perdida `n` vezes (caso seja `nx`) ou será perdido com `n`% de chance. `number` e `fragment` podem ser intervalos de inteiros e.g `1`, `1..2`, `*` (equivalente a `0..MAX_INT`).Segue abaixo alguns exemplos de uso:
    - `fault drop 1`: Perca todos os fragmentos da mensagem de número 1, indepedente do tipo de sequência (unicast, broadcast, atomic ou heartbeat)
    - `fault drop 1..3`: Perca todos os fragmentos das mensagens de número 1 a 3 (inclusivo), independente do tipo de sequência.
    - `fault drop 4b`: Perca a mensagem de número 4 de broadcast.
    - `fault drop *`: Perca todos os fragmentos de todas as mensagens.
    - `fault drop */0`: Perca o fragmento de número 0 de todas as mensagens.
    - `fault drop 1 30%`: Perca todos os fragmentos da mensagem de número 1 com 30% de chance.
    - `fault drop * 1x`: Perca o próximo pacote.
    - `fault drop 1u 1x`: Perca uma vez qualquer fragmento da mensagem de unicast de número 1.
    - `fault drop * 5%`: Perca qualquer pacote com 5% de chance (similar ao campo `faults` da configuração)
    - `fault drop * 5%, drop 1/0 1x`: Perca qualquer pacote com 5% de chance e perca o fragmento 0 da mensagem de número 1 uma vez (100% de chance).
- `node list`: Lista os nós conhecidos.
- `node discover <endpoint>`: Adiciona o nó em `endpoint` à lista de nós conhecidos e tenta fazer o _discover_ do mesmo. Exemplo: `node discover 127.0.0.1:3000`.
- `group join <name> [<key>]`: Entra no grupo `name` identificado pela chave `key`. A chave `key` pode ser omitida caso o grupo esteja registrado no arquivo de configuração `nodes.conf`. Exemplos: `group join novo-grupo AF87BFD4F90DDF851A66923B694627AC651871D84033E22A35BE3F97465B4CB3`, `group join A`.
- `group leave <name>`: Sai do grupo `name`.
- `exit`: Encerra o processo.
- `help`: Exibe lista de comandos e flags disponíveis.

### Flags disponíveis
- `-s '<comandos>'`: Executa `comandos` assim que o processo for iniciado.
