#!/bin/sh

#vlc --network-caching 50 --live-caching 0 --sout-mux-caching 10
vlc --network-caching 50 --live-caching 0 --sout-mux-caching 10 vlc-config.sdp


# milliseconds
#vlc --network-caching 50 --mms-timeout 1000000000 vlc-config.sdp
# seconds
#vlc --network-caching 50 --rtp-timeout 1000000 vlc-config.sdp
# seconds
#vlc --network-caching 50 --udp-timeout 1000000 vlc-config.sdp
# milliseconds
#vlc --network-caching 50 --netsync-timeout 1000000000 vlc-config.sdp
# milliseconds
#vlc --network-caching 50 --preparse-timeout 1000000000 vlc-config.sdp

# vlc -v 2 --network-caching 50 --ipv4-timeout 1000000 --mms-timeout 1000000000 --rtp-timeout 1000000 --udp-timeout 1000000000 --preparse-timeout 1000000000 vlc-config.sdp
#vlc -v 2 --network-caching 50 --mms-timeout 1000000000 --rtp-timeout 1000000 --udp-timeout 1000000000 --preparse-timeout 1000000000 vlc-config.sdp
