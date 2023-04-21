#!/bin/bash

IF=lo
DPORT=9003
SCENARIO=1
LATENCY=100ms
JITTER=10ms
CORRELATION=100%
DISTRIBUTION=pareto


start_traffic_shaping() {
    echo " == SHAPING TRAFFIC ON INTERFACE $IF, ACCORDING TO SCENARIO $SCENARIO =="
    
    tc qdisc add dev $IF root handle 1: prio
    tc filter add dev $IF parent 1: protocol ip u32 match ip dport $DPORT 0xffff flowid 1:1
    tc qdisc add dev $IF parent 1:1 netem delay $LATENCY $JITTER $CORRELATION distribution $DISTRIBUTION

    tc qdisc show dev lo
}

mkdir ./data/$1

reset_traffic_shaping() {
    tc qdisc del dev $IF root
}

## ffmpeg to port 9001 (initial) -> jitter to port 9003 (jittered) -> rist server on port 9005 -> reconstructed measure on 9006.

start_measure_point() {
    ./build/main 9006 2>./data/$1/reconstructed.txt &
}

start_jittered_proxy() {
    ./build/main 9003 9005 9004 2>./data/$1/jittered.txt &
}

start_initial_proxy() {
    ./build/main 9001 9003 9002 2>./data/$1/initial.txt &
}


reset_traffic_shaping
start_traffic_shaping
start_initial_proxy $1
start_jittered_proxy $1
start_measure_point $1

ps T
