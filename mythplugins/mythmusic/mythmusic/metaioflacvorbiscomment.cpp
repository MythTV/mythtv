#include <iostream>
#include <sys/stat.h>
using namespace std;

#include "metaioflacvorbiscomment.h"
#include "metadata.h"


#include <mythtv/mythcontext.h>

MetaIOFLACVorbisComment::MetaIOFLACVorbisComment(void)
    : MetaIO(".flac")
{
}

MetaIOFLACVorbisComment::~MetaIOFLACVorbisComment(void)
{
}


//==========================================================================
/*!
 * \brief Writes metadata back to a file
 *
 * \param mdata A pointer to a Metadata object
 * \param exclusive Flag to indicate if only the data in mdata should be
 *                  in the file. If false, any unrecognised tags already
 *                  in the file will be maintained.
 * \returns Boolean to indicate success/failure.
 */
bool MetaIOFLACVorbisComment::write(Metadata* mdata, bool exclusive)
{
    exclusive = exclusive; // -Wall annoyance
    
    // Sanity check.
    if (!mdata)
        return false;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    if (!FLAC__metadata_chain_read(chain, mdata->Filename().local8Bit())
        || !FLAC__metadata_chain_read(chain, mdata->Filename().ascii()))
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
            found_vc_block = true;
    } while (!found_vc_block && FLAC__metadata_iterator_next(iterator));
    
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
        FLAC__metadata_object_vorbiscomment_resize_comments(block, 0);

    setComment(block, "artist", mdata->Artist());
    setComment(block, "album", mdata->Album());
    setComment(block, "title", mdata->Title());
    setComment(block, "genre", mdata->Genre());

    char text[128];
    if (0 != mdata->Track())
    {
        snprintf(text, 128, "%d", mdata->Track());
        setComment(block, "tracknumber", text);
    }

    if (0 != mdata->Year())
    {
        snprintf(text, 128, "%d", mdata->Year());
        setComment(block, "date", text);
    }

    FLAC__metadata_chain_write(chain, false, false);

    FLAC__metadata_chain_delete(chain);
    FLAC__metadata_iterator_delete(iterator);

    return true;
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOFLACVorbisComment::read(QString filename)
{
    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();
    if (!FLAC__metadata_chain_read(chain, filename.local8Bit())
        || !FLAC__metadata_chain_read(chain, filename.ascii()))
    {
        FLAC__metadata_chain_delete(chain); 
        return NULL;
    }

    bool found_vc_block = false;
    FLAC__StreamMetadata *block = 0;
    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
    
    FLAC__metadata_iterator_init(iterator, chain);

    block = FLAC__metadata_iterator_get_block(iterator);

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_STREAMINFO);

    int samplerate = block->data.stream_info.sample_rate;
    int totalsamples = block->data.stream_info.total_samples;
    int bytesperms = (samplerate) / 1000;

    length = totalsamples / bytesperms;

    do {
        block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
            found_vc_block = true;
    } while (!found_vc_block && FLAC__metadata_iterator_next(iterator));

    if (!found_vc_block)
    {
        FLAC__metadata_chain_delete(chain);
        FLAC__metadata_iterator_delete(iterator);
        return NULL;
    }

    FLAC__ASSERT(0 != block);
    FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);

    title = getComment(block, "title");

    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
        artist = getComment(block, "artist");
        album = getComment(block, "album");
        genre = getComment(block, "genre");
        tracknum = getComment(block, "tracknumber").toInt(); 
        year = getComment(block, "date").toInt();
    }

    FLAC__metadata_chain_delete(chain);
    FLAC__metadata_iterator_delete(iterator);

    length = getTrackLength(filename);

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOFLACVorbisComment::getTrackLength(QString filename)
{
    filename=filename; // -Wall annoyance
    return 0;
}


//==========================================================================
/*!
 * \brief Function to return an individual comment from an FLACVorbis comment
 *
 * \param pBlock Pointer to a FLAC Metadata block
 * \param pLabel The label of the comment you want
 * \returns QString containing the contents of the comment you want
 */
QString MetaIOFLACVorbisComment::getComment(FLAC__StreamMetadata* pBlock, 
                                            const char* pLabel)
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


//==========================================================================
/*!
 * \brief Function to return an individual comment from an FLACVorbis comment
 *
 * \param pBlock Pointer to a FLAC Metadata block
 * \param pLabel The label of the comment you want
 * \param rData A reference to the data you want to write
 * \returns Nothing
 */
void MetaIOFLACVorbisComment::setComment(FLAC__StreamMetadata* pBlock,
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
