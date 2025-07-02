#ifndef FLACENCODER_H_
#define FLACENCODER_H_

#include <cstdint>

#include <FLAC/stream_encoder.h>

#include "encoder.h"
#include "libmythbase/sizetliteral.h"

static constexpr size_t MAX_SAMPLES  { 588_UZ * 4 };
static constexpr int8_t NUM_CHANNELS { 2          };

class MusicMetadata;
class QString;

class FlacEncoder : public Encoder
{
  public:
    FlacEncoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata);
   ~FlacEncoder() override;
    int addSamples(int16_t *bytes, unsigned int len) override; // Encoder
    int processSamples();

  private:
    FLAC__StreamEncoder *m_encoder {nullptr};
    unsigned int  m_sampleIndex {0};

    std::array<std::array<FLAC__int32,MAX_SAMPLES>,NUM_CHANNELS> m_inputIn {};
    std::array<FLAC__int32 *,NUM_CHANNELS>m_input {};
};

#endif
