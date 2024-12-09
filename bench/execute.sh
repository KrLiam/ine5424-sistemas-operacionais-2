#!/bin/bash

make program


# Grafico variando num-nodes
MB=250

## BEB
./program benchmark --out-file "bench/results/nodes-beb/A_-_20_Nodes.json" --num-groups 1 --num-nodes 20 --bytes-per-node $(($MB/20))mb --mode beb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-beb/B_-_15_Nodes.json" --num-groups 1 --num-nodes 15 --bytes-per-node $(($MB/15))mb --mode beb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-beb/C_-_10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode beb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-beb/D_-_5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode beb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-beb/E_-_1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode beb --no-encryption --no-checksum
python3 bench/plot.py \
    --files bench/results/nodes-beb/* \
    --title "BEB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-beb-throughput.png
python3 bench/plot.py \
    --files bench/results/nodes-beb/* \
    --title "BEB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/node-beb-progress.png

## URB
./program benchmark --out-file "bench/results/nodes-urb/A_-_20_Nodes.json" --num-groups 1 --num-nodes 20 --bytes-per-node $(($MB/20))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/B_-_15_Nodes.json" --num-groups 1 --num-nodes 15 --bytes-per-node $(($MB/15))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/C_-_10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/D_-_5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/E_-_1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode urb --no-encryption --no-checksum
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-urb-throughput.png
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/node-urb-progress.png

# ## AB
./program benchmark --out-file "bench/results/nodes-ab/A_-_20_Nodes.json" --num-groups 1 --num-nodes 20 --bytes-per-node $(($MB/20))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/B_-_15_Nodes.json" --num-groups 1 --num-nodes 15 --bytes-per-node $(($MB/15))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/C_-_10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/D_-_5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/E_-_1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode ab --no-encryption --no-checksum
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-ab-throughput.png
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/node-ab-progress.png

# # guto challenge
## ab
./program benchmark --out-file "bench/results/guto-challenge/AB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab
./program benchmark --out-file "bench/results/guto-challenge/AB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab --no-encryption
./program benchmark --out-file "bench/results/guto-challenge/AB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab --no-encryption --no-checksum
## urb
./program benchmark --out-file "bench/results/guto-challenge/URB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb
./program benchmark --out-file "bench/results/guto-challenge/URB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb --no-encryption
./program benchmark --out-file "bench/results/guto-challenge/URB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb --no-encryption --no-checksum
## beb
./program benchmark --out-file "bench/results/guto-challenge/BEB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb
./program benchmark --out-file "bench/results/guto-challenge/BEB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/guto-challenge/BEB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb --no-encryption --no-checksum

python3 bench/plot.py \
    --files bench/results/guto-challenge/BEB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/URB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/AB_-_No_encryption_and_checksum.json \
    --title "BEBxURBxAB - 3 Groups 10 Nodes Each, 3GB, No encryption, No checksum" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-no-encryption-checksum.png

python3 bench/plot.py \
    --files bench/results/guto-challenge/BEB_-_No_encryption.json \
            bench/results/guto-challenge/URB_-_No_encryption.json \
            bench/results/guto-challenge/AB_-_No_encryption.json \
    --title "BEBxURBxAB - 3 Groups 10 Nodes Each, 3GB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-no-encryption-checksum.png

python3 bench/plot.py \
    --files bench/results/guto-challenge/BEB_-_Normal.json \
            bench/results/guto-challenge/URB_-_Normal.json \
            bench/results/guto-challenge/AB_-_Normal.json \
    --title "BEBxURBxAB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-no-encryption-checksum.png