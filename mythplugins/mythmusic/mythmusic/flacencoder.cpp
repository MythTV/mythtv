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

#include <FLAC/stream_encoder.h>
#include <FLAC/assert.h>

struct clientdata
{
    FILE *out;
    const char *outfile;
    QProgressBar *progressbar;
    FLAC__StreamMetadata_SeekTable *seektable; 
    int seekpointcheck;
    int byteswritten;
    int samplesdone;
    int writes;
    int streamoffset;
};

int seekpoint_compare(const FLAC__StreamMetadata_SeekPoint *l, const FLAC__StreamMetadata_SeekPoint *r)
{       
        /* we don't just 'return l->sample_number - r->sample_number' since the 
 * result (FLAC__int64) might overflow an 'int' */
        if(l->sample_number == r->sample_number)
                return 0;
        else if(l->sample_number < r->sample_number)
                return -1;
        else
                return 1;
}

FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], unsigned bytes, unsigned samples, unsigned current_frame, void *client_data)
{
    struct clientdata *clidata = (struct clientdata *)client_data;
    samples = samples;

    if (clidata->streamoffset > 0)
    {
        FLAC__uint64 currentsample = (FLAC__uint64)current_frame * (FLAC__uint64)FLAC__stream_encoder_get_blocksize(encoder), test_sample;

        for (unsigned i = clidata->seekpointcheck; i < clidata->seektable->num_points; i++)
        { 
            test_sample = clidata->seektable->points[i].sample_number;
            if (test_sample > currentsample)
                break;
            else if (test_sample == currentsample)
            {
                clidata->seektable->points[i].stream_offset = clidata->byteswritten - clidata->streamoffset;
                clidata->seektable->points[i].frame_samples = FLAC__stream_encoder_get_blocksize(encoder);
                clidata->seekpointcheck++;
                break;
            }
            else
                clidata->seekpointcheck++;
        }
    }

    clidata->byteswritten += bytes;
    clidata->samplesdone += samples;

    clidata->writes++;
    if (clidata->writes == 5)
    {
        clidata->writes = 0;
        clidata->progressbar->setProgress(clidata->samplesdone);
        qApp->processEvents();
    }

    if (fwrite(buffer, sizeof(FLAC__byte), bytes, clidata->out) == bytes)
        return FLAC__STREAM_ENCODER_WRITE_OK;
    else
        return FLAC__STREAM_ENCODER_WRITE_FATAL_ERROR;
}

FLAC__bool write_big_endian_uint16(FILE *f, FLAC__uint16 val)
{                       
 //   if(!is_big_endian_host) {
        FLAC__byte *b = (FLAC__byte *)&val, tmp;
        tmp = b[0]; b[0] = b[1]; b[1] = tmp;
//    }       
    return fwrite(&val, 1, 2, f) == 2;
}               

FLAC__bool write_big_endian_uint64(FILE *f, FLAC__uint64 val)
{
//    if (!is_big_endian_host) {
        FLAC__byte *b = (FLAC__byte *)&val, tmp;
        tmp = b[0]; b[0] = b[7]; b[7] = tmp;
        tmp = b[1]; b[1] = b[6]; b[6] = tmp;
        tmp = b[2]; b[2] = b[5]; b[5] = tmp;
        tmp = b[3]; b[3] = b[4]; b[4] = tmp;
//    }
    return fwrite(&val, 1, 8, f) == 8;
}

void metadata_callback(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    struct clientdata *clidata = (struct clientdata *)client_data;
    FLAC__byte b;
    FILE *f = clidata->out;  

    const FLAC__uint64 samples = metadata->data.stream_info.total_samples;
    const unsigned min_framesize = metadata->data.stream_info.min_framesize;
    const unsigned max_framesize = metadata->data.stream_info.max_framesize;

    FLAC__ASSERT(metadata->type == FLAC__METADATA_TYPE_STREAMINFO);

    encoder = encoder;

    if (f != stdout) {
        fclose(clidata->out);
        if (0 == (f = fopen(clidata->outfile, "r+")))
            return;
    }

    if(-1 == fseek(f, 26, SEEK_SET)) goto end_;
    fwrite(metadata->data.stream_info.md5sum, 1, 16, f);

    fseek(f, 21, SEEK_SET);
    if(fread(&b, 1, 1, f) != 1) goto framesize_;
    fseek(f, 21, SEEK_SET);
    b = (b & 0xf0) | (FLAC__byte)((samples >> 32) & 0x0F);
    if(fwrite(&b, 1, 1, f) != 1) goto framesize_;
    b = (FLAC__byte)((samples >> 24) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto framesize_;
    b = (FLAC__byte)((samples >> 16) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto framesize_;
    b = (FLAC__byte)((samples >> 8) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto framesize_;
    b = (FLAC__byte)(samples & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto framesize_;

framesize_:
    fseek(f, 12, SEEK_SET);
    b = (FLAC__byte)((min_framesize >> 16) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;
    b = (FLAC__byte)((min_framesize >> 8) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;
    b = (FLAC__byte)(min_framesize & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;
    b = (FLAC__byte)((max_framesize >> 16) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;
    b = (FLAC__byte)((max_framesize >> 8) & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;
    b = (FLAC__byte)(max_framesize & 0xFF);
    if(fwrite(&b, 1, 1, f) != 1) goto seektable_;

seektable_:
    if (clidata->seektable->num_points > 0) 
    {
        long pos;
        unsigned i;

        /* convert any unused seek points to placeholders */
        for (i = 0; i < clidata->seektable->num_points; i++) {
            if (clidata->seektable->points[i].sample_number == FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER)
                break;
            else if (clidata->seektable->points[i].frame_samples == 0)
                clidata->seektable->points[i].sample_number = FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER;
        }

        /* the offset of the seek table data 'pos' should be after then 
         * stream sync and STREAMINFO block and SEEKTABLE header */
        pos = (FLAC__STREAM_SYNC_LEN + FLAC__STREAM_METADATA_IS_LAST_LEN + FLAC__STREAM_METADATA_TYPE_LEN + FLAC__STREAM_METADATA_LENGTH_LEN) / 8;
        pos += metadata->length;
        pos += (FLAC__STREAM_METADATA_IS_LAST_LEN + FLAC__STREAM_METADATA_TYPE_LEN + FLAC__STREAM_METADATA_LENGTH_LEN) / 8;
        fseek(f, pos, SEEK_SET);
        for (i = 0; i < clidata->seektable->num_points; i++) {
           if (!write_big_endian_uint64(f, clidata->seektable->points[i].sample_number)) goto end_;
           if(!write_big_endian_uint64(f, clidata->seektable->points[i].stream_offset)) goto end_;
           if(!write_big_endian_uint16(f, (FLAC__uint16)clidata->seektable->points[i].frame_samples)) goto end_;
        }
    }

end_:
        fclose(f);
        return;
}

void format_input(FLAC__int32 *dest[], FLAC__int16 *ssbuffer, unsigned char *ucbuffer, unsigned wide_samples, FLAC__bool is_big_endian, FLAC__bool is_unsigned_samples, unsigned channels, unsigned bps)
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

void FlacEncode(const char *infile, const char *outfile, 
                int qualitylevel, Metadata *metadata, int totalbytes,
                QProgressBar *progressbar)
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
    FILE *out = fopen(outfile, "w");

    int i, j;

    FLAC__StreamEncoder *encoder = FLAC__stream_encoder_new();

    FLAC__StreamMetadata_SeekTable seek_table;
    seek_table.points = (FLAC__StreamMetadata_SeekPoint *)malloc(sizeof(FLAC__StreamMetadata_SeekPoint) * (100));
    seek_table.num_points = 0;

    for (i = 0; i < 100; i++) 
    {
        seek_table.points[i].sample_number = FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER;
        seek_table.points[i].stream_offset = 0;
        seek_table.points[i].frame_samples = 0;
    }

    for (j = 0; j < 100; j++)
    {
        const FLAC__uint64 sample = totalsamples * j / 100;
        const FLAC__uint64 target_sample = (sample / blocksize) * blocksize;

        if (target_sample < (unsigned)totalsamples)
            seek_table.points[seek_table.num_points++].sample_number = target_sample;
    }

    qsort(seek_table.points, seek_table.num_points, sizeof(FLAC__StreamMetadata_SeekPoint), (int (*)(const void *, const void *))seekpoint_compare);

    bool first = true;
    for (i = j = 0; i < (int)seek_table.num_points; i++) 
    {
        if (!first)
        {
            if (seek_table.points[i].sample_number == seek_table.points[j - 1].sample_number)
                continue;
        }
        first = false;
        seek_table.points[j++] = seek_table.points[i];
    }
    seek_table.num_points = j;

    FLAC__StreamMetadata seektabletable;
    int num_metadata = 0;
    FLAC__StreamMetadata *flacmetadata[2];

    if (seek_table.num_points > 0)
    {
        seektabletable.is_last = false;
        seektabletable.type = FLAC__METADATA_TYPE_SEEKTABLE;
        seektabletable.length = seek_table.num_points * FLAC__STREAM_METADATA_SEEKPOINT_LENGTH;
        seektabletable.data.seek_table = seek_table;

        flacmetadata[num_metadata++] = &seektabletable;
    }

    FLAC__stream_encoder_set_streamable_subset(encoder, true);
    FLAC__stream_encoder_set_do_mid_side_stereo(encoder, do_mid_side);
    FLAC__stream_encoder_set_loose_mid_side_stereo(encoder, loose_mid_side);
    FLAC__stream_encoder_set_channels(encoder, 2);
    FLAC__stream_encoder_set_bits_per_sample(encoder, 16);
    FLAC__stream_encoder_set_sample_rate(encoder, 44100);
    FLAC__stream_encoder_set_blocksize(encoder, blocksize);
    FLAC__stream_encoder_set_max_lpc_order(encoder, max_lpc_order);
    FLAC__stream_encoder_set_qlp_coeff_precision(encoder, qlp_coeff_precision);
    FLAC__stream_encoder_set_do_qlp_coeff_prec_search(encoder, false);
    FLAC__stream_encoder_set_do_escape_coding(encoder, do_escape_coding);
    FLAC__stream_encoder_set_do_exhaustive_model_search(encoder, do_exhaustive_model_search);
    FLAC__stream_encoder_set_min_residual_partition_order(encoder, min_residual_partition_order);
    FLAC__stream_encoder_set_max_residual_partition_order(encoder, max_residual_partition_order);
    FLAC__stream_encoder_set_rice_parameter_search_dist(encoder, rice_parameter_search_dist);
    FLAC__stream_encoder_set_total_samples_estimate(encoder, totalsamples);
    FLAC__stream_encoder_set_metadata(encoder, flacmetadata, 1);
    FLAC__stream_encoder_set_write_callback(encoder, write_callback);
    FLAC__stream_encoder_set_metadata_callback(encoder, metadata_callback);

    struct clientdata clidata;
    clidata.out = out;
    clidata.outfile = outfile;
    clidata.progressbar = progressbar;
    clidata.seektable = &seek_table;
    clidata.seekpointcheck = 0;
    clidata.byteswritten = 0;
    clidata.streamoffset = 0;
    clidata.writes = 0;
    clidata.samplesdone = 0;

    FLAC__stream_encoder_set_client_data(encoder, &clidata);

    if (FLAC__stream_encoder_init(encoder) != FLAC__STREAM_ENCODER_OK)
    {
        cout << "Couldn't init encoder.\n";
        goto abort;
    }

    clidata.streamoffset = clidata.byteswritten;

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
        format_input(input, (FLAC__int16 *)ucbuffer, ucbuffer, wide_samples, false, false, 2, 16);

        if (!FLAC__stream_encoder_process(encoder, (const FLAC__int32 * const *) input, wide_samples)) 
        {
            cout << "error during encoding\n";
            goto abort;
        }
    }

    if (encoder)
    {
        FLAC__stream_encoder_finish(encoder);
        FLAC__stream_encoder_delete(encoder);
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
    FLAC__stream_encoder_finish(encoder);
    FLAC__stream_encoder_delete(encoder);

    free(seek_table.points);

    fclose(in);
    fclose(out);
    unlink(outfile);
}
