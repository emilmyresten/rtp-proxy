#!/bin/bash
set -m # enable job control

DESTINATION=$1
# SCENARIO=$2

if [ $# != 1 ]
then
    echo "Supply a destination for the rtp_to_lc test."
    exit
fi

if [ -d "./data/$DESTINATION" ]
then
    echo "Please provide a unique foldername. Don't want to overwrite data."
    exit
fi

# if (( $SCENARIO < 1 || $SCENARIO > 3 ))
# then
#     echo "Enter a scenario from 1 to 3."
#     exit
# fi

mkdir "./data/$DESTINATION" "./data/$DESTINATION/rtp_to_lc"

for scenario in {1..3}
do
    mkdir "./data/$DESTINATION/rtp_to_lc/scenario_$scenario"

    echo "Starting 10 min scenario $scenario for rtp_to_lc using delayMode 0, saving to ./data/$DESTINATION/rtp_to_lc/scenario_$scenario"
    
    cat rist_cmd | sed "s/DELAY_MODE/0/" | /home/emimyr/Documents/code/rist/build/deliverables/server/ristserver &
    sleep 2s
    ./setup_scenario_$scenario.sh $DESTINATION/rtp_to_lc/scenario_$scenario & 
    
    sleep 10m
    killall ristserver
    killall rtp_proxy
done;
echo "Finished."