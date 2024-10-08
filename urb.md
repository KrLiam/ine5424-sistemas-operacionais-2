
Algoritmo original:
```
Implements:
    UniformReliableBroadcast, instance urb.

Uses:
    BestEffortBroadcast, instance beb
    PerfectFailureDetector, instance P

Variables:
    correct = Π # The set of processes considered correct
    delivered = ∅
    pending = ∅
    for all m:
        ack[m] = ∅

Upon urb.broadcast(m):
    pending = pending ∪ [self,m]
    beb.broadcast([self, m])

Upon beb.deliver(p, [s, m]) # s is the id of the source
    ack[m] = ack[m] ∪ p
    if [s, m] /∈ pending:
        pending = pending ∪ [s, m]
        beb.broadcast([s, m])
    tryDeliver([s,m])

Upon event crash(q) raised by P:
    correct = correct \ {q}
    for all [s, m] ∈ pending:
        tryDeliver([s,m])

Function canDeliver(m):
    return (correct ⊆ ack[m]) # we receive m from all correct processes

Function tryDeliver([s, m]):
    if canDeliver(m) and m /∈ delivered:
        delivered = delivered ∪ m
        Trigger urb.deliver([s, m])
```

Os subconjuntos `delivered` e `pending` foram substituídos por um
sistema de números de sequência. Uma mensagem `m` de origem `s` e número `n` é dada como entregue se `n < expected_num[s]` e `[s, n, m]` não está na fila de entrega `delivery_queue`.

```
Implements:
    UniformReliableBroadcast, instance urb.

Uses:
    BestEffortBroadcast, instance beb
    PerfectFailureDetector, instance P

Variables:
    correct = Π # The set of processes considered correct
    delivered = ∅

    num = 0
    for all s:
        expected_num[s] = 0
    delivery_queue = ∅
    for all m:
        ack[m] = ∅

Upon urb.broadcast(m):
    num += 1
    beb.broadcast([self, num, m])

Upon beb.deliver(p, [s, n, m]) # s is the id of the source, n is the message number
    ack[m] = ack[m] ∪ p

    if n == expected_num[s]:
        expected_num[s] += 1
        delivery_queue = delivery_queue U {[s, n, m]}
        beb.broadcast([s, n, m])

    tryDeliver([s, n, m])
        
Upon event crash(q) raised by P:
    correct = correct \ {q}
    for all [s, n, m] ∈ delivery_queue:
        tryDeliver([s, n, m])

Function canDeliver(m):
    return (correct ⊆ ack[m]) # we receive m from all correct processes

Functio isDelivered([s, n, m]):
    return n < expected_num[s] and [s, n, m] /∈ delivery_queue

Function tryDeliver([s, n, m]):
    if canDeliver(m) and not isDelivered([s, n, m]):
        delivery_queue = delivery_queue - {[s, n, m]}
        Trigger urb.deliver([s, n, m])
```

Problemas do algoritmo acima:
- Não garante ordem de entrega para mensagens de mesma origem. Para isso, `delivery_queue` deveria ser, de fato, uma fila e apenas a primeira entrada de uma dada origem pode ser entregue.
- Para um grupo de `N` nós, a mensagem é retransmitida `N` vezes. Se `N` for relativamente pequeno e a perda de pacotes for alta, pode ser que todos os pacotes sejam perdidos e algum nó não receba a mensagem. Para isso, deve-se realizar time outs de entradas na `delivery_queue` que acionam a retransmissão da mensagem até que o ACK de todos os processos corretos sejam recebidos. Obs: Os receptores não podem, em hipotése alguma, cancelar a entrega de uma mensagem por limite de retransmissão, pois não receber um ACK de um nó `p` não significa que o nó `p` não entregou a mensagem, mas que o ACK pode ter sido apenas perdido. Neste caso, aguarda-se o anúncio do heartbeat para tomar `p` como morto e ajustar o conjunto de processos corretos.