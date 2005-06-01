#ifndef FLACENCODER_H_
#define FLACENCODER_H_

#include <qstring.h>
#include "encoder.h"

class AudioMetadata;

#define HAVE_INTTYPES_H
#include <FLAC/file_encoder.h>

#define MAX_SAMPLES 588 * 4
#define NUM_CHANNELS 2

class Metadata;

class FlacEncoder : public Encoder 
{
  public:
    FlacEncoder(const QString &outfile, int qualitylevel, AudioMetadata *metadata);
   ~FlacEncoder();
    int addSamples(int16_t *bytes, unsigned int len);

  private:
    FLAC__FileEncoder *encoder;
    unsigned int sampleindex;
    FLAC__int32 inputin[NUM_CHANNELS][MAX_SAMPLES];
    FLAC__int32 *input[NUM_CHANNELS];
};

#endif
