// C++
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// Qt
#include <QString>

// MythTV
#include <libmythbase/mythlogging.h>
#include <libmythmetadata/metaioflacvorbis.h>
#include <libmythmetadata/musicmetadata.h>

// MythMusic
#include "flacencoder.h"

#include <FLAC/stream_encoder.h>
#include <FLAC/assert.h>

FlacEncoder::FlacEncoder(const QString &outfile, int qualitylevel,
                         MusicMetadata *metadata)
           : Encoder(outfile, qualitylevel, metadata),
             m_encoder(FLAC__stream_encoder_new())
{
    bool streamable_subset = true;
    bool do_mid_side = true;
    bool loose_mid_side = false;
    int bits_per_sample = 16;
    int sample_rate = 44100;
    int blocksize = 4608;
    int max_lpc_order = 8;
    int qlp_coeff_precision = 0;
    bool qlp_coeff_prec_search = false;
    bool do_escape_coding = false;
    bool do_exhaustive_model_search = false;
    int min_residual_partition_order = 3;
    int max_residual_partition_order = 3;
    int rice_parameter_search_dist = 0;

    FLAC__stream_encoder_set_streamable_subset(m_encoder, streamable_subset);
    FLAC__stream_encoder_set_do_mid_side_stereo(m_encoder, do_mid_side);
    FLAC__stream_encoder_set_loose_mid_side_stereo(m_encoder, loose_mid_side);
    FLAC__stream_encoder_set_channels(m_encoder, NUM_CHANNELS);
    FLAC__stream_encoder_set_bits_per_sample(m_encoder, bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(m_encoder, sample_rate);
    FLAC__stream_encoder_set_blocksize(m_encoder, blocksize);
    FLAC__stream_encoder_set_max_lpc_order(m_encoder, max_lpc_order);
    FLAC__stream_encoder_set_qlp_coeff_precision(m_encoder, qlp_coeff_precision);
    FLAC__stream_encoder_set_do_qlp_coeff_prec_search(m_encoder, qlp_coeff_prec_search);
    FLAC__stream_encoder_set_do_escape_coding(m_encoder, do_escape_coding);
    FLAC__stream_encoder_set_do_exhaustive_model_search(m_encoder, do_exhaustive_model_search);
    FLAC__stream_encoder_set_min_residual_partition_order(m_encoder, min_residual_partition_order);
    FLAC__stream_encoder_set_max_residual_partition_order(m_encoder, max_residual_partition_order);
    FLAC__stream_encoder_set_rice_parameter_search_dist(m_encoder, rice_parameter_search_dist);

    QByteArray ofile = outfile.toLocal8Bit();
    int ret = FLAC__stream_encoder_init_file(
        m_encoder, ofile.constData(), nullptr, nullptr);
    if (ret != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error initializing FLAC encoder. Got return code: %1")
                .arg(ret));
    }

    for (auto & chan : m_inputIn)
        chan.fill(0);

    for (int i = 0; i < NUM_CHANNELS; i++)
        m_input[i] = m_inputIn[i].data();
}

FlacEncoder::~FlacEncoder()
{
    FlacEncoder::addSamples(nullptr, 0); // flush buffer

    if (m_encoder)
    {
        FLAC__stream_encoder_finish(m_encoder);
        FLAC__stream_encoder_delete(m_encoder);
    }

    if (m_metadata)
        MetaIOFLACVorbis().write(m_outfile, m_metadata);
}

int FlacEncoder::addSamples(int16_t *bytes, unsigned int length)
{
    if (length == 0)
    {
        // Flush buffer (if any)
        if (m_sampleIndex > 0)
            return processSamples();
        return 0;
    }
    if (nullptr == bytes)
        return 0;

    unsigned int index = 0;

    length /= sizeof(int16_t);

    while (index < length)
    {
        m_input[0][m_sampleIndex] = (FLAC__int32)(bytes[index++]);
        m_input[1][m_sampleIndex] = (FLAC__int32)(bytes[index++]);
        m_sampleIndex += 1;

        if (m_sampleIndex == MAX_SAMPLES)
        {
            int ret = processSamples();
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

int FlacEncoder::processSamples()
{
    if (!FLAC__stream_encoder_process(m_encoder,
                                      (const FLAC__int32 * const *) m_input.data(),
                                      m_sampleIndex))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to write flac data. Aborting."));
        return EENCODEERROR;
    }
    m_sampleIndex = 0;
    return 0;
}
