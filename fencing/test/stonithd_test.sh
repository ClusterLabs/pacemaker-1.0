killall -9 lrmd
sleep 1
killall -9 stonithd
sleep 1
/usr/lib/heartbeat/stonithd -d -t
sleep 2
/usr/lib/heartbeat/lrmd  &
sleep 2
/usr/lib/heartbeat/lrmadmin -A myid stonith null NULL config_string=hadev1
/usr/lib/heartbeat/lrmadmin -E myid start 1000 0 0
./apitest
