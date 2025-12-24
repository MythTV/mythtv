#ifndef VBILUT_H
#define VBILUT_H

#include <cstdint>

enum vbimode : std::uint8_t
{
    VBI_IVTV,        /// < IVTV packet
    VBI_DVB,         /// < DVB packet
    VBI_DVB_SUBTITLE /// < DVB subtitle packet
};

int hamm8(const uint8_t *p, int *err);
int hamm84(const uint8_t *p, int *err);
int hamm16(const uint8_t *p, int *err);

#endif // VBILUT_H
