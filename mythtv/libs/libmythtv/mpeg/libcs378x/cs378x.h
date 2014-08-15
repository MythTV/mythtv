/*
 * cs378x.h
 *
 *  Created on: Jul 29, 2014
 *      Author: root
 */

#ifndef CS378X_H_
#define CS378X_H_
#ifdef __cplusplus
extern "C" {
#endif

int cs378x_connect_client(int socktype, const char *hostname, const char *service);
int cs378x_getcws(int client_sockfd, unsigned char * pkt, unsigned char * cw);
#ifdef __cplusplus
}
#endif
#endif /* CS378X_H_ */
