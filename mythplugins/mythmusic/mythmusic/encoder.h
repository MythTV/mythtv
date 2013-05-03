#ifndef ENCODER_H_
#define ENCODER_H_

#include <sys/types.h>
#include <stdio.h>

#define EENCODEERROR -1
#define EPARTIALSAMPLE -2
#define ENOTIMPL -3

class MusicMetadata;

class Encoder
{
  public:
    Encoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata);
    virtual ~Encoder();
    virtual int addSamples(int16_t * bytes, unsigned int len) = 0;

    virtual bool isValid() { return (m_out != NULL); }

  protected:
    const QString m_outfile;
    FILE *m_out;
    int m_quality;
    MusicMetadata *m_metadata;
};

#endif
