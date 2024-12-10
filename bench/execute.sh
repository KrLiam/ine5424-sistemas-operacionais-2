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

## URB
./program benchmark --out-file "bench/results/nodes-urb/A_-_1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/B_-_2_Nodes.json" --num-groups 1 --num-nodes 2 --bytes-per-node $(($MB/2))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/C_-_3_Nodes.json" --num-groups 1 --num-nodes 3 --bytes-per-node $(($MB/3))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/D_-_4_Nodes.json" --num-groups 1 --num-nodes 4 --bytes-per-node $(($MB/4))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/E_-_5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/F_-_7_Nodes.json" --num-groups 1 --num-nodes 7 --bytes-per-node $(($MB/7))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/G_-_10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/H_-_15_Nodes.json" --num-groups 1 --num-nodes 15 --bytes-per-node $(($MB/15))mb --mode urb --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-urb/I_-_20_Nodes.json" --num-groups 1 --num-nodes 20 --bytes-per-node $(($MB/20))mb --mode urb --no-encryption --no-checksum

# ## AB
./program benchmark --out-file "bench/results/nodes-ab/A_-_20_Nodes.json" --num-groups 1 --num-nodes 20 --bytes-per-node $(($MB/20))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/B_-_15_Nodes.json" --num-groups 1 --num-nodes 15 --bytes-per-node $(($MB/15))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/C_-_10_Nodes.json" --num-groups 1 --num-nodes 10 --bytes-per-node $(($MB/10))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/D_-_5_Nodes.json" --num-groups 1 --num-nodes 5 --bytes-per-node $(($MB/5))mb --mode ab --no-encryption --no-checksum
./program benchmark --out-file "bench/results/nodes-ab/E_-_1_Node.json" --num-groups 1 --num-nodes 1 --bytes-per-node $(($MB/1))mb --mode ab --no-encryption --no-checksum


# challenge

## ab
./program benchmark --out-file "bench/results/challenge/AB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab
./program benchmark --out-file "bench/results/challenge/AB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab --no-encryption
./program benchmark --out-file "bench/results/challenge/AB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode ab --no-encryption --no-checksum

## urb
./program benchmark --out-file "bench/results/challenge/URB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb
./program benchmark --out-file "bench/results/challenge/URB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb --no-encryption
./program benchmark --out-file "bench/results/challenge/URB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode urb --no-encryption --no-checksum

## beb
./program benchmark --out-file "bench/results/challenge/BEB_-_Normal.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb
./program benchmark --out-file "bench/results/challenge/BEB_-_No_encryption.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb --no-encryption
./program benchmark --out-file "bench/results/challenge/BEB_-_No_encryption_and_checksum.json" --num-groups 3 --num-nodes 10 --bytes-per-node 100mb --mode beb --no-encryption --no-checksum
