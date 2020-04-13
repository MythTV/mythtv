#ifndef FREESAT_HUFFMAN_H
#define FREESAT_HUFFMAN_H

// POSIX header
#include <unistd.h>

// Qt header
#include <QString>

QString freesat_huffman_to_string(const unsigned char *compressed, uint size);

#endif // FREESAT_HUFFMAN_H
