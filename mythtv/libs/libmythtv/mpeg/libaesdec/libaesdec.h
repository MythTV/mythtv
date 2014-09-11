/*
 * libaesdec.h
 *
 *  Created on: Jun 22, 2014
 *      Author: root
 */

#ifdef __cplusplus
extern "C" {
#endif

// -- alloc & free the key structure
void *aes_get_key_struct(void);
void aes_free_key_struct(void *keys);

// -- set aes keys, 16 bytes each
void aes_set_control_words(void *keys, const unsigned char *even, const unsigned char *odd);

// -- decrypt TS packet
void aes_decrypt_packet(void *keys, unsigned char *packet);

#ifdef __cplusplus
}
#endif

