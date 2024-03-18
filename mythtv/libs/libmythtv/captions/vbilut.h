#ifndef VBILUT_H
#define VBILUT_H

#include <array>
#include <cstdint>
#include <string>

extern const std::array<const std::array<const uint8_t,16>,1+8+8> lang_chars;
extern const std::array<const uint8_t,     13> chartab_original;
extern const std::array<const uint16_t,   256> hammtab;
extern const std::array<const uint8_t,    256> hamm84tab;
extern const std::array<const uint8_t,    256> unham84tab;
extern const std::array<const uint8_t,    256> vbi_bit_reverse;
extern const std::array<const std::string,  8> formats;
extern const std::array<const std::string,  4> subtitles;
extern const std::array<const std::array<const uint8_t,256>,3> hamm24par;
extern const std::array<const uint8_t,    256> hamm24val;
extern const std::array<const uint16_t,    64> hamm24err;
extern const std::array<const int32_t,     64> hamm24cor;

enum vbimode : std::uint8_t
{
    VBI_IVTV,        /// < IVTV packet
    VBI_DVB,         /// < DVB packet
    VBI_DVB_SUBTITLE /// < DVB subtitle packet
};

int hamm8(const uint8_t *p, int *err);
int hamm84(const uint8_t *p, int *err);
int hamm16(const uint8_t *p, int *err);
int hamm24(const uint8_t *p, int *err);

#endif // VBILUT_H
