#ifndef _DARWIN_SENDFILE_H
#define _DARWIN_SENDFILE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef off_t __off64_t;

ssize_t sendfile64(int out_fd, int in_fd, __off64_t *offset, size_t size);

#ifdef __cplusplus
}
#endif

#endif

