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

`Shape only the traffic going to a specific port.`
```
sudo tc qdisc add dev lo root handle 1: prio
sudo tc filter add dev lo parent 1: protocol ip u32 match ip dport 9024 0xffff flowid 1:1
sudo tc qdisc add dev lo parent 1:1 netem delay 100ms 10ms distribution normal
```
```
sudo tc qdisc del dev lo root handle 1: prio
sudo tc qdisc del dev lo parent 1:1 netem
sudo tc filter del dev lo parent 1: protocol ip u32 match ip dport 9024 0xffff flowid 1:1
```