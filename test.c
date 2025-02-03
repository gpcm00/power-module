#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<errno.h>

#define printsizeof(d)		printf("sizeof("#d"):\t%ld\n", sizeof(d));

long aligned_to_next_page(long n, int p) {

	return n + (p - (n % p));
}

int decodemode(char* mode)
{
	if(!strcmp(mode, "rd")) {
		return 1;
	}
	
	if(!strcmp(mode, "wr")) {
		return 2;
	}
	
	return -1;
}

void readmem(volatile void* addr, int n) {
	volatile void* readaddr;
	for(int i = 0; i < n; i+=sizeof(int)) {
		readaddr = addr + i;
		printf("0x%lX: ", (long)readaddr);
		int val = *(int*)readaddr;
		printf("0x%X (%d)\n",  val, val);
	}
}

void writemem(volatile void* addr, int n) {
	*(volatile int*)addr = n;
}

int main(int argc, char** argv)
{
	if(argc != 4) {
		printf("usage: %s <rd | wr> <addr> <n>\n"
				"n reads n bytes in read mode\n"
				"n wites the value n in write mode\n", argv[0]);
		return 1; 
	} 
	
	int mode = decodemode(argv[1]);
	if(mode == -1) {
		printf("mode must be either rd or wr\n");
		return 1;
	}
	

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		perror("open");
		return 1;
	}
	
	printsizeof(char);
	printsizeof(short);
	printsizeof(int);
	printsizeof(long);
	
	int pagesize = getpagesize();
	long addr = strtol(argv[2], NULL, 16);
	int n = strtol(argv[3], NULL, 16);
	long addr_offset = addr % pagesize;
	long addr_aligned = addr - addr_offset;
	
	printf("aligned address 0x%lX\n", addr_aligned);
	

	long total_offset = (mode == 1) ? aligned_to_next_page(addr_offset + n, pagesize) : pagesize;
	
	printf("requesting %ld bytes\n", total_offset);
	
	volatile void* base = mmap(NULL, total_offset, PROT_READ | PROT_WRITE, MAP_SHARED, 
								fd, addr_aligned);
	if(base == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	
	if(mode == 1) {
		printf("requesting to read %d bytes from addr 0x%lX\n", n, addr);
		readmem(base + addr_offset, n);
	} else {
		writemem(base + addr_offset, n);
	}
	
	return 0;
	
}
