# The Deets

![](https://cdn.discordapp.com/attachments/370454293192507396/1144287993260146748/image.png)

Replay set: https://drive.google.com/file/d/1i2oe929loEzhvCfRYG-DeZZQ_YouaBYD/view

osu!game_space: 146.879 MB -> 94.658 MB

osu!screen_space: 79.733 MB -> 33.240 MB

osu!taiko: 2.794 MB -> 1.920 MB

osu!mania: 14.574 MB -> 9.227 MB

TOTAL: 243.970616 MB -> 139.04455 MB


Game space (Raw input enabled replays) is lossless for every single frame where any key is being held, for the frames with no key inputs the x/y coordinates are quantised to a lower resolution - an imperceptible change but it can easily be removed.

Screen space is technically 'gainful' compression, as it fixes the slight rounding issue present in the original OSR

All other formats are entirely lossless.
