#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
//#include <iniparser.h>
#include <arpa/inet.h>

#include "config.h"

//#include "ev.h"

#ifndef MAX_PATH
#	define MAX_PATH 1023
#endif

#define TRUE (1)
#define FALSE (0)

#ifdef __GNUC__
# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)
#else
# define likely(x)       (!!(x))
# define unlikely(x)     (!!(x))
#endif

#undef offsetof
#if defined(__GNUC__) && (__GNUC__ >= 4)
# define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE,MEMBER)
#else
# define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/***************************************************/

#define MPEG_SYNC (0x47)
#define MPEG_PKT_SIZE (188)

#define MPEG_HEADER_LEN (4)

#define MPEG_PSI_CRC_LEN (4)

#define MPEG_HDR_CNT_OFFSET (3)
#define MPEG_HDR_CNT_MASK (0xf)
#define MPEG_HDR_CNT_SHIFT (0)

#define MPEG_HDR8(_ptr_) ((uint8_t*)_ptr_)
#define MPEG_HDR32(_ptr_) ((uint32_t*)_ptr_)

#define MPEG_HDR_TEI(_ptr_) (MPEG_HDR8(_ptr_)[1] & 0x80)
#define MPEG_HDR_PUS(_ptr_) (MPEG_HDR8(_ptr_)[1] & 0x40)
#define MPEG_HDR_TP(_ptr_) (MPEG_HDR8(_ptr_)[1] & 0x20)
#define MPEG_HDR_PID(_ptr_) ( ((((int)MPEG_HDR8(_ptr_)[1]) << 8) | MPEG_HDR8(_ptr_)[2]) & 0x1fff )
#define MPEG_HDR_SC(_ptr_) (MPEG_HDR8(_ptr_)[3] >> 6)
#define MPEG_HDR_AF(_ptr_) (MPEG_HDR8(_ptr_)[3] & 0x20)
#define MPEG_HDR_PD(_ptr_) (MPEG_HDR8(_ptr_)[3] & 0x10)
#define MPEG_HDR_CNT(_ptr_) (MPEG_HDR8(_ptr_)[3] & 0xf)

#define MPEG_HDR_CNT_NEXT(_seq_) (((_seq_) + 1) & 0xf)

#define MPEG_HDR_SC_CLEAR(_ptr_) (MPEG_HDR8(_ptr_)[3] &= 0x3f)

#define MPEG_HDR_AF_LEN(_ptr_) (MPEG_HDR8(_ptr_)[MPEG_HEADER_LEN])

#define MPEG_PID_PAT (0x0000)
#define MPEG_PID_CAT (0x0001)
#define MPEG_PID_SDT (0x0011)

#define MPEG_HDR_SC_NONE (0x0)
#define MPEG_HDR_SC_RESERVED (0x1)
#define MPEG_HDR_SC_EVEN (0x2)
#define MPEG_HDR_SC_ODD (0x3)
#define MPEG_HDR_SC_ENCRYPTED(_x_) ((_x_) & 0x2)

#define MPEG_RESERVED_PID (0x1fff)
#define MPEG_TOTAL_PIDS (0x2000)

/***************************************************/

#define xmalloc malloc
#define xfree free

/***************************************************/

void log_message(int level, const char *module, const char *module_dyn, const char *msg, ...);
void log_data(int level, const char *module, const char *module_dyn, const char *msg, const void *data, int len);

#ifndef LOG_MODULE
# define LOG_MODULE "global"
# define LOG_PARAM (NULL)
#endif

#define log_error(msg...) log_message(LOG_ERR, LOG_MODULE, LOG_PARAM, msg)
#define log_warning(msg...) log_message(LOG_WARNING, LOG_MODULE, LOG_PARAM, msg)
#define log_info(msg...) log_message(LOG_INFO, LOG_MODULE, LOG_PARAM, msg)
#define log_debug(msg...) if (unlikely(debug)) log_message(LOG_DEBUG, LOG_MODULE, LOG_PARAM, msg)

#define log_error_data(msg, data, len) log_data(LOG_ERR, LOG_MODULE, LOG_PARAM, msg": %s", data, len)
#define log_warning_data(msg, data, len) log_data(LOG_WARNING, LOG_MODULE, LOG_PARAM, msg": %s", data, len)
#define log_info_data(msg, data, len) log_data(LOG_INFO, LOG_MODULE, LOG_PARAM, msg": %s", data, len)
#define log_debug_data(msg, data, len) if (unlikely(debug)) log_data(LOG_DEBUG, LOG_MODULE, LOG_PARAM, msg": %s", data, len)

/***************************************************/

typedef uint64_t client_session_id;

/***************************************************/

extern int debug;
extern int verbose;
extern struct ev_loop *evloop;
extern ev_tstamp ev_started_at;
extern char *server_str_id;
extern int server_listen_port;
extern int high_prio_sockets;
extern int recv_socket_bufsize;

#endif /* GLOBAL_H */
