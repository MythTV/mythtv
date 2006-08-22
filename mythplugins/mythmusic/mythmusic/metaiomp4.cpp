#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

using namespace std;

#define USE_TAGGING
#include "metaiomp4.h"
#include "metadata.h"
#include <mp4ff.h>
#include <faad.h>
#include <mythtv/mythcontext.h>


MetaIOMP4::MetaIOMP4(void)
    : MetaIO(".mp4")
{
}

MetaIOMP4::~MetaIOMP4(void)
{
}

//
//  Extern C callbacks for metadata reading 
//


uint32_t md_read_callback(void *user_data, void *buffer, uint32_t length)
{
  mp4callback_data_t *file_data = (mp4callback_data_t*)user_data;
  return fread(buffer, 1, length, file_data->file);
}

uint32_t md_write_callback(void *user_data, void *buffer, uint32_t length)
{
  mp4callback_data_t *file_data = (mp4callback_data_t*)user_data;
  return fwrite(buffer, 1, length, file_data->file);
}

uint32_t md_truncate_callback(void *user_data) 
{
  mp4callback_data_t *file_data = (mp4callback_data_t*)user_data;
  ftruncate(file_data->fd, ftello(file_data->file));
  return 0;
}

uint32_t md_seek_callback(void *user_data, uint64_t position)
{
  mp4callback_data_t *file_data = (mp4callback_data_t*)user_data;
  return fseek(file_data->file, position, SEEK_SET);
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
bool MetaIOMP4::write(Metadata* mdata, bool exclusive)
{
    exclusive = exclusive; // -Wall annoyance
    
    // Sanity check.
    if (!mdata)
        return false;

    mp4callback_data_t callback_data;

    callback_data.fd = open(mdata->Filename().local8Bit(), O_RDWR);
    if (callback_data.fd < 0) {
        return false;
    }
     
    callback_data.file = fdopen(callback_data.fd, "r+");
    if (!callback_data.file)
    {
        close(callback_data.fd);
        return false;
    }

    //
    //  Create the callback structure
    //

    mp4ff_callback_t *mp4_cb = (mp4ff_callback_t*) malloc(sizeof(mp4ff_callback_t));
    if (!mp4_cb) {
      close(callback_data.fd);
      fclose(callback_data.file);
      return false;
    }
    mp4_cb->read = md_read_callback;
    mp4_cb->seek = md_seek_callback;
    mp4_cb->write = md_write_callback;
    mp4_cb->truncate = md_truncate_callback;
    mp4_cb->user_data = &callback_data;

    mp4ff_metadata_t *mp4ff_mdata = (mp4ff_metadata_t*)malloc(sizeof(mp4ff_metadata_t));
    if (!mp4ff_mdata) {
      free(mp4_cb);
      close(callback_data.fd);
      fclose(callback_data.file);
      return false;
    }
    mp4ff_mdata->tags = (mp4ff_tag_t*)malloc(7 * sizeof(mp4ff_tag_t));
    if (!mp4ff_mdata) {
      free(mp4_cb);
      free(mp4ff_mdata);
      close(callback_data.fd);
      fclose(callback_data.file);
      return false;
    }

    //
    //  Open the mp4 input file  
    //                   

    mp4ff_t *mp4_ifile = mp4ff_open_read(mp4_cb);
    if (!mp4_ifile)
    {
        free(mp4_cb);
        free(mp4ff_mdata);
        close(callback_data.fd);
        fclose(callback_data.file);
        return false;
    }

    mp4ff_mdata->tags[0].item = "artist";
    mp4ff_mdata->tags[0].value = (char*)mdata->Artist().ascii();

    mp4ff_mdata->tags[1].item = "album";
    mp4ff_mdata->tags[1].value = (char*)mdata->Album().ascii();

    mp4ff_mdata->tags[2].item = "title";
    mp4ff_mdata->tags[2].value = (char*)mdata->Title().ascii();

    mp4ff_mdata->tags[3].item = "genre";
    mp4ff_mdata->tags[3].value = (char*)mdata->Genre().ascii();

    mp4ff_mdata->tags[4].item = "date";
    mp4ff_mdata->tags[4].value = (char*)malloc(128);
    snprintf(mp4ff_mdata->tags[4].value, 128, "%d", mdata->Year());

    mp4ff_mdata->tags[5].item = "track";
    mp4ff_mdata->tags[5].value = (char*)malloc(128);
    snprintf(mp4ff_mdata->tags[5].value, 128, "%d", mdata->Track());

    mp4ff_mdata->tags[6].item = "compilation";
    mp4ff_mdata->tags[6].value = (char*)malloc(2);
    mp4ff_mdata->tags[6].value[0] = mdata->Compilation() ? 1 : 0;
    mp4ff_mdata->tags[6].value[1] = 0;

    mp4ff_mdata->count = 7;

    mp4ff_meta_update(mp4_cb, mp4ff_mdata);

    mp4ff_close(mp4_ifile);
    free(mp4_cb);
    close(callback_data.fd);
    fclose(callback_data.file);
    free(mp4ff_mdata->tags[4].value);
    free(mp4ff_mdata->tags[5].value);
    free(mp4ff_mdata->tags[6].value);
    free(mp4ff_mdata->tags);
    free(mp4ff_mdata);

    return true;
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOMP4::read(QString filename)
{
    QString artist = "", album = "", title = "", genre = "";
    QString writer = "", comment = "";
    int year = 0, tracknum = 0, length = 0;
    bool compilation = false;

    mp4callback_data_t callback_data;
    callback_data.fd = 0;
    callback_data.file = fopen(filename.local8Bit(), "r");
    if (!callback_data.file)
    {
        return NULL;
    }
    

    //
    //  Create the callback structure
    //

    mp4ff_callback_t *mp4_cb = (mp4ff_callback_t*) malloc(sizeof(mp4ff_callback_t));
    mp4_cb->read = md_read_callback;
    mp4_cb->seek = md_seek_callback;
    mp4_cb->user_data = &callback_data;


    //
    //  Open the mp4 input file  
    //                   

    mp4ff_t *mp4_ifile = mp4ff_open_read(mp4_cb);
    if (!mp4_ifile)
    {
        free(mp4_cb);
        fclose(callback_data.file);
        return NULL;
    }

    //
    //  Look for metadata
    //

    char *char_storage = NULL;

    if (mp4ff_meta_get_title(mp4_ifile, &char_storage))
    {
        title = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_artist(mp4_ifile, &char_storage))
    {
        artist = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_writer(mp4_ifile, &char_storage))
    {
        writer = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_album(mp4_ifile, &char_storage))
    {
        album = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_date(mp4_ifile, &char_storage))
    {
        year = QString(char_storage).toUInt();
        free(char_storage);
    }

    if (mp4ff_meta_get_comment(mp4_ifile, &char_storage))
    {
        comment = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_genre(mp4_ifile, &char_storage))
    {
        genre = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if (mp4ff_meta_get_track(mp4_ifile, &char_storage))
    {
        tracknum = QString(char_storage).toUInt();
        free(char_storage);
    }

    if (mp4ff_meta_get_compilation(mp4_ifile, &char_storage))
    {
        compilation = (1 == char_storage[0]);
        free(char_storage);
    }

    //
    //  Find the AAC track inside this mp4 which we need to do to find the
    //  length
    //

    int track_num;
    if ( (track_num = getAACTrack(mp4_ifile)) < 0)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return NULL;
    }

    unsigned char *buffer = NULL;
    uint buffer_size;

    mp4ff_get_decoder_config(
                                mp4_ifile, 
                                track_num, 
                                &buffer, 
                                &buffer_size
                            );    
    
    if (!buffer)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return NULL;
    }
   
    mp4AudioSpecificConfig mp4ASC;
    if (AudioSpecificConfig(buffer, buffer_size, &mp4ASC) < 0)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return NULL;
    }
    
    long samples = mp4ff_num_samples(mp4_ifile, track_num);
    float f = 1024.0;

    if (mp4ASC.sbr_present_flag == 1)
    {
        f = f * 2.0;
    }
    
    float numb_seconds = (float)samples*(float)(f-1.0)/(float)mp4ASC.samplingFrequency;

    length = (int) (numb_seconds * 1000);

    mp4ff_close(mp4_ifile);
    free(mp4_cb);
    fclose(callback_data.file);

    metadataSanityCheck(&artist, &album, &title, &genre);

    Metadata *retdata = new Metadata(filename, 
                                     artist,
                                     compilation ? artist : "",
                                     album, 
                                     title, 
                                     genre,
                                     year, 
                                     tracknum, 
                                     length);
    
    retdata->setCompilation(compilation);

    //retdata->setComposer(writer);
    //retdata->setComment(comment);
    //retdata->setBitrate(mp4ASC.samplingFrequency);

    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOMP4::getTrackLength(QString filename)
{

    mp4callback_data_t callback_data;
    callback_data.fd = 0;
    callback_data.file = fopen(filename.local8Bit(), "r");
    if (!callback_data.file)
    {
        return 0;
    }

    //
    //  Create the callback structure
    //

    mp4ff_callback_t *mp4_cb = (mp4ff_callback_t*) malloc(sizeof(mp4ff_callback_t));
    mp4_cb->read = md_read_callback;
    mp4_cb->seek = md_seek_callback;
    mp4_cb->user_data = &callback_data;


    //
    //  Open the mp4 input file  
    //                   

    mp4ff_t *mp4_ifile = mp4ff_open_read(mp4_cb);
    if (!mp4_ifile)
    {
        free(mp4_cb);
        fclose(callback_data.file);
        return 0;
    }

    //
    //  Find the AAC track inside this mp4 which we need to do to find the
    //  length
    //

    int track_num;
    if ( (track_num = getAACTrack(mp4_ifile)) < 0)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return 0;
    }

    unsigned char *buffer = NULL;
    uint buffer_size;

    mp4ff_get_decoder_config(
                                mp4_ifile, 
                                track_num, 
                                &buffer, 
                                &buffer_size
                            );    
    
    if (!buffer)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return 0;
    }
   

    mp4AudioSpecificConfig mp4ASC;
    if (AudioSpecificConfig(buffer, buffer_size, &mp4ASC) < 0)
    {
        mp4ff_close(mp4_ifile);
        free(mp4_cb);
        fclose(callback_data.file);
        return 0;
    }
    
    long samples = mp4ff_num_samples(mp4_ifile, track_num);
    float f = 1024.0;

    if (mp4ASC.sbr_present_flag == 1)
    {
        f = f * 2.0;
    }
    
    float numb_seconds = (float)samples*(float)(f-1.0)/(float)mp4ASC.samplingFrequency;

    int seconds = (int) (numb_seconds * 1000);

    mp4ff_close(mp4_ifile);
    free(mp4_cb);
    fclose(callback_data.file);

    return seconds;
}


int MetaIOMP4::getAACTrack(mp4ff_t *infile)
{
        //
        //  Find an AAC track inside an mp4 container
        //

        int i, rc;
        int numTracks = mp4ff_total_tracks(infile);

        for (i = 0; i < numTracks; i++)
        {
                unsigned char *buff = NULL;
                uint buff_size = 0;
                mp4AudioSpecificConfig mp4ASC;

                mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

                if (buff)
                {
                    rc = AudioSpecificConfig(buff, buff_size, &mp4ASC);
                    free(buff);

                    if (rc < 0)
                        continue;
                    return i;
                }
         }

         //
         //  No AAC tracks
         // 

         return -1;
}


//==========================================================================
void MetaIOMP4::metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre)
{
    if (artist->length() < 1)
    {
        artist->append("Unknown Artist");
    }
    
    if (album->length() < 1)
    {
        album->append("Unknown Album");
    }
    
    if (title->length() < 1)
    {
        title->append("Unknown Title");
    }
    
    if (genre->length() < 1)
    {
        genre->append("Unknown Genre");
    }
    
}


