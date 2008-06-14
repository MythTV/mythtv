#ifndef _FREESAT_HUFFMAN_H_
#define _FREESAT_HUFFMAN_H_

// POSIX header
#include <unistd.h>

// Qt header
#include <qstring.h>

QString freesat_huffman_to_string(const unsigned char *compressed, uint size);

#endif // _FREESAT_HUFFMAN_H_
