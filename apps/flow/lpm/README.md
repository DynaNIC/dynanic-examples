This example showcase LPM feature of the group 1.


Packets which belongs to the subnet 192.168.1.0/24 are forwarded to the queue 1
192.168.1.0/16 to queue 2
and the rest to the 0.

LPM is considered therefore 192.168.1.1 will go to the 1
192.168.2.1 to the 2
192.1.1.1 to the 0

```
sudo ./lpm -a 0000:05:00.0 --file-prefix=lpm
```
