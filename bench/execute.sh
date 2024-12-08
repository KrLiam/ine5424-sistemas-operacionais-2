#!/bin/bash

make program


# Grafico variando num-nodes
MB=500

## BEB
./program benchmark --out-file "bench/results/nodes-beb/10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/nodes-beb/7_Nodes.json" --num-groups 1 --num-nodes 7 --bytes-per-node $(($MB/7))mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/nodes-beb/5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/nodes-beb/3_Nodes.json" --num-groups 1 --num-nodes 3 --bytes-per-node $(($MB/3))mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/nodes-beb/1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode beb --no-encryption
python3 bench/plot.py \
    --files bench/results/nodes-beb/* \
    --title "BEB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-beb.png

## URB
./program benchmark --out-file bench/results/nodes-beb/10 Nodes.json --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode urb --no-encryption
./program benchmark --out-file bench/results/nodes-urb/7 Nodes.json --num-groups 1 --num-nodes 7 --bytes-per-node $(($MB/7))mb --mode urb --no-encryption
./program benchmark --out-file bench/results/nodes-urb/5 Nodes.json --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode urb --no-encryption
./program benchmark --out-file bench/results/nodes-urb/3 Nodes.json --num-groups 1 --num-nodes 3 --bytes-per-node $(($MB/3))mb --mode urb --no-encryption
./program benchmark --out-file bench/results/nodes-urb/1 Node.json --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode urb --no-encryption
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-beb.png

# ## AB
./program benchmark --out-file bench/results/nodes-ab/10 Nodes.json --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode ab --no-encryption
./program benchmark --out-file bench/results/nodes-ab/7 Nodes.json --num-groups 1 --num-nodes 7 --bytes-per-node $(($MB/7))mb --mode ab --no-encryption
./program benchmark --out-file bench/results/nodes-ab/5 Nodes.json --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode ab --no-encryption
./program benchmark --out-file bench/results/nodes-ab/3 Nodes.json --num-groups 1 --num-nodes 3 --bytes-per-node $(($MB/3))mb --mode ab --no-encryption
./program benchmark --out-file bench/results/nodes-ab/1 Node.json --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode ab --no-encryption
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-beb.png

# # guto challenge
# ## ab
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode ab
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode ab --no-encryption
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode ab --no-encryption --no-checksum
# ## urb
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode urb
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode urb --no-encryption
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode urb --no-encryption --no-checksum
# ## beb
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode beb
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode beb --no-encryption
# ./program benchmark --num-groups 3 --num-nodes 10 --bytes-per-node 100mib --mode beb --no-encryption --no-checksum


