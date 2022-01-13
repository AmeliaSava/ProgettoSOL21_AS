#!/bin/bash

#function that centers the text and puts in between two lines
center() 
{
    printf '=%.0s' $(seq 1 $(tput cols))
    echo "$1" | sed  -e :a -e "s/^.\{1,$(tput cols)\}$/ & /;ta" | tr -d '\n' | head -c $(tput cols)
    printf '=%.0s' $(seq 1 $(tput cols)) | sed 's/^ //'
}

# variables to save stats
n_read=0
tot_br=0

n_write=0
tot_bw=0

n_lock=0
n_olock=0
n_unlock=0
n_close=0

max_mem=0
max_file=0
tot_exp=0

n_threds=0
n_requests=0

max_con=0

#if the log file exists
if [ -e "log_file.txt" ]; then

    #counting occurrences of key words
    n_read=$(grep "ReadFile" "log_file.txt" | wc -l)
    n_write=$(grep "WriteFile" "log_file.txt" | wc -l)
    n_lock=$(grep "LockFile" "log_file.txt" | wc -l)
    n_olock=$(grep "OpenLock" "log_file.txt" | wc -l)
    n_unlock=$(grep "UnlockFile" "log_file.txt" | wc -l)    
    n_close=$(grep "CloseFile" "log_file.txt" | wc -l)
    max_con=$(grep "Connected" "log_file.txt" | wc -l)

    #taking values from stats variables
    max_mem=$(grep -e "MAXMEM" "log_file.txt" | cut -c 8- )
    max_file=$(grep -e "MAXFILES" "log_file.txt" | cut -c 10- )
    tot_exp=$(grep -e "EXP" "log_file.txt" | cut -c 5- )

    #counting the number of threads that were activated
    n_threads=$(grep -e "StartedThread" "log_file.txt" | wc -l)

    #sum of all the bytes that were written
    for i in $(grep -e "Bytes" "log_file.txt" | cut -c 7- ); do   
        tot_bw=$tot_bw+$i;
    done
    #obtaining sum
    tot_bw=$(bc <<< ${tot_bw})

    #sum of all the bytes that were written
    for i in $(grep -e "Size" "log_file.txt" | cut -c 6- ); do   
        tot_br=$tot_br+$i;
    done

    #obtaining sum
    tot_br=$(bc <<< ${tot_br})

    #printing values
    center "SERVER STATS"

    echo " "

    enable -n echo

    echo "Number of reads: ${n_read}"

    echo -n "Average bytes read: "
    #calculating the mean value and printing it
    if [ ${n_read} != 0 ]; then
        echo "scale=0; ${tot_br} / ${n_read}" | bc -l
    fi

    echo "Number of writes: ${n_write}"
    
    echo -n "Average bytes wrote:" 
    #calculating the mean value and printing it
    if [ ${n_write} != 0 ]; then
        echo "scale=0; ${tot_bw} / ${n_write}" | bc -l
    fi

    echo "Number of locks: ${n_lock}"
    echo "Number of open lock: ${n_olock}"
    echo "Number of unlock: ${n_unlock}"
    echo "Number of close: ${n_close}"
    echo "Max memory reached: ${max_mem}"
    echo "Max files saved: ${max_file}"
    echo "Times a file was expelled due to a capacity miss: ${tot_exp}"

    for i in $(grep "StartedThread" "log_file.txt" | cut -c 15- ); do  
        n_requests=$(grep "$i" "log_file.txt" | wc -l )
        echo "$n_requests requests were served by thread $i"
    done

    echo "Max connections: ${max_con}"

    echo " "
    center "END"
           
else
    echo "No log file found"
fi