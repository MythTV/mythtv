/*
 * hdhomerun_os.h
 *
 * Copyright © 2006 Silicondust Engineering Ltd. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(_WIN32) || defined(_WIN64)
#define __WINDOWS__
#endif

#if defined(__WINDOWS__)
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

#else /* !defined(__WINDOWS__) */
#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#endif

#if !defined(TRUE)
#define TRUE 1
#endif

#if !defined(FALSE)
#define FALSE 0
#endif

#if defined(__WINDOWS__)

#define LIBTYPE

#if defined(DLL_IMPORT)
#define LIBTYPE __declspec( dllexport )
#endif

#if defined(DLL_EXPORT)
#define LIBTYPE __declspec( dllimport )
#endif

typedef int bool_t;
typedef signed __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;

#define socklen_t int
#define close closesocket
#define sock_getlasterror WSAGetLastError()
#define sock_getlasterror_socktimeout (WSAGetLastError() == WSAETIMEDOUT)
#define va_copy(x, y) x = y
#define atoll _atoi64
#define strdup _strdup
#define strcasecmp _stricmp
#define snprintf _snprintf
#define fseeko _fseeki64
#define ftello _ftelli64
#define usleep(us) Sleep((us)/1000)
#define sleep(sec) Sleep((sec)*1000)
#define THREAD_FUNC_PREFIX DWORD WINAPI
#define SIGPIPE SIGABRT

static inline uint64_t getcurrenttime(void)
{
	struct timeb tb;
	ftime(&tb);
	return ((uint64_t)tb.time * 1000) + tb.millitm;
}

static inline int setsocktimeout(int s, int level, int optname, uint64_t timeout)
{
	int t = (int)timeout;
	return setsockopt(s, level, optname, (char *)&t, sizeof(t));
}

static inline int pthread_create(pthread_t *tid, void *attr, LPTHREAD_START_ROUTINE start, void *arg)
{
	*tid = CreateThread(NULL, 0, start, arg, 0, NULL);
	if (!*tid) {
		return (int)GetLastError();
	}
	return 0;
}

static inline int pthread_join(pthread_t tid, void **value_ptr)
{
	while (1) {
		DWORD ExitCode = 0;
		if (!GetExitCodeThread(tid, &ExitCode)) {
			return (int)GetLastError();
		}
		if (ExitCode != STILL_ACTIVE) {
			return 0;
		}
	}
}

extern inline void pthread_mutex_init(pthread_mutex_t *mutex, void *attr)
{
	*mutex = CreateMutex(NULL, FALSE, NULL);
}

extern inline void pthread_mutex_lock(pthread_mutex_t *mutex)
{
	WaitForSingleObject(*mutex, INFINITE);
}

extern inline void pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	ReleaseMutex(*mutex);
}

#else /* !defined(__WINDOWS__) */

typedef int bool_t;

#define LIBTYPE
#define sock_getlasterror errno
#define sock_getlasterror_socktimeout (errno == EAGAIN)
#define THREAD_FUNC_PREFIX void *

static inline uint64_t getcurrenttime(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return ((uint64_t)t.tv_sec * 1000) + (t.tv_usec / 1000);
}

static inline int setsocktimeout(int s, int level, int optname, uint64_t timeout)
{
	struct timeval t;
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;
	return setsockopt(s, level, optname, (char *)&t, sizeof(t));
}

#endif
