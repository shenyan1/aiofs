#include<sys/shm.h>
char *
getshmptr (int shmid)
{
    char *ptr = 0;
    if (shmid > 0)
	ptr = shmat (shmid, 0, 0);
    return ptr;
}

main ()
{
    char *buf = getshmptr (1637287492);
    printf ("%s\n", buf);
}
