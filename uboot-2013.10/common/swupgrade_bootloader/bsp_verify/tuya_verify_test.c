#include <stdio.h>
#include "tuya_verify.h"

int main(int argc, char *argv[])
{
	int ret = -1;

	if (argc < 3) {
		printf("%s file_for_verify file_of_sign\n", argv[0]);
		return -1;
	}

	ret = tuya_verify_file(argv[1], argv[2]);
	if(!ret)
		printf("tuya_verify_file success\n");
	else
		printf("tuya_verify_file failed\n");

	return 0;
}
