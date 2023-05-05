#!/bin/bash
set -m # enable job control

declare -A delayMode
delayMode["jittery_rtp_to_lc"]="-1"
delayMode["rtp_to_lc"]="0"
delayMode["arrival_time"]="1"
delayMode["origin_rtp_timestamp"]="3"
delayMode["cumulative_ratio_estimate"]="4"
delayMode["cumulative_ratio_estimate_skip_transient"]="5"
delayMode["fuzzy_filtering_estimate_incautious"]="6"
delayMode["fuzzy_filtering_estimate_cautious"]="7"

DESTINATION=$1
# SCENARIO=$2

if [ $# != 1 ]
then
    echo "Supply a destination for the quick test (10m)."
    exit
fi

if [ -d "./data/$DESTINATION" ]
then
    echo "Please provide a unique foldername. Don't want to overwrite data."
    exit
fi

cleanup() {
    sudo killall ristserver
    sudo killall rtp_proxy
    echo "Cleanup performed."
    exit 1
}

# Register the cleanup function as a handler for SIGINT
trap cleanup SIGINT

mkdir "./data/$DESTINATION" 

SCENARIO=1

for algorithm in "origin_rtp_timestamp" "cumulative_ratio_estimate" # "rtp_to_lc"  "arrival_time"  "jittery_rtp_to_lc"  "cumulative_ratio_estimate" "cumulative_ratio_estimate_skip_transient"
do
    mkdir "./data/$DESTINATION/$algorithm" "./data/$DESTINATION/$algorithm/scenario_$SCENARIO"

    echo "Starting 10 min scenario $SCENARIO for $algorithm using delayMode ${delayMode[$algorithm]}, saving to ./data/$DESTINATION/$algorithm/scenario_$SCENARIO"
    
    cat rist_cmd | sed "s/DELAY_MODE/${delayMode[$algorithm]}/" | /home/emimyr/Documents/code/rist/build/deliverables/server/ristserver &
    sleep 2s
    ./setup_scenario_$SCENARIO.sh $DESTINATION/$algorithm/scenario_$SCENARIO & 
    
    sleep 5m
    killall ristserver
    killall rtp_proxy
done;
echo "Finished."