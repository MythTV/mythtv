typedef off_t __off64_t;

ssize_t sendfile64(int out_fd, int in_fd, __off64_t *offset, size_t size);
