#!/bin/bash

IF=lo
DPORT=9024
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
    tc qdisc del dev lo root
}

start_measure_point_in_bg() {
    ./build/main 9026 2>./data/$1/reconstructed.txt &
}

start_proxy_in_bg() {
    ./build/main 9001 9024 9002 2>./data/$1/initial.txt &
}



start_traffic_shaping
start_measure_point_in_bg $1
start_proxy_in_bg $1

ps T

# reset_traffic_shaping