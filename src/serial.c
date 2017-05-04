#include "serial.h"
#include "memory.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <SDL2/SDL.h>

static int sock = -1;
static SDL_Thread *serial_recv_thread;

#define SERIAL_DELAY_CYCLE 20000 
int serial_received = 0;
int serial_sent = 0;
int serial_remaining = 0;
uint8_t serial_recv_buffer = 0;

static int recv_thread(void *ptr) {
	while(1){
		if(sock<0) break;
		if(recv(sock, &serial_recv_buffer, 1, 0)!=1){
			perror("recv");
			close(sock);
			sock = -1;
			serial_recv_buffer = 0xff;
		}
		//printf("r %X\n", serial_recv_buffer);
		//SDL_Delay(1);
		serial_remaining = SERIAL_DELAY_CYCLE;
		serial_received = 1;
	}
	return 0;
}

static int noconn_recv_thread(void *ptr) {
	//SDL_Delay(1);
	serial_remaining = SERIAL_DELAY_CYCLE;
	serial_recv_buffer = 0xff;
	serial_received = 1;
	return 0;
}

void serial_send(uint8_t data) {
	if(sock<0 || send(sock, &data, 1, MSG_NOSIGNAL|MSG_DONTWAIT)!=1){
		if(sock>0){
			perror("send");
			close(sock);
			sock = -1;
		}
		SDL_Thread *noconn_thread = SDL_CreateThread(noconn_recv_thread, "noconn_thread", 
		(void*)NULL);
		if(noconn_thread == NULL)
			printf("\nSDL_CreateThread failed: %s\n", SDL_GetError());
		else
			SDL_DetachThread(noconn_thread);
	}else{
		//printf("w %X\n", data);
	}
}

static void serial_start_recv_thread() {
	serial_recv_thread = SDL_CreateThread(recv_thread, "serial_recv_thread", 
		(void*)NULL);
	if(serial_recv_thread == NULL){
		printf("\nSDL_CreateThread failed: %s\n", SDL_GetError());
	}else{
		SDL_DetachThread(serial_recv_thread);
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
	serial_start_recv_thread();	
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

	serial_start_recv_thread();
	return 0;
}

int serial_linked() {
	if(sock<0) return 0;
	else return 1;
}

void serial_close() {
	close(sock);
	sock = -1;
}

