
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
    --output bench/plots/png/node-beb-throughput.png \
             bench/plots/svg/node-beb-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/nodes-beb/* \
    --title "BEB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/node-beb-progress.png \
             bench/plots/svg/node-beb-progress.svg


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
    --output bench/plots/png/node-urb-throughput.png \
             bench/plots/svg/node-urb-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/nodes-urb/* \
    --title "URB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/node-urb-progress.png \
             bench/plots/svg/node-urb-progress.svg


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
    --output bench/plots/png/node-ab-throughput.png \
             bench/plots/svg/node-ab-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/nodes-ab/* \
    --title "AB - 1 Group, ${MB}MB, No encryption" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/node-ab-progress.png \
             bench/plots/svg/node-ab-progress.svg



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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-all-no-encryption-checksum.png \
             bench/plots/svg/guto-challenge-all-no-encryption-checksum.svg


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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-all-no-encryption.png \
             bench/plots/svg/guto-challenge-all-no-encryption.svg


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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-all-normal.png \
             bench/plots/svg/guto-challenge-all-normal.svg


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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-beb-throughput.png \
             bench/plots/svg/guto-challenge-beb-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/guto-challenge/BEB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/BEB_-_No_encryption.json \
            bench/results/guto-challenge/BEB_-_Normal.json \
    --title "BEB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/guto-challenge-beb-progress.png \
             bench/plots/svg/guto-challenge-beb-progress.svg


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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-urb-throughput.png \
             bench/plots/svg/guto-challenge-urb-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/guto-challenge/URB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/URB_-_No_encryption.json \
            bench/results/guto-challenge/URB_-_Normal.json \
    --title "URB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/guto-challenge-urb-progress.png \
             bench/plots/svg/guto-challenge-urb-progress.svg


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
    --smooth-factor 100 \
    --output bench/plots/png/guto-challenge-ab-throughput.png \
             bench/plots/svg/guto-challenge-ab-throughput.svg

### Progress
python3 bench/plot.py \
    --files bench/results/guto-challenge/AB_-_No_encryption_and_checksum.json \
            bench/results/guto-challenge/AB_-_No_encryption.json \
            bench/results/guto-challenge/AB_-_Normal.json \
    --title "AB - 3 Groups 10 Nodes Each, 3GB" \
    --x-column elapsed_time \
    --y-column progress_ratio \
    --x-label "Elapsed time (Seconds)" \
    --y-label "Progress (%)" \
    --y-scale 0.01 \
    --output bench/plots/png/guto-challenge-ab-progress.png \
             bench/plots/svg/guto-challenge-ab-progress.svg