#ifndef VBIDECODER_H_
#define VBIDECODER_H_

#include <cstdint>

class TeletextReader;

class TeletextDecoder
{
  public:
    explicit TeletextDecoder(TeletextReader *reader)
      : m_teletext_reader(reader) {}
    virtual ~TeletextDecoder() = default;

    int  GetDecoderType(void) const { return m_decodertype; }
    void Decode(const unsigned char *buf, int vbimode);

  private:
    TeletextReader *m_teletext_reader {nullptr};
    int             m_decodertype     {-1};
};

#endif
