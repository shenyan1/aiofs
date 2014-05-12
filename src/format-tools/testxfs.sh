for((i=0;i<700;i++))do
echo $i
 if [ ! -f "/shenyan/f$i" ]; then 
    dd if=/dev/sda of=/shenyan/f$i bs=4M count=50 iflag=direct
 fi
done
