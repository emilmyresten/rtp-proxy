#!/bin/bash
set -m # enable job control

declare -A delayMode
delayMode["jittery_rtp_to_lc"]="-1"
delayMode["rtp_to_lc"]="0"
delayMode["arrival_time"]="1"
delayMode["origin_rtp_timestamp"]="3"



DESTINATION=$1

if [ $# != 1 ]
then
    echo "Supply a destination for the test run."
    exit
fi

if [ -d "./data/$DESTINATION" ]
then
    echo "Please provide a unique foldername. Don't want to overwrite data."
    exit
fi

mkdir "./data/$DESTINATION"

for algorithm in "arrival_time" "origin_rtp_timestamp" "rtp_to_lc" "jittery_rtp_to_lc"
do
    mkdir "./data/$DESTINATION/$algorithm"
    for scenario in {1..3}
    do
        mkdir "./data/$DESTINATION/$algorithm/scenario_$scenario"

        echo "Starting scenario $scenario for $algorithm using delayMode ${delayMode[$algorithm]}, saving to ./data/$DESTINATION/$algorithm/scenario_$scenario"
        
        cat rist_cmd | sed "s/DELAY_MODE/${delayMode[$algorithm]}/" | /home/emimyr/Documents/code/rist/build/deliverables/server/ristserver &
        sleep 2s

        ./setup_scenario_$scenario.sh $DESTINATION/$algorithm/scenario_$scenario & 
        
        sleep 1.005h # 1 hour and 18 seconds.

        killall ristserver
        killall rtp_proxy
    done;
done;

echo "Finished."