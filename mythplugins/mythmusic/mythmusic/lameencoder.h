#ifndef LAMEENCODER_H_
#define LAMEENCODER_H_

#include <qstring.h>

class Metadata;
class Encoder;

#ifdef MMX
#define LAME_WORKAROUND 1
#undef MMX
#endif

#include <lame/lame.h>

#ifdef LAME_WORKAROUND
#define MMX 1
#endif

class LameEncoder : public Encoder 
{
  public:
    LameEncoder(const QString &outfile, int qualitylevel, Metadata *metadata);
   ~LameEncoder();
    int addSamples(int16_t *bytes, unsigned int len);

  private:
    int init_encoder(lame_global_flags *gf, int quality, int vbr);
    void init_id3tags(lame_global_flags *gf, Metadata *metadata);

    int quality;
    int bits;
    int channels;
    int samplerate;
    int bytes_per_sample;
    int samples_per_channel; 

    bool vbr;

    int mp3buf_size;
    char *mp3buf;

    int mp3bytes;

    lame_global_flags *gf;
};

#endif

