This example showcase RTE Flow FLAG action.
Packets with even DST IP address are flagged.
And others are left without change.

If packet is received its printed if it has even or odd DST IP based on its FLAG status.
After terminating the app simple stats are shown.
```
sudo ./flag -a 0000:05:00.0 --file-prefix=flag
```
