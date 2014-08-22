#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>

#include <openssl/aes.h>
#include <openssl/md5.h>

#include "cs378x.h"

static unsigned long crc_table[256] = { 0x00000000L, 0x77073096L, 0xee0e612cL,
		0x990951baL, 0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
		0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL,
		0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L, 0x1db71064L, 0x6ab020f2L,
		0xf3b97148L, 0x84be41deL, 0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L,
		0x83d385c7L, 0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
		0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L,
		0x4c69105eL, 0xd56041e4L, 0xa2677172L, 0x3c03e4d1L, 0x4b04d447L,
		0xd20d85fdL, 0xa50ab56bL, 0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L,
		0xacbcf940L, 0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
		0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L,
		0x56b3c423L, 0xcfba9599L, 0xb8bda50fL, 0x2802b89eL, 0x5f058808L,
		0xc60cd9b2L, 0xb10be924L, 0x2f6f7c87L, 0x58684c11L, 0xc1611dabL,
		0xb6662d3dL, 0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
		0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L,
		0x0f00f934L, 0x9609a88eL, 0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL,
		0x91646c97L, 0xe6635c01L, 0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L,
		0xf262004eL, 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
		0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL,
		0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L, 0x4db26158L, 0x3ab551ceL,
		0xa3bc0074L, 0xd4bb30e2L, 0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL,
		0xd3d6f4fbL, 0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
		0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL,
		0x270241aaL, 0xbe0b1010L, 0xc90c2086L, 0x5768b525L, 0x206f85b3L,
		0xb966d409L, 0xce61e49fL, 0x5edef90eL, 0x29d9c998L, 0xb0d09822L,
		0xc7d7a8b4L, 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
		0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL, 0xead54739L,
		0x9dd277afL, 0x04db2615L, 0x73dc1683L, 0xe3630b12L, 0x94643b84L,
		0x0d6d6a3eL, 0x7a6a5aa8L, 0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L,
		0x7d079eb1L, 0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
		0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L, 0xfed41b76L,
		0x89d32be0L, 0x10da7a5aL, 0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L,
		0x17b7be43L, 0x60b08ed5L, 0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L,
		0x4fdff252L, 0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
		0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L, 0xdf60efc3L,
		0xa867df55L, 0x316e8eefL, 0x4669be79L, 0xcb61b38cL, 0xbc66831aL,
		0x256fd2a0L, 0x5268e236L, 0xcc0c7795L, 0xbb0b4703L, 0x220216b9L,
		0x5505262fL, 0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
		0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L,
		0xec63f226L, 0x756aa39cL, 0x026d930aL, 0x9c0906a9L, 0xeb0e363fL,
		0x72076785L, 0x05005713L, 0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL,
		0x0cb61b38L, 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
		0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL,
		0xf6b9265bL, 0x6fb077e1L, 0x18b74777L, 0x88085ae6L, 0xff0f6a70L,
		0x66063bcaL, 0x11010b5cL, 0x8f659effL, 0xf862ae69L, 0x616bffd3L,
		0x166ccf45L, 0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
		0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL,
		0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L,
		0x47b2cf7fL, 0x30b5ffe9L, 0xbdbdf21cL, 0xcabac28aL, 0x53b39330L,
		0x24b4a3a6L, 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
		0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L,
		0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL };

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf) DO1(buf); DO1(buf);
#define DO4(buf) DO2(buf); DO2(buf);
#define DO8(buf) DO4(buf); DO4(buf);

static unsigned long crc32(unsigned long crc, const uint8_t *buf, unsigned int len) {
	if (!buf)
		return 0L;
	crc = crc ^ 0xffffffffL;
	while (len >= 8) {
		DO8(buf);
		len -= 8;
	}
	if (len) {
		do {
			DO1(buf);
		} while (--len);
	}
	return crc ^ 0xffffffffL;
}

static int32_t boundary(int32_t exp, int32_t n) {
	return ((((n - 1) >> exp) + 1) << exp);
}

static uint8_t *init_4b(uint32_t val, uint8_t *b) {
	b[0] = (val >> 24) & 0xff;
	b[1] = (val >> 16) & 0xff;
	b[2] = (val >> 8) & 0xff;
	b[3] = (val) & 0xff;
	return b;
}
static uint8_t *init_2b(uint32_t val, uint8_t *b) {
	b[0] = (val >> 8) & 0xff;
	b[1] = (val) & 0xff;
	return b;
}
#define FDGETLINE_TIMEOUT 500
#define FDGETLINE_RETRIES 30

#define FDREAD_TIMEOUT 1500
#define FDREAD_RETRIES 7

#define FDWRITE_TIMEOUT 1500
#define FDWRITE_RETRIES 7

static ssize_t safe_write(int fd, const void *buf, size_t len) {
	ssize_t written;
	do {
		written = write(fd, buf, len);
		if ((written < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return written;
	} while (1);
	return -1;
}

static ssize_t fdwrite(int fd, char *buf, size_t buf_size) {
	ssize_t wbytes = 0;
	int intrs = 0;
	int num_timeouts = 0;
	struct pollfd fdset[1];
	fdset[0].fd = fd;
	fdset[0].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;

	if (buf_size <= 0 || fd == -1)
		return 0;
	while (1) {
		int fdready = poll(fdset, 1, FDWRITE_TIMEOUT);
		if (fdready == -1 || fdready == 0) { /* Timeout || error */
			if (errno == EINTR) {
				if (++intrs < FDWRITE_RETRIES)
					continue;
				else
					return wbytes;
			}
			if (num_timeouts++ <= FDWRITE_RETRIES) {
				continue;
			} else {
				return wbytes > 0 ? wbytes : -1;
			}
		}
		if (fdready < 1 || (fdready == 1 && fdset[0].revents != POLLOUT)) /* Timeout || error */
			return -1;
		ssize_t j = safe_write(fd, buf + wbytes, buf_size - wbytes);
		if (j < 0) // Error writing
			return j;
		if (j == 0) { // No data, retry?
			if (num_timeouts++ > FDREAD_RETRIES) {
				return wbytes;
			}
			continue;
		}
		wbytes += j;
		if (wbytes == (ssize_t) buf_size)
			return wbytes;
	}
	return 0;
}

static ssize_t safe_read(int fd, void *buf, size_t len) {
	ssize_t readen;
	do {
		readen = read(fd, buf, len);
		if ((readen < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return readen;
	} while (1);
	return -1;
}

static ssize_t fdread_ex(int fd, char *buf, size_t buf_size, int timeout, int retries,
		int waitfull) {
	ssize_t rbytes = 0;
	int intrs = 0;
	int num_timeouts = 0;
	struct pollfd fdset[1];
	fdset[0].fd = fd;
	fdset[0].events = POLLIN;
	if (buf_size <= 0 || fd == -1)
		return 0;
	while (1) {
		int fdready = poll(fdset, 1, timeout);
		if (fdready == -1 || fdready == 0) { /* Timeout || error */
			if (errno == EINTR) {
				if (++intrs < retries)
					continue;
				else
					return rbytes;
			}
			if (num_timeouts++ <= retries) {
				continue;
			} else {
				if (timeout) {
				 }
				return rbytes > 0 ? rbytes : -1;
			}
		}
		ssize_t j = safe_read(fd, buf + rbytes, buf_size - rbytes);
		if (j < 0) // Error reading
			return j;
		if (j == 0) { // No data, retry?
			if (num_timeouts++ > retries) {
				return rbytes;
			}
			continue;
		}
		rbytes += j;
		if (!waitfull)
			return rbytes;
		if (rbytes == (ssize_t) buf_size)
			return rbytes;
	}
	return 0;
}

static ssize_t fdread(int fd, char *buf, size_t buf_size) {
	return fdread_ex(fd, buf, buf_size, FDREAD_TIMEOUT, FDREAD_RETRIES, 1);
}

static void set_sock_nonblock(int sockfd) {
	int arg = fcntl(sockfd, F_GETFL, NULL);
	arg |= O_NONBLOCK;
	fcntl(sockfd, F_SETFL, arg);
}

static void set_sock_block(int sockfd) {
	int arg = fcntl(sockfd, F_GETFL, NULL);
	arg &= (~O_NONBLOCK);
	fcntl(sockfd, F_SETFL, arg);
}

static char *my_inet_ntop(int family, struct sockaddr *addr, char *dest, int dest_len) {
	struct sockaddr_in *addr_v4 = (struct sockaddr_in *) addr;
	struct sockaddr_in6 *addr_v6 = (struct sockaddr_in6 *) addr;
	switch (family) {
	case AF_INET:
		return (char *) inet_ntop(AF_INET, &addr_v4->sin_addr, dest, dest_len);
		break;
	case AF_INET6:
		return (char *) inet_ntop(AF_INET6, &addr_v6->sin6_addr, dest, dest_len);
		break;
	default:
		memset(dest, 0, dest_len);
		strcpy(dest, "unknown");
		return dest;
	}
}
static int do_connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen,
		int timeout) {
	set_sock_nonblock(sockfd);
	// Trying to connect with timeout
	int result = connect(sockfd, serv_addr, addrlen);
	if (result < 0) { // Not connected
		if (errno != EINPROGRESS) { // Not in progress
			return -1;
		} else { // Wait a timeout ms for socket to become ready to write into
			struct pollfd fdset[1];
			fdset[0].fd = sockfd;
			fdset[0].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
			do {
				int fdready = poll(fdset, 1, timeout);
				if (fdready == -1 && errno == EINTR)
					continue;
				if (fdready < 1
						|| (fdready == 1 && fdset[0].revents != POLLOUT)) { // Timeout || error
					// Get socket error
					unsigned int err_val = 0, sz = sizeof(int);
					getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*) (&err_val),
							&sz);
					errno = err_val;
					if (!errno) // It can't be success, so simulate timeout
						errno = ENETDOWN;
					return -1;
				}
				break;
			} while (1);
		}
	}
	set_sock_block(sockfd);
	return 0;
}


static int cs378x_get_cw(int client_sockfd, AES_KEY aes_decrypt_key,
		uint16_t *ca_id, uint16_t *idx, uint8_t *cw) {
	unsigned char data[256];

	int data_len = 256;
	int i;

	// Read AUTH token
	ssize_t r = fdread(client_sockfd, (char *)data, 4);
	if (r < 4)
		return -1;

	READ:
	for (i = 0; i < data_len; i += 16) { // Read and decrypt payload
		fdread(client_sockfd, (char *) data + i, 16);
		AES_decrypt(data + i, data + i, &aes_decrypt_key);
		if (i == 0)
			data_len = boundary(4, data[1] + 20); // Initialize real data length
	}
	if (data_len < 0) // Fatal error
		return -1;

	// EMM request, ignore it. Sometimes OSCAM sends two EMM requests after CW
	if (data[0] == 0x05)
		goto READ;

	if (data[0] != 0x01) {
		return 0;
	}

	if (data_len < 48) {
		return 0;
	}

	if (data[1] < 16) {
		return 0;
	}

	*ca_id = (data[10] << 8) | data[11];
	*idx = (data[16] << 8) | data[17];
	memcpy(cw, data + 20, 32);

	return 1;
}

int cs378x_getcws(int client_sockfd, unsigned char * pkt, unsigned char * cw) {
	int i, j;
	uint16_t ca_id = 0, idx = 0;
	unsigned char msgbuf[116];
	char tbuf[256];
	unsigned char dump[16];
	char * user = "mythtv"; // TODO move to config file
	char * pass = "mythtv"; // TODO move to config file
	unsigned int auth_token;

	j = 0;
	for (i = 0; i < 32; i++) {
		j += sprintf(tbuf + j, "%02X", pkt[i]);
	}

	MD5((unsigned char*) pass, strlen(pass), dump);
	AES_KEY aes_encrypt_key;
	AES_KEY aes_decrypt_key;
	AES_set_encrypt_key(dump, 128, &aes_encrypt_key);
	AES_set_decrypt_key(dump, 128, &aes_decrypt_key);
	auth_token = crc32(0L, MD5((unsigned char *) user, strlen(user), dump), 16);
	memset(msgbuf, 0, 116);

	init_4b(auth_token, msgbuf);

	msgbuf[5] = 92; // Data length
	init_4b(crc32(0L, pkt, 92), msgbuf + 8); // Data CRC

	//service ID
	init_2b(0x01, msgbuf + 12);

	// CAID
	init_2b(0x5601, msgbuf + 14);

	//msgid
	init_2b(12, msgbuf + 20);

	// fill?
	msgbuf[22] = 0xff;
	msgbuf[23] = 0xff;

	memcpy(msgbuf + 24, pkt, 92);

	j = 0;
	for (i = 0; i < 32; i++) {
		j += sprintf(tbuf + j, "%02X", msgbuf[i]);
	}

	for (i = 4; i < 116; i += 16) // Encrypt payload
		AES_encrypt(msgbuf + i, msgbuf + i, &aes_encrypt_key);

	fdwrite(client_sockfd, (char*) msgbuf, 116);

	return cs378x_get_cw(client_sockfd, aes_decrypt_key, &ca_id, &idx, cw);
}

int cs378x_connect_client(int socktype, const char *hostname, const char *service) { //, const char * user, const char *pass
	struct addrinfo hints, *res;
	int n;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = socktype;


	n = getaddrinfo(hostname, service, &hints, &res);

	if (n < 0) {
		return -1;
	}

	int sockfd = -1;
	struct addrinfo *ressave = res;
	char str_addr[INET6_ADDRSTRLEN] = { 0 };
	while (res) {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd > -1) {
			my_inet_ntop(res->ai_family, res->ai_addr, str_addr,
					sizeof(str_addr));
			if (do_connect(sockfd, res->ai_addr, res->ai_addrlen, 1000) < 0) {
				close(sockfd);
				sockfd = -1;
			} else {
				break; // connected
			}
		} else {
			sleep(1);
			return -1;
		}
		res = res->ai_next;
	}
	freeaddrinfo(ressave);

	if (socktype == SOCK_STREAM) {
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	}
	return sockfd;
}
