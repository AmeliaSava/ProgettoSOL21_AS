#!/bin/bash

n_read=0
tot_br=0
n_write=0
tot_bw=0
n_lock=0
n_olock=0
n_close=0
max_mem=0
max_file=0
tot_exp=0
n_threds=0
max_con=0

if [ -e "log_file.txt" ]; then

    n_read=$(grep "ReadFile" "log_file.txt" | wc -l)
    n_write=$(grep "WriteFile" "log_file.txt" | wc -l)
    n_lock=$(grep "LockFile" "log_file.txt" | wc -l)
    n_olock=$(grep "OpenLock" "log_file.txt" | wc -l)    
    n_close=$(grep "CloseFile" "log_file.txt" | wc -l)
    max_con=$(grep "Connected" "log_file.txt" | wc -l)

    max_mem=$(grep -e "MAXMEM" "log_file.txt" | cut -c 8- )
    max_file=$(grep -e "MAXFILES" "log_file.txt" | cut -c 10- )
    tot_exp=$(grep -e "EXP" "log_file.txt" | cut -c 5- )

    n_threads=$(grep -e "StartedThread" "log_file.txt" | wc -l)
    
    for i in $(grep -e "Bytes" "log_file.txt" | cut -c 7- ); do   
        tot_bw=$tot_bw+$i;
    done

    tot_bw=$(bc <<< ${tot_bw})

    for i in $(grep -e "Size" "log_file.txt" | cut -c 6- ); do   
    tot_br=$tot_br+$i;
    done

    tot_br=$(bc <<< ${tot_br})

    for i in $(grep -e "Bytes" "log_file.txt" | cut -c 7- ); do   
        tot_bw=$tot_bw+$i;
    done

    for i in $( eval echo {0..${n_threads}} ); do   
    tot_br=$tot_br+$i;
    done

    tot_br=$(bc <<< ${tot_br})

    echo "Number of reads:"
    echo "${n_read}"

    echo "Mean bytes read:"
    if [ ${n_read} != 0 ]; then
        echo "scale=0; ${tot_br} / ${n_read}" | bc -l
    fi

    echo "Number of writes:"
    echo "${n_write}"
    
    echo "Mean bytes wrote:" 
    if [ ${n_write} != 0 ]; then
        echo "scale=0; ${tot_bw} / ${n_write}" | bc -l
    fi

    echo "Number of locks:"
    echo "${n_lock}"
    echo "Number of open lock:"
    echo "${n_olock}"
    echo "Number of close:"
    echo "${n_close}"
    echo "Max memory reached:"
    echo "${max_mem}"
    echo "Max files saved:"
    echo "${max_file}"
    echo "Times a file was expelled due to a capacity miss:"
    echo "${tot_exp}"
    echo "Max connections:"
    echo "${max_con}"
           
else
    echo "No log file found"
fi