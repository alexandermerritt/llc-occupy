#! /usr/bin/env bash
set -e
set -u

# Measure cycles to load cache lines from all sockets.

NODES=16
SIZE=$((1<<28))

for ((node=0; node<$NODES; node++)); do
    echo -n "socket $node "
    numactl --physcpubind=0 --membind=$node \
        ./occupy $SIZE $SIZE 1
done
