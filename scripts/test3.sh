#!/bin/bash

echo "Test3:"; 
echo -e "Number of thread workers = 8;\nMax Memory available in bytes = 33554432;\nMax number of file server can handle = 100;\nSocket file name = storage_sock.sk;\nLog file name = log_file;" > config.txt

./server &
pid=$!
sleep 2s

folder_index=10

while :
do

    ./client -p -f storage_sock.sk -W ./stresstest/files$folder_index/file1.png

    folder_index=
    
    if (($folder_index == 0)); then
        folder_index=10
    
done

sleep 1s

kill -s SIGINT $pid
wait $pid