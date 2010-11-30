#ifndef VBIDECODER_H_
#define VBIDECODER_H_

#include <stdint.h>

class TeletextReader;

class TeletextDecoder
{
  public:
    TeletextDecoder(TeletextReader *reader)
      : m_teletext_reader(reader), m_decodertype(-1) {}
    virtual ~TeletextDecoder() {}

    int  GetDecoderType(void) const { return m_decodertype; }
    void Decode(const unsigned char *buf, int vbimode);

  private:
    TeletextReader *m_teletext_reader;
    int             m_decodertype;
};

#endif
