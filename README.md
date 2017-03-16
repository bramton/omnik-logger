# omnik-logger

socat - UNIX-LISTEN:/var/run/collectd-socket
socat - UNIX-CONNECT:/var/run/collectd-socket       
PUTVAL "hostname/solarpv/pv_power" N:100

nc -q 0 localhost 55 < packet.bin

select collectd
SELECT *  FROM "solarpv_value" ORDER BY time DESC LIMIT 10
