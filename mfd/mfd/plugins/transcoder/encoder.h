#ifndef ENCODER_H_
#define ENCODER_H_

#include <sys/types.h>
#include <stdio.h>

#define EENCODEERROR -1
#define EPARTIALSAMPLE -2
#define ENOTIMPL -3

class AudioMetadata;

class Encoder 
{
  public:
    Encoder(const QString &l_outfile, int qualitylevel, AudioMetadata *l_metadata);
    virtual ~Encoder();
    virtual int addSamples(int16_t * bytes, unsigned int len) = 0;

    virtual bool isValid() { return (out != NULL); }

  protected:
    QString outfile;
    FILE *out;
    int quality;
    AudioMetadata *metadata;
};

#endif
