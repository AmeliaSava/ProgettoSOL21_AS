#!/bin/bash

echo "Test1:"; 
echo -e "Number of thread workers = 1;\nMax Memory available in bytes = 128000000;\nMax number of file server can handle = 10000;\nSocket file name = storage_sock.sk;\nLog file name = log_file;" > config.txt

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./server &
pid=$!
sleep 2s

./client -p -f storage_sock.sk -t 200 -W ./storage1/Images/1kb.png -w ./storage2
./client -p -f storage_sock.sk -t 200 -l ./storage1/Images/1kb.png -c ./storage1/Images/1kb.png -l ./storage2/append.txt -u ./storage2/append.txt
./client -p -f storage_sock.sk -t 200 -w ./storage1
./client -p -f storage_sock.sk -t 200 -r ./storage2/prova.txt -R -d ./savedfiles  
./client -h

sleep 1s

kill -s SIGHUP $pid
wait $pid