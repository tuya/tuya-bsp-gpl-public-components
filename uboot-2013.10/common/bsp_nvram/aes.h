#ifndef TUYA_BSP_AES_H
#define TUYA_BSP_AES_H

/**
 * @brief encrypt data by aes128
 * @param data data to be encrypt
 * @param len data length
 * @param ec_data encrypted data
 * @param ec_len encrypted data length
 * @param key key(must be 16bytes)
 * @return success return 0, failed return errno
 * @note caller should free *ec_data by itself
 */
extern int aes128_data_encode(const unsigned char *data, const unsigned int len,\
                               unsigned char **ec_data, unsigned int *ec_len,\
                               const unsigned char *key);

/**
* @brief decrypt data by aes128
* @param data data to be decrypt
* @param len data length
* @param dec_data decrypted data
* @param dec_len decrypted data length
* @param key key(must be 16bytes)
* @return success return 0, failed return errno
* @note caller should free *dec_data by itself
*/
extern int aes128_data_decode(const unsigned char *data, const unsigned int len,\
                              unsigned char **dec_data, unsigned int *dec_len,\
                              const unsigned char *key);

#endif
