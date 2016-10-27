#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "cpu.h"
#include "cartridge.h"
#include "memory.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[]) {
	if(argc != 2){
		printf("too few argument\n");
		return -1;
	}

	int fd;
	struct stat sbuf;
	fd = open(argv[1], O_RDONLY);
	if(fd == -1)
		handle_error("open");
	if(fstat(fd, &sbuf) == -1)
		handle_error("fstat");

	uint8_t *rom = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(rom == MAP_FAILED)
		handle_error("mmap");

	struct cartridge *cart = cart_init(rom);
	if(cart == NULL)
		return -1;
	struct gb_carthdr *hdr = cart_header(cart);
    printf("title: %.16s\ncgbflag: 0x%X\ncarttype: 0x%X\nromsize: 0x%X\nramsize: 0x%X\n",
		 hdr->title, hdr->cgbflag, hdr->carttype, hdr->romsize, hdr->ramsize);

	if(memory_init(cart)){
		free(cart);
		return -1;
	}


	memory_free();

	return 0;
}
