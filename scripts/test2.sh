#!/bin/bash

echo "Test2:"; 
echo -e "Number of thread workers = 4;\nMax Memory available in bytes = 1048576;\nMax number of file server can handle = 10;\nSocket file name = storage_sock.sk;\nLog file name = log_file;" > config.txt

./server &
pid=$!
sleep 2s

./client -p -f storage_sock.sk -t 200 -W ./storage2/append.txt -W ./storage2/prova.txt
./client -p -f storage_sock.sk -t 200 -r ./storage2/append.txt
./client -p -f storage_sock.sk -t 200 -a ./storage2/append.txt,pino
./client -p -f storage_sock.sk -t 200 -w ./storage1 -D ./expelled

sleep 1s

kill -s SIGHUP $pid
wait $pid