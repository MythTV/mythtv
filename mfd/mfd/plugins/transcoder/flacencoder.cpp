#include <qstring.h>

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#include "metadata.h"
#include "flacencoder.h"

//  #include <mythtv/mythcontext.h>

FlacEncoder::FlacEncoder(const QString &l_outfile, int qualitylevel,
                         AudioMetadata *l_metadata)
           : Encoder(l_outfile, qualitylevel, l_metadata)
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
        warning(QString("Error initializing FLAC encoder."
                        " Got return code: %1").arg(ret));
    }

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        input[i] = &(inputin[i][0]); 
    }
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
        if(! addMetadataTags())
        {
            warning(QString("problem writing metadata tags for %1")
                            .arg(outfile));
        }
    }
}


QString FlacEncoder::getComment(FLAC__StreamMetadata* pBlock, const char* pLabel)
{
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    entry = pBlock->data.vorbis_comment.comments;

    QString qlabel = pLabel;
    QString retstr = "";
    for (unsigned int i = 0; i < pBlock->data.vorbis_comment.num_comments; i++)
    {
        char *fieldname = new char[(entry + i)->length + 1];
        fieldname[(entry + i)->length] = 0;
        strncpy(fieldname, (char *)((entry + i)->entry), (entry + i)->length);
        QString entrytext = fieldname;
        delete [] fieldname;
        int loc;

        if ((loc = entrytext.find("=")) &&
            entrytext.lower().left(qlabel.length()) == qlabel.lower())
        {
            return QString::fromUtf8(entrytext.right(entrytext.length() - loc - 1));
        }
    }

    return "";
}   

void FlacEncoder::setComment(FLAC__StreamMetadata* pBlock,
                                         const char* pLabel,
                                         const QString& rData)
{
    if (rData.length() < 1)
        return;

    QString test = getComment(pBlock, pLabel);

    QString thenewentry = QString(pLabel).upper() + "=" + rData;
    QCString utf8str = thenewentry.utf8();
    int thenewentrylen = utf8str.length();

    FLAC__StreamMetadata_VorbisComment_Entry entry;

    entry.length = thenewentrylen;
    entry.entry = (unsigned char *)utf8str.data();

    FLAC__metadata_object_vorbiscomment_insert_comment(pBlock,
                                                       pBlock->data.vorbis_comment.num_comments,
                                                       entry, true);
}


bool FlacEncoder::addMetadataTags()
{

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    if (!FLAC__metadata_chain_read(chain, outfile.local8Bit())
        && !FLAC__metadata_chain_read(chain, outfile.ascii()))
    {
        FLAC__metadata_chain_delete(chain);
        return false;
    }

    bool found_vc_block = false;
    FLAC__StreamMetadata *block = 0;
    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iterator, chain);

    do {
        block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        {
            found_vc_block = true;
        }
       } 
       while (!found_vc_block && FLAC__metadata_iterator_next(iterator));
    
    if (!found_vc_block)
    {
        block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

        while (FLAC__metadata_iterator_next(iterator))
            ;

        if (!FLAC__metadata_iterator_insert_block_after(iterator, block))
        {
            FLAC__metadata_chain_delete(chain);
            FLAC__metadata_iterator_delete(iterator);
            return false;
        }

        FLAC__ASSERT(FLAC__metadata_iterator_get_block(iterator) == block);
    }

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);


    // Should probably deal with "exclusive" here....
    if (block->data.vorbis_comment.comments > 0)
    {
        FLAC__metadata_object_vorbiscomment_resize_comments(block, 0);
    }

    setComment(block, "ARTIST", metadata->getArtist());

    if (metadata->getCompilation())
    {
        // We use the MusicBrainz Identifier to indicate a compilation
        setComment(block, "MUSICBRAINZ_ALBUMARTISTID", 
              "89ad4ac3-39f7-470e-963a-56509c546377");

        if (metadata->getCompilationArtist().length() > 0)
        {
            setComment(block, "ALBUM ARTIST",
                       metadata->getCompilationArtist());
        }
        
    }

    setComment(block, "ALBUM", metadata->getAlbum());
    setComment(block, "TITLE", metadata->getTitle());
    setComment(block, "GENRE", metadata->getGenre());

    char text[128];
    if (metadata->getTrack() != 0)
    {
        snprintf(text, 128, "%d", metadata->getTrack());
        setComment(block, "TRACKNUMBER", text);
    }

    if (metadata->getYear() > 0)
    {
        snprintf(text, 128, "%d", metadata->getYear());
        setComment(block, "DATE", text);
    }

    FLAC__metadata_chain_write(chain, false, false);

    FLAC__metadata_chain_delete(chain);
    FLAC__metadata_iterator_delete(iterator);

    return true;
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
                warning("Failed to write flac data. Giving Up.");
                return EENCODEERROR;
            }
            sampleindex = 0;
        }
    } while (index < length);

    return 0;
}

