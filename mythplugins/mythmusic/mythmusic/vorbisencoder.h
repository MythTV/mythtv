#ifndef VORBISENCODER_H_
#define VORBISENCODER_H_

class MusicMetadata;
class Encoder;
class QString;

#include <vorbis/vorbisenc.h>

class VorbisEncoder : public Encoder
{
  public:
    VorbisEncoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata);
   ~VorbisEncoder() override;
    int addSamples(int16_t *bytes, unsigned int len) override; // Encoder

  private:
    ogg_page         m_og            {};
    ogg_packet       m_op            {};
    long             m_packetsDone   {0};
    long             m_bytesWritten  {0L};
    vorbis_comment   m_vc            {};
    ogg_stream_state m_os            {};

    vorbis_dsp_state m_vd            {};
    vorbis_block     m_vb            {};
    vorbis_info      m_vi            {};
};

#endif
