make
./event_driven_server.out &
echo "launched event driven server in the background"
httperf --server localhost --port 8000 --uri /README.md --num-conns 10000 --rate 800 --num-call 1 --timeout 5