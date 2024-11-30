# omnik-logger

socat - UNIX-LISTEN:/var/run/collectd-socket
socat - UNIX-CONNECT:/var/run/collectd-socket       
PUTVAL "hostname/solarpv/pv_power" N:100

nc -q 0 localhost 55 < packet.bin

# Location
Two options to specify to which location the Omnik messages should be send. First, the web interface of the Omnik module has an option to specify an additional location for the messages to be send. Second, we can add a DNS entry for the original url which was used by the Omnik module.

```
local-zone: "solarmandata.com" static
local-data: "www.solarmandata.com. IN A 192.168.0.1"
```

Fun fact, if the Omnik module does not receive a DNS record, it uses a hard coded IP.
