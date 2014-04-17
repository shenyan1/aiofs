for((i=0;i<700;i++))do
echo $i
dd if=/dev/sda of=/shenyan/f$i bs=4M count=50 iflag=direct
done
