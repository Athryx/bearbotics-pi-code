# vision-manager

This program's job is to listen for mqtt commands from the broker, and switch modes between vision and rmote viewing.

It is also in charge of starting the gstreamer remote viewing pipeline.

To view the remote viewing, copy the vlc-format.sdp to the driverstation, and run:

```
vlc vlc-format.sdp
```
