#include "freesat_huffman.h"

static constexpr uint8_t START   { '\0' };
static constexpr uint8_t STOP    { '\0' };
static constexpr uint8_t ESCAPE  { '\1' };

QString freesat_huffman_to_string(const unsigned char *compressed, uint size)
{
    const unsigned char *src = compressed;

    if ((src[1] != 1) && (src[1] != 2))
        return {""};

    const std::vector<fsattab> &fsat_table = (src[1] == 1) ? fsat_table_1 : fsat_table_2;
    const std::vector<uint16_t> &fsat_index = (src[1] == 1) ? fsat_index_1 : fsat_index_2;

    QByteArray uncompressed(size * 3, '\0');
    int p = 0;
    unsigned value = 0;
    unsigned byte = 2;
    unsigned bit = 0;
    while (byte < 6 && byte < size)
    {
        value |= src[byte] << ((5-byte) * 8);
        byte++;
    }
    uchar lastch = START;

    do
    {
        bool found = false;
        unsigned bitShift = 0;
        uchar nextCh = STOP;
        if (lastch == ESCAPE)
        {
            found = true;
            // Encoded in the next 8 bits.
            // Terminated by the first ASCII character.
            nextCh = (value >> 24) & 0xff;
            bitShift = 8;
            if ((nextCh & 0x80) == 0)
            {
                if (nextCh < ' ')
                    nextCh = STOP;
                lastch = nextCh;
            }
        }
        else
        {
            auto indx = (unsigned)lastch;
            for (unsigned j = fsat_index[indx]; j < fsat_index[indx+1]; j++)
            {
                unsigned mask = 0;
                unsigned maskbit = 0x80000000;
                for (uint16_t kk = 0; kk < fsat_table[j].m_bits; kk++)
                {
                    mask |= maskbit;
                    maskbit >>= 1;
                }
                if ((value & mask) == fsat_table[j].m_value)
                {
                    nextCh = fsat_table[j].m_next;
                    bitShift = fsat_table[j].m_bits;
                    found = true;
                    lastch = nextCh;
                    break;
                }
            }
        }
        if (found)
        {
            if (nextCh != STOP && nextCh != ESCAPE)
            {
                if (p >= uncompressed.size())
                    uncompressed.resize(p+10);
                uncompressed[p++] = nextCh;
            }
            // Shift up by the number of bits.
            for (unsigned b = 0; b < bitShift; b++)
            {
                value = (value << 1) & 0xfffffffe;
                if (byte < size)
                    value |= (src[byte] >> (7-bit)) & 1;
                if (bit == 7)
                {
                    bit = 0;
                    byte++;
                }
                else
                {
                    bit++;
                }
            }
        }
        else
        {
            // Entry missing in table.
            QString result = QString::fromUtf8(uncompressed, p);
            result.append("...");
            return result;
        }
    } while (lastch != STOP && byte < size+4);

    return QString::fromUtf8(uncompressed, p);
}
