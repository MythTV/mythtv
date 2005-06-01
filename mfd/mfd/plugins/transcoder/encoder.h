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
    Encoder(const QString &outfile, int qualitylevel, AudioMetadata *metadata);
    virtual ~Encoder();
    virtual int addSamples(int16_t * bytes, unsigned int len) = 0;

    virtual bool isValid() { return (out != NULL); }

  protected:
    const QString *outfile;
    FILE *out;
    int quality;
    AudioMetadata *metadata;
};

#endif
