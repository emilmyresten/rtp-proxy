# RTP proxy

C++ program to note the interdeparture times of RTP packets and simulate network jitter using a pareto distribution.

## Passing RTP using FFMPEG

### `Stream generation`

```
ffmpeg -f lavfi -i testsrc=size=640x480:rate=30 -f rtp "rtp://<ip>:<port>?pkt_size=1024"
```

If a new sdp file is required, run with `-sdp_file saved.sdp` once to generate the session description. Don't forget to update the port within this file to make sure the player listens to the proxy.

### `Proxy`:
```
./main <from> <to> <via>
```

This enables using multiple instances of the RTP proxy simultaneously, so that the RIST server can transmit data to an instance of the RTP proxy for the evaluation of playout schedulers.

### `Player`:

(be sure to specify the path to the .sdp file correctly (it is in the assets folder.))

```
ffplay -i saved.sdp -protocol_whitelist file,udp,rtp
```

## Build

```
mkdir build && cd build
```

```
cmake .. && make
```


## Data collection
Collect data by piping stderr to file, e.g. 
```
./main 2>./test_data.txt
```


## TC / NetEm

Show all queueing rules on loopback
```
sudo tc qdisc del dev lo root
```

Remove all queueing rules from loopback
```
sudo tc qdisc del dev lo root
```
## Scripts
The three scenarios are setup with the setup script. 
Just specify the folder in which to store the data as an argument to the script. 
Run with sudo i.e. sudo ./setup_scenario_1.sh 23-04-20.
To run all scenarios for all playout algorithms automatically in succession
run `sudo ./run_all.sh <destination-folder>`