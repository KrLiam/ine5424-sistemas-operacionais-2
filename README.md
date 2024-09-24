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

Você deverá ver algo do tipo

```
21:32:38 INFO [lib/pipeline/channel/channel_layer.cpp:20] (0): Initialized receiver thread.
21:32:38 INFO [test/process_runner.cpp:232] (1): Local node endpoint is 127.0.0.1:3000.
>
```

Isso significa que 0 está pronto para se comunicar. Por exemplo, digite o seguinte comando no terminal e aperte Enter para fazer com que 0 envie `Hello!` para si mesmo.

```
> text "Hello!" -> 0
<INSERIR LOG AQUI> 
```

**Comandos disponíveis:**
- `text <message> -> <id>`: Envia a string `message` o nó `id`. A palavra-chave `text` pode ser omitida. Exemplos: `text "Hello world" -> 1`, `"Bye" -> 0`.
- `dummy <size> -> <id>`: Envia um texto de teste de tamanho `size` para o nó `id`. Exemplo: `dummy 1 -> 0` (envia 1 byte pra 0), `dummy 50000 -> 1` (envia 50000 bytes a 1).
- `exit`. Encerra o processo.