#ifndef FLACENCODER_H_
#define FLACENCODER_H_

#include <qstring.h>
#include "encoder.h"

class AudioMetadata;

#define HAVE_INTTYPES_H
#include <FLAC/all.h>

#define MAX_SAMPLES 588 * 4
#define NUM_CHANNELS 2

class Metadata;

class FlacEncoder : public Encoder 
{
  public:
    FlacEncoder(const QString &l_outfile, int qualitylevel, AudioMetadata *l_metadata);
   ~FlacEncoder();

    int     addSamples(int16_t *bytes, unsigned int len);

  private:

    bool    addMetadataTags();
    QString getComment(FLAC__StreamMetadata* pBlock, const char* pLabel);
    void    setComment(FLAC__StreamMetadata* pBlock, const char* pLabel, const QString& rData);

    FLAC__FileEncoder  *encoder;
    unsigned int        sampleindex;
    FLAC__int32         inputin[NUM_CHANNELS][MAX_SAMPLES];
    FLAC__int32        *input[NUM_CHANNELS];
};

#endif
