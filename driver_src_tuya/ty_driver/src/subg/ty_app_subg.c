#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEV_NAME "/dev/tysubg"

int main(int argc, char *argv[])
{
	int fd = 0;
	char read_buf[4];

	fd = open(DEV_NAME,O_RDWR);
	if(fd < 0)
	{
		printf("open /dev/tysubg error\n");
		return -1;
	}

	while(1) {
		printf("hello world.\n");
		read(fd, read_buf, sizeof(read_buf)/sizeof(char));
		printf("buf=0x%x, 0x%x, 0x%x, 0x%x\n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
		sleep(1);
	}
	
	close(fd);

	return 0;

}
