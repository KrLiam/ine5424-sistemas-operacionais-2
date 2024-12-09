
MB=250

# Variando o número de nós

## BEB

### Throughput
python3 bench/plot.py \
    --files bench/results/nodes-beb/* \
    --title "BEB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-beb-throughput.png

### Progress
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

### Throughput
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-urb-throughput.png

### Progress
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/node-urb-progress.png


## AB

### Throughput
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/node-ab-throughput.png

### Progress
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/node-ab-progress.png



# Guto Challenge

## No encryption and checksum

### Throughput
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


## No encryption

### Throughput
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


## Normal

### Throughput
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


## BEB

### Throughput
python3 bench/plot.py \
    --files bench/results/guto-challenge/BEB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/BEB_-_No_encryption.json \
            bench/results/guto-challenge/BEB_-_Normal.json \
    --title "BEB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-beb-throughput.png


## URB

### Throughput
python3 bench/plot.py \
    --files bench/results/guto-challenge/URB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/URB_-_No_encryption.json \
            bench/results/guto-challenge/URB_-_Normal.json \
    --title "URB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-urb-throughput.png


## AB

### Throughput
python3 bench/plot.py \
    --files bench/results/guto-challenge/AB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/AB_-_No_encryption.json \
            bench/results/guto-challenge/AB_-_Normal.json \
    --title "AB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column total_out \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Total outbound throughput (MB)" \
    --y-scale 1000000 \
    --output bench/plots/guto-challenge-ab-throughput.png