#!/bin/bash

echo "Test3:"; 
echo -e "Number of thread workers = 8;\nMax Memory available in bytes = 33554432;\nMax number of file server can handle = 100;\nSocket file name = storage_sock.sk;\nLog file name = log_file;" > config.txt

timeout --signal=SIGINT 30s ./server & 
pid=$!
sleep 2s

folder_index=10
file_index=10

while [ -e /proc/$pid/status ]
do
    if [ -e /proc/$pid/status ]; then

        ./client -f storage_sock.sk -W ./stresstest/files$folder_index/file$file_index.txt -r ./stresstest/files$folder_index/file$file_index.txt -l ./stresstest/files$folder_index/file$file_index.txt -u ./stresstest/files$folder_index/file$file_index.txt -R 1
        ./client -f storage_sock.sk -l ./stresstest/files$folder_index/file$file_index.txt -u ./stresstest/files$folder_index/file$file_index.txt -r ./stresstest/files$folder_index/file$file_index.txt -l ./stresstest/files$folder_index/file$file_index.txt -c ./stresstest/files$folder_index/file$file_index.txt
        
        file_index=$(($file_index - 1))
        
        if (($file_index == 0)); then
            file_index=10
            folder_index=$(($folder_index - 1))
            if (($folder_index == 0)); then
                folder_index=10
            fi
        fi
    fi
    
done
