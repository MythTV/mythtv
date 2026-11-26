#ifndef LIBMYTHTV_MPEG_MPEG2PARSER_H
#define LIBMYTHTV_MPEG_MPEG2PARSER_H

#include <cstdint>
#include <utility>
#include <vector>

/**
Extracts closed captions from MPEG-1 and MPEG-2 user_data (start code value 0xB2)
and converts them to CTA-708 format (which is used by ATSC A/53 Part 4).

This is based on FFmpeg's mpeg_decode_a53_cc() from libavcodec/mpeg12dec.c
*/
class Mpeg2ClosedCaptionsExtractor
{
  public:
    enum class Format : uint8_t
    {
        kAuto,
        kA53_Part4,
        kSCTE20,
        kDVD,
        kDish,
    };
    static const char* formatToString(Format format)
    {
        switch (format)
        {
        case Format::kAuto:      return "auto";
        case Format::kA53_Part4: return "A/53 Part 4";
        case Format::kSCTE20:    return "SCTE-20";
        case Format::kDVD:       return "DVD";
        case Format::kDish:      return "Dish Network";
        default:                 return "invalid";
        }
    }
    explicit Mpeg2ClosedCaptionsExtractor(Format format = Format::kAuto)
        : m_format(format) {}

    Format getFormat() const { return m_format; }
    void setFormat(Format format) { m_format = format; }

    /** Copy closed caption data from buf into the internal buffer, converting
    to CTA-708 format.
    @return True iff closed captions were appended to the internal buffer.
    */
    bool append(const uint8_t* buf, int buf_size, bool top_field_first);
    /// Return the extracted closed captions and clear the internal buffer.
    std::vector<uint8_t> extract()
    {
        std::vector<uint8_t> ret {m_captions};
        //std::swap(ret, m_captions);
        m_captions.clear();
        return ret;
    }

  private:
    void setFormatPrivate(Format format);
    Format m_format {Format::kAuto};
    std::vector<uint8_t> m_captions;
};

#endif // LIBMYTHTV_MPEG_MPEG2PARSER_H
