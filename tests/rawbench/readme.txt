有三个文件：
1、rawbench.c 测试主文件；
编译方法：cc -o rawbench rawbench.c -laio -lpthread -Wall 
使用方法：./rawbench tpcc.spc /dev/sdg1
2、stop_test.c 随时用它可以正常停止测试；
使用方法：./stop_test
3、tpcc.spc是我伪造的一个小的trace文件，供测试的。