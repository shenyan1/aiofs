#include<stdio.h>
main(){
char f[sizeof("hello")];

strcpy(f,"hello");
printf("%d %s\n",sizeof("hello"),f);
}
