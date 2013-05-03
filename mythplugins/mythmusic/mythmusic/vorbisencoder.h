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
   ~VorbisEncoder();
    int addSamples(int16_t *bytes, unsigned int len);

  private:
    ogg_page og;
    ogg_packet op;
    long packetsdone;
    int eos;
    long bytes_written;
    vorbis_comment vc;
    ogg_stream_state os;

    vorbis_dsp_state vd;
    vorbis_block vb;
    vorbis_info vi;

    MusicMetadata *m_metadata;
};

#endif
