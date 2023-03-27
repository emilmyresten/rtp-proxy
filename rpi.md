
### SSH to the RPI from linux via direct ethernet:
1. Connect directly using ethernet
2. Settings -> Network -> Wired (click cogwheel) -> IPv4 -> Shared to other computers 
3. `ip r` to find newly created subnet
4. nmap <subnet> to find the rpi IP (only other host on network)
5. ssh <user>@<IP>


### start stream from RPI to linux using FFMPEG
`ffmpeg -f lavfi -i testsrc=size=640x480:rate=30 -f rtp "rtp://<ip>:<port>?pkt_size=1024"`

should work fine if ip is correct and port is same as proxy is running on.


### Disable ntp on rpi
```
systemctl disable systemd-timesyncd.service
```
