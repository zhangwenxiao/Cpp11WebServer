# logfile=$(date "./log/+%Y-%m-%d-%H:%M:%S.log")
logfile=$(date "+%Y-%m-%d-%H:%M:%S")
./server 8888 | tee "../log/"${logfile}".log"
