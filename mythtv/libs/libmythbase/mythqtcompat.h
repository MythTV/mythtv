#ifndef MYTH_QT_COMPAT_H_
#define MYTH_QT_COMPAT_H_

#if (QT_VERSION >= 0x050000)
typedef qintptr qt_socket_fd_t;
#else
typedef int qt_socket_fd_t;
#endif

#endif // MYTH_QT_COMPAT_H_
