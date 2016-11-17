#include "serial.h"
#include "memory.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

static int sock = -1;

//データなし/エラー時は負の値を返す
int serial_recv() {
	if(sock<0) return -1;

	int result;
	uint8_t data;
	if((result=read(sock, &data, 1))==1){
		return data;
	}else if(result < 0){
		perror("read");
		close(sock);
		sock = -1;
	}
	return -1;
}

void serial_send() {
	if(sock<0) return;

	if(write(sock, &INTERNAL_IO[IO_SB_R], 1)!=1){
		perror("write");
		close(sock);
		sock = -1;
	}
}

int serial_serverinit(int port) {
	int sock0;
	struct sockaddr_in addr;
	struct sockaddr_in client;
	int yes = 1;

	if((sock0 = socket(AF_INET, SOCK_STREAM, 0))<0){
		perror("socket");
		return -1;
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
	if(bind(sock0, (struct sockaddr *)&addr, sizeof(addr))<0){
		perror("bind");
		close(sock0);
		return -1;
	}
	if(listen(sock0, 0)<0){
		perror("listen");
		close(sock0);
		return -1;
	}

	printf("Waiting for connection...\n");
	socklen_t len = sizeof(client);
	if((sock = accept(sock0, (struct sockaddr *)&client, &len))<0){
		perror("accept");
		close(sock0);
		return -1;
	}

	printf("Connection from %s:%d\n",
			inet_ntoa(client.sin_addr), ntohs(client.sin_port));

	close(sock0);

	return 0;
}

int serial_clientinit(char *host, int port) {
	struct sockaddr_in server;
	if((sock = socket(AF_INET, SOCK_STREAM, 0))<0){
		perror("socket");
		return -1;
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(host);
	if(server.sin_addr.s_addr == 0xffffffff){
		struct hostent *h;
		h=gethostbyname(host);
		if(host==NULL){
			perror("gethostbyname");
			close(sock);
			sock = -1;
			return -1;
		}
		server.sin_addr.s_addr = *(unsigned int *)h->h_addr_list[0];
	}

	if(connect(sock, (struct sockaddr*)&server, sizeof(server))<0){
		perror("connect");
		close(sock);
		sock = -1;
		return -1;
	}

	return 0;
}

void serial_close() {
	close(sock);
	sock = -1;
}

