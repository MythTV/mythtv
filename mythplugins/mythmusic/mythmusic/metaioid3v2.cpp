#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
using namespace std;

#include <mad.h>

#include "metaioid3v2.h"
#include "metadata.h"

#include "metaio_libid3hack.h"


#include <mythtv/mythcontext.h>

//==========================================================================
MetaIOID3v2::MetaIOID3v2(void)
    : MetaIO(".mp3")
{
}

//==========================================================================
MetaIOID3v2::~MetaIOID3v2(void)
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
bool MetaIOID3v2::write(Metadata* mdata, bool exclusive)
{
    // Sanity check.
    if (!mdata)
        return false;

    id3_file* p_input = NULL;
    
    p_input = id3_file_open(mdata->Filename().local8Bit(), ID3_FILE_MODE_READWRITE);
    if (!p_input)
        p_input = id3_file_open(mdata->Filename().ascii(), ID3_FILE_MODE_READWRITE);
  
    if (!p_input)
      return false;

    // We don't like id3v1 tags... too limiting.
    // We don't write them on encoding, so we delete them if one exists.
    id3_tag *tag = id3_file_tag(p_input);
    if (!tag)
    {
        id3_file_close(p_input);
        return false;
    }

    if (exclusive)
    {
       id3_tag_clearframes(tag);
    }

    if (!mdata->Artist().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_ARTIST);
        setComment(tag, ID3_FRAME_ARTIST, mdata->Artist());
    }
    
    if (mdata->Compilation())
    {
        if (!exclusive)
            removeComment(tag, 
                          MYTH_ID3_FRAME_COMMENT,
                          MYTH_ID3_FRAME_MUSICBRAINZ_ALBUMARTISTDESC);
                          
        setComment(tag, MYTH_ID3_FRAME_COMMENT,
                   MYTH_MUSICBRAINZ_ALBUMARTIST_UUID, 
                   MYTH_ID3_FRAME_MUSICBRAINZ_ALBUMARTISTDESC);
        
        if (!mdata->CompilationArtist().isEmpty())
        {
            if (!exclusive) 
                removeComment(tag, MYTH_ID3_FRAME_COMPILATIONARTIST);
                
            setComment(tag, MYTH_ID3_FRAME_COMPILATIONARTIST,
                       mdata->CompilationArtist());        
        }
    }
    else if (!exclusive) 
    {
        removeComment(tag, 
                      MYTH_ID3_FRAME_COMMENT,
                      MYTH_ID3_FRAME_MUSICBRAINZ_ALBUMARTISTDESC);
                      
        removeComment(tag, MYTH_ID3_FRAME_COMPILATIONARTIST);
      
    }

    if (!mdata->Title().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_TITLE);
        setComment(tag, ID3_FRAME_TITLE, mdata->Title());
    }
    
    if (!mdata->Album().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_ALBUM);
        setComment(tag, ID3_FRAME_ALBUM, mdata->Album());
    }
    
    if (mdata->Year() > 999
        && mdata->Year() < 10000) // 4 digit year.
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_YEAR);
        setComment(tag, ID3_FRAME_YEAR, QString("%1").arg(mdata->Year()));
    }

    // Write Genre maintaining the ID3v1 genre number if applicable
    QString data = mdata->Genre();
    if (!data.isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_GENRE);

        id3_ucs4_t* p_ucs4 = 
          id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)data.utf8());

        int genrenum = id3_genre_number(p_ucs4);

        free(p_ucs4);

        // Use the number if it's standard, otherwise just write it (valid in ID3v2)
        if (genrenum >= 0)
            setComment(tag, ID3_FRAME_GENRE, 
                       QString("%1").arg(genrenum));
        else
            setComment(tag, ID3_FRAME_GENRE, data);
    }    
    
    if (0 != mdata->Track())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_TRACK);
        setComment(tag, ID3_FRAME_TRACK, QString("%1").arg(mdata->Track()));
    }


    
    id3_tag_options(tag, ID3_TAG_OPTION_COMPRESSION, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_CRC, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_UNSYNCHRONISATION, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_ID3V1, 0);
    
    // The id3_file_update function has a feature deficit.
    // See the files: metaio_libid3hack.c/h
    bool rv = (0 == ID3_FILE_UPDATE(p_input));

    return (0 == id3_file_close(p_input)) && rv;
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOID3v2::read(QString filename)
{
    QString artist = "", compilation_artist = "", album = "", title = "", 
            genre = "";
    int year = 0, tracknum = 0, length = 0;
    bool compilation = false;
    id3_file *p_input = NULL;
    
    p_input = id3_file_open(filename.local8Bit(), ID3_FILE_MODE_READONLY);
    if (!p_input)
        p_input = id3_file_open(filename.ascii(), ID3_FILE_MODE_READONLY);

    if (p_input)
    {
        id3_tag *tag = id3_file_tag(p_input);
        
        if (!tag)
        {
            id3_file_close(p_input);
            return NULL;
        }
        
        title = getComment(tag, ID3_FRAME_TITLE);
        artist = getComment(tag, ID3_FRAME_ARTIST);
        compilation_artist = getComment(tag, MYTH_ID3_FRAME_COMPILATIONARTIST);
        album = getComment(tag, ID3_FRAME_ALBUM);
        compilation = (MYTH_MUSICBRAINZ_ALBUMARTIST_UUID 
                       == getComment(tag,
                                     MYTH_ID3_FRAME_COMMENT,
                                     MYTH_ID3_FRAME_MUSICBRAINZ_ALBUMARTISTDESC));
                                          
        // Get Track Num dealing with 1/16, 2/16 etc. format
        tracknum = getComment(tag, ID3_FRAME_TRACK)
            .replace(QRegExp("^([0-9]*).*"), QString("\\1")).toInt();
        
        // NB Year could be TDRC or TYER depending on version....
        // From: http://www.id3.org/id3v2.4.0-structure.txt
        // Time stamps can be: yyyy, yyyy-MM, yyyy-MM-dd, yyyy-MM-ddTHH,
        //                     yyyy-MM-ddTHH:mm and yyyy-MM-ddTHH:mm:ss
        // Basically all starting yyyy.
        
        // Depending on the version of libid3tag, it will reassign a #define,
        // but we want to look for both.
        year = getComment(tag, ID3_FRAME_YEAR)
            .replace(QRegExp("^([0-9]{4}).*"), QString("\\1")).toInt();
        if (0 == year)
            year = getComment(tag, "TYER").toInt();
        
        // Genre.
        genre = getComment(tag, ID3_FRAME_GENRE);
        
        // Genre in ID3v2 = "genrenum|Genre Name"
        QString genrenumtest = genre;
        genrenumtest.replace(QRegExp("^[0-9]*$"), QString(""));
        
        if (genrenumtest.isEmpty())
        {
            // This means the genre is 100% numeric
            // Try and decode genre number        
            id3_ucs4_t *p_tmp = 
                id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)genre.utf8());
            
            id3_ucs4_t const *p_ucs4 = id3_genre_name(p_tmp);
            free(p_tmp);
    
            id3_utf8_t *p_utf8 = id3_ucs4_utf8duplicate(p_ucs4);  
            genre = QString::fromUtf8((const char*)p_utf8);
            
            free(p_utf8);
        }

        id3_file_close(p_input);
    }

    // Fallback to filename reading
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }

    length = getTrackLength(filename);

    Metadata *retdata = new Metadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length);
                                     
    retdata->setCompilation(compilation);

    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
int MetaIOID3v2::getTrackLength(QString filename)
{
    struct mad_stream stream;
    struct mad_header header;
    mad_timer_t timer;

    unsigned char buffer[8192];
    unsigned int buflen = 0;

    mad_stream_init(&stream);
    mad_header_init(&header);
   
    timer = mad_timer_zero;

    FILE *input = fopen(filename.local8Bit(), "r");
    if (!input)
        input = fopen(filename.ascii(), "r");

    if (!input)
        return 0;

    struct stat s;
    fstat(fileno(input), &s);
    unsigned long old_bitrate = 0;
    bool vbr = false;
    int amount_checked = 0;
    int alt_length = 0;
    bool loop_de_doo = true;
    
    while (loop_de_doo) 
    {
        if (buflen < sizeof(buffer)) 
        {
            int bytes;
            bytes = fread(buffer + buflen, 1, sizeof(buffer) - buflen, input);
            if (bytes <= 0)
                break;
            buflen += bytes;
        }

        mad_stream_buffer(&stream, buffer, buflen);

        while (1)
        {
            if (mad_header_decode(&header, &stream) == -1)
            {
                if (!MAD_RECOVERABLE(stream.error))
                {
                    break;
                }
                if (stream.error == MAD_ERROR_LOSTSYNC)
                {
                    int tagsize = id3_tag_query(stream.this_frame,
                                                stream.bufend - 
                                                stream.this_frame);
                    if (tagsize > 0)
                    {
                        mad_stream_skip(&stream, tagsize);
                        s.st_size -= tagsize;
                    }
                }
            }
            else
            {
                if(amount_checked == 0)
                {
                    old_bitrate = header.bitrate;
                }
                else if(header.bitrate != old_bitrate)
                {
                    vbr = true;
                }
                if(amount_checked == 32 && !vbr)
                {
                    alt_length = (s.st_size * 8) / (old_bitrate / 1000);
                    loop_de_doo = false;
                    break;
                }
                amount_checked++;
                mad_timer_add(&timer, header.duration);
            }
            
        }
        
        if (stream.error != MAD_ERROR_BUFLEN)
            break;

        memmove(buffer, stream.next_frame, &buffer[buflen] - stream.next_frame);
        buflen -= stream.next_frame - &buffer[0];
    }

    mad_header_finish(&header);
    mad_stream_finish(&stream);

    fclose(input);

    if (vbr)
        return mad_timer_count(timer, MAD_UNITS_MILLISECONDS);

    return alt_length;
}


inline QString MetaIOID3v2::getRawID3String(union id3_field *pField)
{
    QString tmp = "";
        
    id3_ucs4_t const *p_ucs4 = (id3_ucs4_t *) id3_field_getstring(pField);
    
    if (p_ucs4)
    {
        id3_utf8_t *p_utf8 = id3_ucs4_utf8duplicate(p_ucs4);
        
        if (!p_utf8)
             return "";

        tmp = QString::fromUtf8((const char*)p_utf8);

        free(p_utf8);
    }
    else
    {
        unsigned int nstrings = id3_field_getnstrings(pField);
    
        for (unsigned int j=0; j<nstrings; ++j)
        {
            p_ucs4 = id3_field_getstrings(pField, j);

            if (!p_ucs4)
                break;

           id3_utf8_t *p_utf8 = id3_ucs4_utf8duplicate(p_ucs4);

           if (!p_utf8)
               break;

           tmp += QString::fromUtf8((const char*)p_utf8);

           free(p_utf8);
       }
    }
    
    return tmp;
  
}

//==========================================================================
/*!
 * \brief Function to remove an individual comment in an ID3v2 Tag
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want to delete
 * \param desc Optional descripton to delete (frame type pLabel must support this)
 * \returns Nothing
 */
void MetaIOID3v2::removeComment(id3_tag *pTag,
                                const char* pLabel,
                                const QString desc)
{
    if (!pLabel)
        return;
    
    struct id3_frame* p_frame = NULL;
    
    bool just_delete = desc.isEmpty();
    
    for (int i=0; NULL != (p_frame = id3_tag_findframe(pTag, pLabel, i)); ++i)
    {
        if (just_delete)
        {
            // Let's delete it!!
            if (0 == id3_tag_detachframe(pTag, p_frame))
                id3_frame_delete(p_frame);
        }
        else
        {
            // Get the description to compare
            QString tmp = getRawID3String(&p_frame->fields[1]);

            // Let's compare the descriptions.
            if (tmp == desc)
            {
                // Now we know the descriptions match so we delete the frame.
                if (0 == id3_tag_detachframe(pTag, p_frame))
                    id3_frame_delete(p_frame);
                
                // We're done
                break;
            }
        }
    }
}


//==========================================================================
/*!
 * \brief Function to return an individual comment from an ID3v2 comment
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want
 * \param desc An optional description (frame type pLabel must support this)
 * \returns QString containing the contents of the comment you want
 */
QString MetaIOID3v2::getComment(id3_tag *pTag, const char* pLabel, 
                                const QString desc)
{
    if (!pLabel)
        return "";

    struct id3_frame *p_frame = NULL;
    
    for (int i=0; NULL != (p_frame = id3_tag_findframe(pTag, pLabel, i)); ++i ) 
    {
        int field_num = 1;
        
        QString tmp = "";
        
        // Compare the first field with the description if supplied
        if (!desc.isEmpty())
        {
            tmp = getRawID3String(&p_frame->fields[field_num++]);

            // Now compare tmp to desc
            if (tmp != desc)
            {
                // No match - move on.
                continue;
            }      
        }
          
        // Get the value and return it.
        tmp = getRawID3String(&p_frame->fields[field_num]);

        return tmp;
    }
    
    // Not found.
    return "";
}

//==========================================================================
/*!
 * \brief Function to set an individual comment in an ID3v2 Tag
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want
 * \param value A reference to the data you want to write
 * \param desc An optional description (frame type pLabel must support this)
 * \returns Nothing
 */
bool MetaIOID3v2::setComment(id3_tag *pTag,
                             const char* pLabel, 
                             const QString value,
                             const QString desc)
{
    if (!pLabel || "" == value)
      return false;

    struct id3_frame* p_frame = NULL;
    id3_ucs4_t* p_ucs4 = NULL;

    p_frame = id3_frame_new(pLabel);

    if (NULL == p_frame)
      return false;

    if (id3_field_settextencoding(&p_frame->fields[0],
                                  ID3_FIELD_TEXTENCODING_UTF_16) != 0)
    {
        id3_frame_delete(p_frame);
        return false;
    }

    // Write a description in field 1 if needs be.
    if (!desc.isEmpty())
    {
        p_ucs4 = id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)desc.utf8());
    
        if (!p_ucs4)
        {
            id3_frame_delete(p_frame);
            return false;      
        }
    
        if (0 != id3_field_setstring(&p_frame->fields[1], p_ucs4))
        {
            free(p_ucs4);
            id3_frame_delete(p_frame);
            return false;      
        }
          
        free(p_ucs4);
    }

    p_ucs4 = id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)value.utf8());

    if (!p_ucs4)
    {
        id3_frame_delete(p_frame);
        return false;      
    }

    if ((desc.isEmpty() && id3_field_setstrings(&p_frame->fields[1], 1, &p_ucs4))
        || (!desc.isEmpty() && id3_field_setstring(&p_frame->fields[2], p_ucs4)))
    {
        free(p_ucs4);
        id3_frame_delete(p_frame);
        return false;      
    }

    free(p_ucs4);

    if (0 != id3_tag_attachframe(pTag, p_frame))
    {
        id3_frame_delete(p_frame);
        return false;      
    }

    return true;
}
