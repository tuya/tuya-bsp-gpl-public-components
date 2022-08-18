#ifndef SHA256_H_
#define SHA256_H_

/**
* @brief hash data with sha256
* @param data data to be hash
* @param len data length
* @param hash hash value (need 32 bytes buffer)

* @return success return 0, failed return errno
*/
int sha256(const void *data, int len, unsigned char *hash);

#endif
