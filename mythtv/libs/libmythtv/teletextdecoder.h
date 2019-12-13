#ifndef VBIDECODER_H_
#define VBIDECODER_H_

#include <cstdint>

class TeletextReader;

class TeletextDecoder
{
  public:
    explicit TeletextDecoder(TeletextReader *reader)
      : m_teletextReader(reader) {}
    virtual ~TeletextDecoder() = default;

    int  GetDecoderType(void) const { return m_decoderType; }
    void Decode(const unsigned char *buf, int vbimode);

  private:
    TeletextReader *m_teletextReader {nullptr};
    int             m_decoderType    {-1};
};

#endif
