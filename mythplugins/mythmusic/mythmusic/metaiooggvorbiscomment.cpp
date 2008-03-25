// POSIX headers
#include <unistd.h>
#include <sys/stat.h>

// C headers
#include <cmath>

// C++ headers
#include <cstdlib>
#include <iostream>
//Added by qt3to4:
#include <Q3CString>
using namespace std;

// MythTV headers
#include <mythtv/compat.h>
#include <mythtv/util.h>

// MythMusic headers
#include "metaiooggvorbiscomment.h"
#include "metaiovorbiscomment.h"
#include "metadata.h"
#include "vcedit.h"
#include <vorbis/vorbisfile.h>
#include <qfileinfo.h>

//==========================================================================
MetaIOOggVorbisComment::MetaIOOggVorbisComment(void)
    : MetaIO(".ogg")
{
}


//==========================================================================
MetaIOOggVorbisComment::~MetaIOOggVorbisComment(void)
{
}


//==========================================================================
/*!
 * \brief Low Level Function to get the raw vorbis comment block
 *
 * \note Typically used when encoding a file at the same time
 *
 * \param mdata A pointer to a Metadata object
 * \returns A vorbis_comment pointer or NULL on error
 */
vorbis_comment* 
MetaIOOggVorbisComment::getRawVorbisComment(Metadata* mdata,
                                            vorbis_comment* pComment)
{
    // Sanity check.
    if (!mdata)
        return NULL;

    vorbis_comment* p_comment = new vorbis_comment;

    if (!p_comment)
        return NULL;

    vorbis_comment_init(p_comment);
    
    if (pComment)
    {
        // We leave most comments alone, but we
        // need to clear out the ones we're using to prevent doublers.
        
        // This seems a little pointless but is the easiest thing to do.
        QString tmp;
        for (int i=0; i<pComment->comments; ++i)
        {
            // find the tagname
            tmp = pComment->user_comments[i];
            int tag = tmp.find('=');
            if (tag)
            {
                tmp = tmp.left(tag).upper();
                if (MYTH_VORBISCOMMENT_ARTIST != tmp
                    && MYTH_VORBISCOMMENT_COMPILATIONARTIST != tmp
                    && MYTH_VORBISCOMMENT_TITLE != tmp
                    && MYTH_VORBISCOMMENT_ALBUM != tmp
                    && MYTH_VORBISCOMMENT_GENRE != tmp
                    && MYTH_VORBISCOMMENT_TRACK != tmp
                    && MYTH_VORBISCOMMENT_MUSICBRAINZ_ALBUMARTISTID != tmp)
                {
                    vorbis_comment_add(p_comment, pComment->user_comments[i]);
                }
            }
        }
        
        // Now need to copy our modified comment back to the one passed in.
        vorbis_comment_clear(pComment);
        vorbis_comment_init(pComment);
        if (p_comment->comments > 0)
        {
            for (int i=0; i<p_comment->comments; ++i)
            {
                vorbis_comment_add(pComment, p_comment->user_comments[i]);
            }
        }
        
        // Clean up and Set the pointer to use for the rest of the program
        vorbis_comment_clear(p_comment);
        delete p_comment;
        p_comment = pComment;
    }

    Q3CString utf8str;
    if (!mdata->Artist().isEmpty())
    {
        utf8str = mdata->Artist().utf8();
        char *artist = utf8str.data();
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_ARTIST,
                               artist);    
    }
    
    if (mdata->Compilation())
    {
        // We use the MusicBrainz Identifier to indicate a compilation
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_MUSICBRAINZ_ALBUMARTISTID,
                               MYTH_MUSICBRAINZ_ALBUMARTIST_UUID);

        if (!mdata->CompilationArtist().isEmpty())
        {
            utf8str = mdata->CompilationArtist().utf8();
            char *compilation_artist = utf8str.data();
            vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_COMPILATIONARTIST,
                                   compilation_artist); 
            
        }
    }
        
    if (!mdata->Title().isEmpty())
    {
        utf8str = mdata->Title().utf8();
        char *title = utf8str.data();
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_TITLE,
                               title);
    }
    
    if (!mdata->Album().isEmpty())
    {
        utf8str = mdata->Album().utf8();
        char *album = utf8str.data();
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_ALBUM,
                               album);
    }
    
    if (!mdata->Genre().isEmpty())
    {
        utf8str = mdata->Genre().utf8();
        char *genre = utf8str.data();
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_GENRE,
                               genre);
    }
    
    if (0 != mdata->Track())
    {
        char tracknum[10];
        snprintf(tracknum, 9, "%d", mdata->Track());
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_TRACK,
                               tracknum);
    }
    
    if (0 != mdata->Year())
    {
        char year[10];
        snprintf(year, 9, "%d", mdata->Year());
        vorbis_comment_add_tag(p_comment, (char *)MYTH_VORBISCOMMENT_DATE,
                               year);
    }
    
    
    return p_comment;
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
bool MetaIOOggVorbisComment::write(Metadata* mdata, bool exclusive)
{
    // Sanity check
    if (!mdata)
        return false;

    FILE* p_input = NULL;
    p_input = fopen(mdata->Filename().local8Bit(), "rb");
    if (!p_input)
        p_input = fopen(mdata->Filename().ascii(), "rb");
    
    if (!p_input)
        return false;

    QString newfilename = createTempFile(
        mdata->Filename().local8Bit() + ".XXXXXX");

    FILE *p_output = fopen(newfilename, "wb");

    if (!p_output)
    {
        fclose(p_input);
        return false;
    }

    vcedit_state* p_state = vcedit_new_state();

    if (vcedit_open(p_state, p_input) < 0)
    {
        vcedit_clear(p_state);
        fclose(p_input);
        fclose(p_output);
        return false;
    }
    
    // grab and clear the exisiting comments
    vorbis_comment* p_comment = vcedit_comments(p_state);

    if (exclusive) 
    {
        vorbis_comment_clear(p_comment);
        vorbis_comment_init(p_comment);
    }

    if (!getRawVorbisComment(mdata, p_comment))
    {
        vcedit_clear(p_state);
        fclose(p_input);
        fclose(p_output);
        return false;
    }

   
    // write out the modified stream
    if (vcedit_write(p_state, p_output) < 0)
    {
        vcedit_clear(p_state);
        fclose(p_input);
        fclose(p_output);
        return false;
    }
    
    // done
    vcedit_clear(p_state);
    fclose(p_input);
    fclose(p_output);

    // Rename the file
    if (0 != rename(newfilename.local8Bit(), mdata->Filename().local8Bit())
        || 0 != rename(newfilename.ascii(), mdata->Filename().ascii()))
    {
        // Can't overwrite the file for some reason.
        remove(newfilename.local8Bit()) && remove(newfilename.ascii());
        return false;
    }

    return true;
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOOggVorbisComment::read(QString filename)
{
    QString artist = "", compilation_artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;
    bool compilation = false;

    FILE* p_input = NULL;
    p_input = fopen(filename.local8Bit(), "rb");
    if (!p_input)
        p_input = fopen(filename.ascii(), "rb");
    
    if (p_input)
    {
        OggVorbis_File vf;
        vorbis_comment *comment = NULL;

        if (0 != ov_open(p_input, &vf, NULL, 0))
        {
            fclose(p_input);
        }
        else
        {
            comment = ov_comment(&vf, -1);
        
            //
            //  Try and fill metadata info from tags in the ogg file
            //
        
            artist = getComment(comment, MYTH_VORBISCOMMENT_ARTIST);
            compilation_artist = getComment(comment, MYTH_VORBISCOMMENT_COMPILATIONARTIST);
            album = getComment(comment, MYTH_VORBISCOMMENT_ALBUM);
            title = getComment(comment, MYTH_VORBISCOMMENT_TITLE);
            genre = getComment(comment, MYTH_VORBISCOMMENT_GENRE);
            tracknum = atoi(getComment(comment, MYTH_VORBISCOMMENT_TRACK).ascii()); 
            year = atoi(getComment(comment, MYTH_VORBISCOMMENT_DATE).ascii());
            
            QString tmp = getComment(comment, MYTH_VORBISCOMMENT_MUSICBRAINZ_ALBUMARTISTID);
            compilation = (MYTH_MUSICBRAINZ_ALBUMARTIST_UUID == tmp);

            length = getTrackLength(&vf);

            // Do not call fclose as ov_clear takes care of things for us.
            ov_clear(&vf);
        }
    }
    
    //
    //  If the user has elected to get metadata from file names or if the
    //  above did not find a title tag
    //
    
    if (title.isEmpty())
    {
        year = 0;
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
        
    Metadata *retdata = new Metadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length);

    retdata->setCompilation(compilation);
    
    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in milliseconds) of an ov_open'ed file
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
int MetaIOOggVorbisComment::getTrackLength(OggVorbis_File* pVf)
{
    if (!pVf)
        return 0;

    return (int)(ov_time_total(pVf, -1) * 1000);
}


//==========================================================================
/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
int MetaIOOggVorbisComment::getTrackLength(QString filename)
{
    FILE* p_input = NULL;
    p_input = fopen(filename.local8Bit(), "rb");
    if (!p_input)
        p_input = fopen(filename.ascii(), "rb");
    
    if (!p_input)
        return 0;

    OggVorbis_File vf;
    
    if (ov_open(p_input, &vf, NULL, 0))
    {
        fclose(p_input);
        return 0;
    }
    
    int rv = getTrackLength(&vf);
    
    // Do not call fclose as ov_clear takes care of things for us.
    ov_clear(&vf);
    
    return rv;
}


//==========================================================================
/*!
 * \brief Function to return an individual comment from an OggVorbis comment
 *
 * \param pComment Pointer to a vorbis_comment
 * \param pLabel The label of the comment you want
 * \returns QString containing the contents of the comment you want
 */
QString MetaIOOggVorbisComment::getComment(vorbis_comment* pComment, 
                                           const char* pLabel)
{
    char *tag;
    QString retstr;
    
    if (pComment 
        && NULL != (tag = vorbis_comment_query(pComment, (char *)pLabel, 0)))
        retstr = QString::fromUtf8(tag);
    else
        retstr = "";

    return retstr;
}

