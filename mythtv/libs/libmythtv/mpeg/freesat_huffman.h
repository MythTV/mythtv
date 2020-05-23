#ifndef FREESAT_HUFFMAN_H
#define FREESAT_HUFFMAN_H

// POSIX header
#include <unistd.h>

// C++ headers
#include <vector>

// Qt header
#include <QString>

struct fsattab {
    uint32_t m_value;
    uint16_t m_bits;
    uint8_t  m_next;
};

extern const std::vector<fsattab> fsat_table_1;
extern const std::vector<fsattab> fsat_table_2;
extern const std::vector<uint16_t> fsat_index_1;
extern const std::vector<uint16_t> fsat_index_2;

QString freesat_huffman_to_string(const unsigned char *compressed, uint size);

#endif // FREESAT_HUFFMAN_H
