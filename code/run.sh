mkdir ../log
logfile=$(date "+%Y-%m-%d-%H:%M:%S")
./server 8888 4 | tee "../log/"${logfile}".log"
