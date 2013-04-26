#ifndef FLACENCODER_H_
#define FLACENCODER_H_

#include <stdint.h>

#include <FLAC/export.h>
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
  /* FLAC 1.0.4 to 1.1.2 */
  #include <FLAC/file_encoder.h>
  #define encoder_new() FLAC__file_encoder_new()
  #define encoder_setup(enc, streamable_subset, do_mid_side_stereo, \
            loose_mid_side_stereo, channels, bits_per_sample, \
            sample_rate, blocksize, max_lpc_order, \
            qlp_coeff_precision, do_qlp_coeff_prec_search, \
            do_escape_coding, do_exhaustive_model_search, \
            min_residual_partition_order, max_residual_partition_order, \
            rice_parameter_search_dist) \
            { \
              FLAC__file_encoder_set_streamable_subset(enc, streamable_subset); \
              FLAC__file_encoder_set_do_mid_side_stereo(enc, do_mid_side_stereo); \
              FLAC__file_encoder_set_loose_mid_side_stereo(enc, loose_mid_side_stereo); \
              FLAC__file_encoder_set_channels(enc, channels); \
              FLAC__file_encoder_set_bits_per_sample(enc, bits_per_sample); \
              FLAC__file_encoder_set_sample_rate(enc, sample_rate); \
              FLAC__file_encoder_set_blocksize(enc, blocksize); \
              FLAC__file_encoder_set_max_lpc_order(enc, max_lpc_order); \
              FLAC__file_encoder_set_qlp_coeff_precision(enc, qlp_coeff_precision); \
              FLAC__file_encoder_set_do_qlp_coeff_prec_search(enc, do_qlp_coeff_prec_search); \
              FLAC__file_encoder_set_do_escape_coding(enc, do_escape_coding); \
              FLAC__file_encoder_set_do_exhaustive_model_search(enc, do_exhaustive_model_search); \
              FLAC__file_encoder_set_min_residual_partition_order(enc, min_residual_partition_order); \
              FLAC__file_encoder_set_max_residual_partition_order(enc, max_residual_partition_order); \
              FLAC__file_encoder_set_rice_parameter_search_dist(enc, rice_parameter_search_dist); \
            }
  #define encoder_finish(enc) FLAC__file_encoder_finish(enc)
  #define encoder_delete(enc) FLAC__file_encoder_delete(enc)
  #define encoder_process(enc, data, index) FLAC__file_encoder_process(enc, data, index)
  #define FLAC_ENCODER FLAC__FileEncoder
#else 
  /* FLAC 1.1.3 and up */
  #define NEWFLAC
  #include <FLAC/stream_encoder.h>
  #define encoder_new() FLAC__stream_encoder_new()
  #define encoder_setup(enc, streamable_subset, do_mid_side_stereo, \
            loose_mid_side_stereo, channels, bits_per_sample, \
            sample_rate, blocksize, max_lpc_order, \
            qlp_coeff_precision, do_qlp_coeff_prec_search, \
            do_escape_coding, do_exhaustive_model_search, \
            min_residual_partition_order, max_residual_partition_order, \
            rice_parameter_search_dist) \
            { \
              FLAC__stream_encoder_set_streamable_subset(enc, streamable_subset); \
              FLAC__stream_encoder_set_do_mid_side_stereo(enc, do_mid_side_stereo); \
              FLAC__stream_encoder_set_loose_mid_side_stereo(enc, loose_mid_side_stereo); \
              FLAC__stream_encoder_set_channels(enc, channels); \
              FLAC__stream_encoder_set_bits_per_sample(enc, bits_per_sample); \
              FLAC__stream_encoder_set_sample_rate(enc, sample_rate); \
              FLAC__stream_encoder_set_blocksize(enc, blocksize); \
              FLAC__stream_encoder_set_max_lpc_order(enc, max_lpc_order); \
              FLAC__stream_encoder_set_qlp_coeff_precision(enc, qlp_coeff_precision); \
              FLAC__stream_encoder_set_do_qlp_coeff_prec_search(enc, do_qlp_coeff_prec_search); \
              FLAC__stream_encoder_set_do_escape_coding(enc, do_escape_coding); \
              FLAC__stream_encoder_set_do_exhaustive_model_search(enc, do_exhaustive_model_search); \
              FLAC__stream_encoder_set_min_residual_partition_order(enc, min_residual_partition_order); \
              FLAC__stream_encoder_set_max_residual_partition_order(enc, max_residual_partition_order); \
              FLAC__stream_encoder_set_rice_parameter_search_dist(enc, rice_parameter_search_dist); \
            }
  #define encoder_finish(enc) FLAC__stream_encoder_finish(enc)
  #define encoder_delete(enc) FLAC__stream_encoder_delete(enc)
  #define encoder_process(enc, data, index) FLAC__stream_encoder_process(enc, data, index)
  #define FLAC_ENCODER FLAC__StreamEncoder
#endif

#include "encoder.h"

#define MAX_SAMPLES 588 * 4
#define NUM_CHANNELS 2

class MusicMetadata;
class QString;

class FlacEncoder : public Encoder
{
  public:
    FlacEncoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata);
   ~FlacEncoder();
    int addSamples(int16_t *bytes, unsigned int len);

  private:
    FLAC_ENCODER *encoder;
    unsigned int sampleindex;
    FLAC__int32 inputin[NUM_CHANNELS][MAX_SAMPLES];
    FLAC__int32 *input[NUM_CHANNELS];
};

#endif
