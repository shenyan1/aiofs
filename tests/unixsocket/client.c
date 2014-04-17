#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_SEND 1025
#define UNIX_PATH "/tmp/sinfor"

void dump_unix(int sock_fd)
{
	char tmp[MAX_SEND] = {0};
	char recv[MAX_SEND] = {0};
	while(fgets(tmp, MAX_SEND, stdin) != NULL)
	{
		write(sock_fd, tmp, strlen(tmp));
		read(sock_fd, recv, MAX_SEND);
		printf("data : %s\n", recv);
		bzero(tmp,MAX_SEND);
		bzero(recv, MAX_SEND);
	}
}
int main(int argc, char** argv)
{
	int conn_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if(conn_sock == -1)
	{
		perror("socket fail ");
		return -1;
	}
	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strcpy((void*)&addr.sun_path, UNIX_PATH);
	if(connect(conn_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("connect fail ");
		return -1;
	}
	dump_unix(conn_sock);
	close(conn_sock);
	return 0;
}
