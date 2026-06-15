This example showcase RTE Flow MARK action.
Packets with DST IP address *.*.*.1 are marked with ID 1.
Packets with DST IP address *.*.*.2 are marked with ID 2.
And others are marked with ID 3.
This example also is also showcasing usage of priotities and default rule.

If packet is received its MARK status is shown.
After terminating the app simple stats are shown.
```
sudo ./mark -a 0000:05:00.0 --file-prefix=mark
```
