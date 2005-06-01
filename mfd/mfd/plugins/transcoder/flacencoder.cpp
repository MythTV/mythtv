#include <qstring.h>

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#include "metadata.h"
#include "flacencoder.h"

#include <FLAC/file_encoder.h>
#include <FLAC/assert.h>
#include <mythtv/mythcontext.h>

FlacEncoder::FlacEncoder(const QString &outfile, int qualitylevel,
                         AudioMetadata *metadata)
           : Encoder(outfile, qualitylevel, metadata)
{
    sampleindex = 0;

    int blocksize = 4608;
    bool do_exhaustive_model_search = false;
    bool do_escape_coding = false;
    bool do_mid_side = true;
    bool loose_mid_side = false;
    int qlp_coeff_precision = 0;
    int min_residual_partition_order = 3, max_residual_partition_order = 3;
    int rice_parameter_search_dist = 0;
    int max_lpc_order = 8;

    encoder = FLAC__file_encoder_new();

    FLAC__file_encoder_set_streamable_subset(encoder, true);
    FLAC__file_encoder_set_do_mid_side_stereo(encoder, do_mid_side);
    FLAC__file_encoder_set_loose_mid_side_stereo(encoder, loose_mid_side);
    FLAC__file_encoder_set_channels(encoder, NUM_CHANNELS);
    FLAC__file_encoder_set_bits_per_sample(encoder, 16);
    FLAC__file_encoder_set_sample_rate(encoder, 44100);
    FLAC__file_encoder_set_blocksize(encoder, blocksize);
    FLAC__file_encoder_set_max_lpc_order(encoder, max_lpc_order);
    FLAC__file_encoder_set_qlp_coeff_precision(encoder, qlp_coeff_precision);
    FLAC__file_encoder_set_do_qlp_coeff_prec_search(encoder, false);
    FLAC__file_encoder_set_do_escape_coding(encoder, do_escape_coding);
    FLAC__file_encoder_set_do_exhaustive_model_search(encoder, 
                                                    do_exhaustive_model_search);
    FLAC__file_encoder_set_min_residual_partition_order(encoder, 
                                                  min_residual_partition_order);
    FLAC__file_encoder_set_max_residual_partition_order(encoder, 
                                                  max_residual_partition_order);
    FLAC__file_encoder_set_rice_parameter_search_dist(encoder, 
                                                    rice_parameter_search_dist);

    FLAC__file_encoder_set_filename(encoder, outfile.local8Bit());

    int ret = FLAC__file_encoder_init(encoder);
    if (ret != FLAC__FILE_ENCODER_OK)
    {
        VERBOSE(VB_GENERAL, QString("Error initializing FLAC encoder."
                                    " Got return code: %1").arg(ret));
    }

    for (int i = 0; i < NUM_CHANNELS; i++)
        input[i] = &(inputin[i][0]); 
}

FlacEncoder::~FlacEncoder()
{
    addSamples(0, 0); // flush buffer

    if (encoder)
    {
        FLAC__file_encoder_finish(encoder);
        FLAC__file_encoder_delete(encoder);
    }

    if (metadata)
    {
/*
        MetaIOFLACVorbisComment *p_tagger = new MetaIOFLACVorbisComment;
        QString filename = metadata->Filename();
        QString tmp = *outfile;
        metadata->setFilename(tmp);
        p_tagger->write(metadata);
        metadata->setFilename(filename);
        delete p_tagger;
*/
    }
}

int FlacEncoder::addSamples(int16_t *bytes, unsigned int length)
{
    unsigned int index = 0;

    length /= sizeof(int16_t);

    do {
        while (index < length && sampleindex < MAX_SAMPLES) 
        {
            input[0][sampleindex] = (FLAC__int32)(bytes[index++]);
            input[1][sampleindex] = (FLAC__int32)(bytes[index++]);
            sampleindex += 1;
        }

        if(sampleindex == MAX_SAMPLES || (length == 0 && sampleindex > 0) ) 
        {
            if (!FLAC__file_encoder_process(encoder,
                                            (const FLAC__int32 * const *) input,
                                            sampleindex))
            {
                VERBOSE(VB_GENERAL, QString("Failed to write flac data."
                                            " Aborting."));
                return EENCODEERROR;
            }
            sampleindex = 0;
        }
    } while (index < length);

    return 0;
}

