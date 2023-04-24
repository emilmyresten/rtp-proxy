#!/bin/bash

SCENARIO=2
LATENCY=100ms
JITTER=1ms
CORRELATION=50%
DISTRIBUTION=pareto

NS=network_sim
NS_IF=veth1
LOCAL_IF=veth0
NS_IP=172.16.0.1
LOCAL_IP=172.16.0.2


create_network_namespace() {
    ip netns add $NS
    ip link add $LOCAL_IF type veth peer name $NS_IF
    ip link set $NS_IF netns $NS
    ip -n $NS addr add $NS_IP/24 dev $NS_IF
    ip -n $NS link set $NS_IF up

    ip addr add $LOCAL_IP/24 dev $LOCAL_IF
    ip link set $LOCAL_IF up

}

reset_network_namespace() {
    ip link delete $LOCAL_IF
    ip netns exec $NS ip link delete $NS_IF
    ip netns del $NS
}

start_traffic_shaping() {
    echo " == SHAPING TRAFFIC ON INTERFACE $LOCAL_IF, ACCORDING TO SCENARIO $SCENARIO =="
    
    # limit based on https://stackoverflow.com/questions/18792347/what-does-option-limit-in-tc-netem-mean-and-do
    # rate based on https://lists.linuxfoundation.org/pipermail/netem/2018-May/001691.html (and it works)
    tc qdisc add dev $LOCAL_IF root handle 1: netem delay $LATENCY $JITTER $CORRELATION distribution $DISTRIBUTION limit 1000000 rate 2gbit # reorder 1%
    tc qdisc show dev $LOCAL_IF
}

mkdir ./data/$1

reset_traffic_shaping() {
    tc qdisc del dev $LOCAL_IF root
}

start_measure_point() {
    ./build/main 9006 2>./data/$1/reconstructed.txt &
}

start_jittered_proxy() {
    ip netns exec $NS ./build/main 9003 $LOCAL_IP 9005 9004 2>./data/$1/jittered.txt &
}

start_initial_proxy() {
    ./build/main 9001 $NS_IP 9003 9002 2>./data/$1/initial.txt &
}

reset_network_namespace
create_network_namespace

reset_traffic_shaping
start_traffic_shaping

start_initial_proxy $1
start_jittered_proxy $1
start_measure_point $1

ps T
