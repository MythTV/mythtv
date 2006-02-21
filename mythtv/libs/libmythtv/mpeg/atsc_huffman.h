#ifndef _ATSC_HUFFMAN_H_
#define _ATSC_HUFFMAN_H_

// POSIX header
#include <unistd.h>

// Qt header
#include <qstring.h>

QString atsc_huffman1_to_string(const unsigned char *compressed,
                                uint size, uint table);

QString atsc_huffman2_to_string(const unsigned char *compressed,
                                uint length, uint table);


#endif //_ATSC_HUFFMAN_H_
