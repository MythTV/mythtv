#include <qstring.h>
#include <qcstring.h>
#include <qapplication.h>
#include <qprogressbar.h>

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#include "metadata.h"
#include "flacdecoder.h"
#include "flacencoder.h"

#include <FLAC/file_encoder.h>
#include <FLAC/assert.h>

struct clientdata
{
    QProgressBar *progressbar;
    int writes;
    long long samplesdone;
};

void progress_callback(const FLAC__FileEncoder *encoder, 
                       FLAC__uint64 bytes_written, 
                       FLAC__uint64 samples_written, unsigned frames_written, 
                       unsigned total_frames_estimate, void *client_data)
{
    encoder = encoder;
    bytes_written = bytes_written;
    frames_written = frames_written;
    total_frames_estimate = total_frames_estimate;

    clientdata *clidata = (clientdata *)client_data;

    clidata->samplesdone = samples_written;

    clidata->writes++;
    if (clidata->writes == 5)
    {
        clidata->writes = 0;
        clidata->progressbar->setProgress(clidata->samplesdone);
        qApp->processEvents();
    }
}

void format_input(FLAC__int32 *dest[], FLAC__int16 *ssbuffer, 
                  unsigned char *ucbuffer, unsigned wide_samples, 
                  FLAC__bool is_big_endian, FLAC__bool is_unsigned_samples, 
                  unsigned channels, unsigned bps)
{       
    is_big_endian = is_big_endian;
    is_unsigned_samples = is_unsigned_samples;
    bps = bps;
    unsigned wide_sample, sample, channel;
        
    ucbuffer = ucbuffer;
            
    for (sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
        for (channel = 0; channel < channels; channel++, sample++)
            dest[channel][wide_sample] = (FLAC__int32)ssbuffer[sample];
}

void FlacEncode(const char *infile, const char *outfile, int qualitylevel, 
                Metadata *metadata, int totalbytes, QProgressBar *progressbar)
{
    qualitylevel = qualitylevel;

    long int totalsamples = (totalbytes / 4);
    progressbar->setTotalSteps(totalsamples);
    progressbar->setProgress(0);
    qApp->processEvents();

    totalsamples = totalbytes / 4;
    int blocksize = 4608;
    bool do_exhaustive_model_search = false;
    bool do_escape_coding = false;
    bool do_mid_side = true;
    bool loose_mid_side = false;
    int qlp_coeff_precision = 0;
    int min_residual_partition_order = 3, max_residual_partition_order = 3;
    int rice_parameter_search_dist = 0;
    int max_lpc_order = 8;

    FILE *in = fopen(infile, "r");

    FLAC__FileEncoder *encoder = FLAC__file_encoder_new();

    FLAC__file_encoder_set_streamable_subset(encoder, true);
    FLAC__file_encoder_set_do_mid_side_stereo(encoder, do_mid_side);
    FLAC__file_encoder_set_loose_mid_side_stereo(encoder, loose_mid_side);
    FLAC__file_encoder_set_channels(encoder, 2);
    FLAC__file_encoder_set_bits_per_sample(encoder, 16);
    FLAC__file_encoder_set_sample_rate(encoder, 44100);
    FLAC__file_encoder_set_blocksize(encoder, blocksize);
    FLAC__file_encoder_set_max_lpc_order(encoder, max_lpc_order);
    FLAC__file_encoder_set_qlp_coeff_precision(encoder, qlp_coeff_precision);
    FLAC__file_encoder_set_do_qlp_coeff_prec_search(encoder, false);
    FLAC__file_encoder_set_do_escape_coding(encoder, do_escape_coding);
    FLAC__file_encoder_set_do_exhaustive_model_search(encoder, do_exhaustive_model_search);
    FLAC__file_encoder_set_min_residual_partition_order(encoder, min_residual_partition_order);
    FLAC__file_encoder_set_max_residual_partition_order(encoder, max_residual_partition_order);
    FLAC__file_encoder_set_rice_parameter_search_dist(encoder, rice_parameter_search_dist);
    FLAC__file_encoder_set_total_samples_estimate(encoder, totalsamples);

    struct clientdata clidata;
    clidata.progressbar = progressbar;
    clidata.writes = 0;
    clidata.samplesdone = 0;

    FLAC__file_encoder_set_client_data(encoder, &clidata);
    FLAC__file_encoder_set_progress_callback(encoder, progress_callback);
    FLAC__file_encoder_set_filename(encoder, outfile);

    if (FLAC__file_encoder_init(encoder) != FLAC__FILE_ENCODER_OK)
    {
        cout << "Couldn't init encoder.\n";
        goto abort;
    }

    int bytes_read;
    unsigned char ucbuffer[2048 * FLAC__MAX_CHANNELS * ((FLAC__REFERENCE_CODEC_MAX_BITS_PER_SAMPLE + 7) / 8)];
    static FLAC__int32 inputin[FLAC__MAX_CHANNELS][2048];
    static FLAC__int32 *input[FLAC__MAX_CHANNELS];

    for (int i = 0; i < (int)FLAC__MAX_CHANNELS; i++)
        input[i] = &(inputin[i][0]); 

    while (!feof(in))
    {
        bytes_read = fread(ucbuffer, sizeof(unsigned char), 2048 * 2, in);

        if (bytes_read == 0)
        {
            if (ferror(in)) 
            {
                cout << "error during read\n";
                goto abort;
            }
        }
        else if (bytes_read % 2 != 0) 
        {
            cout << "got partial sample\n";
            goto abort;
        }

        unsigned wide_samples = bytes_read / 4;
        format_input(input, (FLAC__int16 *)ucbuffer, ucbuffer, wide_samples, 
                     false, false, 2, 16);

        if (!FLAC__file_encoder_process(encoder, 
                                        (const FLAC__int32 * const *) input, 
                                        wide_samples)) 
        {
            cout << "error during encoding\n";
            goto abort;
        }
    }

    if (encoder)
    {
        FLAC__file_encoder_finish(encoder);
        FLAC__file_encoder_delete(encoder);
    }

    fclose(in);

    if (metadata)
    {
        FlacDecoder *decoder = new FlacDecoder(outfile, NULL, NULL, NULL);
        decoder->commitMetadata(metadata);

        delete decoder;
    }

    return;
abort:
    FLAC__file_encoder_finish(encoder);
    FLAC__file_encoder_delete(encoder);

    fclose(in);
}
