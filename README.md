# Sistemas Operacionais II

> Projeto da disciplina de Sistemas Operacionais II (INE5424) do grupo A.

## Como Testar

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
- `text <message> -> <id>`: Envia a string `message` o nó `id`. A palavra-chave `text` pode ser omitida. Exemplos: `text "Hello world" -> 1`, `"Bye" -> 0`.
- `dummy <size> -> <id>`: Envia um texto de teste de tamanho `size` para o nó `id`. Exemplo: `dummy 1 -> 0` (envia 1 byte pra 0), `dummy 50000 -> 1` (envia 50000 bytes a 1).
- `exit`. Encerra o processo.

### Flags disponíveis
- `-s '<comandos>'`: Executa `comando` assim que o processo for iniciado.
- `-f <fault-list>`: Define as falhas que devem ocorrer na recepção de cada pacote com base em uma lista de falhas fornecida. Exemplo: `./program 2 -f [0, L, 1000, 500]` fará com que o nó 2 receba o primeiro pacote sem atraso, perca o segundo, receba o terceiro com 1000ms de atraso e o quarto com 500ms de atraso, respectivamente. Obs: Todos os atrasos são relativos ao momento que o pacote é recebido pela porta UDP, logo, não sendo o atraso real do pacote na rede.