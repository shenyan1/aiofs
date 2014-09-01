for((i=0;i<1000;i++))do
echo $i
# if [ ! -f "/mnt/sdb1/t$i" ]; then 
    dd if=/dev/sda of=/mnt/sdb1/t$i bs=4M count=50
# fi
done
