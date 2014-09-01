for i in `ipcs -m | grep 1048576 | awk '{print $2}'`
do
ipcrm -m $i
done
