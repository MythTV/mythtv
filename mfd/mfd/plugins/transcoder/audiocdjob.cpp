/*
	audiocdjob.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Class for importing audio cds
*/

#include <sys/stat.h>   //  for
#include <sys/types.h>  //  mkdir()

#include <qregexp.h>

#include "settings.h"
#include "audiocdjob.h"
#include "transcoder.h"
#include "cdinput.h"
#include "encoder.h"
#include "vorbisencoder.h"

AudioCdJob::AudioCdJob(
                        Transcoder                 *owner,
                        int                         l_job_id,
                        QString                     l_scratch_dir_string,
                        QString                     l_destination_dir_string,
                        MetadataServer             *l_metadata_server,
                        int                         l_container_id,
                        MfdTranscoderAudioCodec     l_codec,
                        MfdTranscoderAudioQuality   l_quality
                      )
           :ImportJob(
                        owner, 
                        l_job_id, 
                        l_scratch_dir_string, 
                        l_destination_dir_string,
                        l_metadata_server,
                        l_container_id
                     )
{
    codec = l_codec;
    quality = l_quality;
}         

void AudioCdJob::run()
{
    //
    //  set our priority
    //
    
    int nice_level = mfdContext->getNumSetting("plugin-audiocdjob-nice", 18);
    nice(nice_level);
    
    //
    //  Setup our scratch space
    //
    
    if(! setupScratch() )
    {
        parent->wakeUp();
        exit();
    }

    //
    //  get the collection in question
    //
    
    bool all_is_well = false;
    QValueList<int> metadata_list;
    QString collection_name;
    metadata_server->lockMetadata();
        MetadataContainer *which_one = metadata_server->getMetadataContainer(container_id);
        if(which_one)
        {
            all_is_well = true;
            if (! which_one->isAudio())
            {
                all_is_well = false;
                warning("asked to rip cd, but collection is not of type audio");
            }
            else if (!which_one->isRipable())
            {
                all_is_well = false;
                warning("asked to rip cd, but collection is not ripable");
            }
            else if (which_one->isBeingRipped())
            {
                all_is_well = false;
                warning("asked to rip cd, but collection is already being ripped");
            }
            else
            {
                collection_name = which_one->getName();

                //
                //  Get a list of int's that represent all the tracks
                //

                QIntDict<Metadata> *all_the_metadata = which_one->getMetadata();
                QIntDictIterator<Metadata> it( *all_the_metadata );
                for ( ; it.current(); ++it )
                {
                    metadata_list.push_back(it.current()->getId());
                }
            }
        }
        else
        {
            all_is_well = false;
            warning(QString("No metadata container with an id of %i. Nothing to rip")
                            .arg(container_id));
        }
    metadata_server->unlockMetadata();

    if(!all_is_well)
    {
        exit();
    }

    //
    //  Ask the metadata server to mark the collection as being ripped
    //
    
    metadata_server->markCollectionAsBeingRipped(container_id, true);

    log(QString("starting to rip %1 tracks from \"%2\"")
                .arg(metadata_list.count())
                .arg(collection_name), 3);


    //
    //  Iterate over the tracks, ripping and encoding each in turn
    //


    QUrl track_url;
    bool found_track;
    QValueList<int>::Iterator iter;
    for ( iter = metadata_list.begin(); iter != metadata_list.end(); ++iter )
    {
        if(!keep_going)
        {   
            break;
        }
        found_track= false;
        metadata_server->lockMetadata();
            Metadata *metadata = metadata_server->getMetadataByContainerAndId(
                                                                                container_id,
                                                                                (*iter)
                                                                             );
            if(metadata)
            {
                found_track = true;
                track_url = metadata->getUrl();
            }
            else
            {
                warning("during audio cd rip, a track seems to have disappeared");
            }
            AudioMetadata *cast_metadata = (AudioMetadata *) metadata;
            AudioMetadata *private_copy_of_metadata = new AudioMetadata(*cast_metadata);
        metadata_server->unlockMetadata();

        //
        //  Check that a file with the exact name this file will end up
        //  having does not already exist (if it does, skip this file on the
        //  theory that it has already been ripped).
        //
        
        destination_dir_string = QDir::cleanDirPath(destination_dir_string);
        if(!destination_dir_string.endsWith("/"))
        {
            destination_dir_string.append("/");
        }

        QString destination_filename = destination_dir_string;
        handleFileTokens(destination_filename, private_copy_of_metadata);
        if (quality == MFD_TRANSCODER_AUDIO_QUALITY_PERFECT)
        {
            destination_filename.append(".flac");
        }
        else
        {
            if (codec == MFD_TRANSCODER_AUDIO_CODEC_OGG)
            {
                destination_filename.append(".ogg");
            }
            else if (codec == MFD_TRANSCODER_AUDIO_CODEC_MP3)
            {
                destination_filename.append(".mp3");
            }
            else
            {
                warning("unknown or illogical combination of audio quality and codec");
                destination_filename.append(".confused"); 
            }
        }

        QFileInfo exists_checker(destination_filename);
        if(exists_checker.exists())
        {
            log(QString("skipping rip/encode of track number %1, as it "
                        "seems to already exist")
                        .arg((*iter)), 3);
        }
        else
        {
            ripAndEncodeTrack(track_url, destination_filename, private_copy_of_metadata);
        }
        
        delete private_copy_of_metadata;
    }

    //
    //  All done, this collection is now longer being ripped
    //

    metadata_server->markCollectionAsBeingRipped(container_id, false);

    //
    //  If the user has set it, eject the CD
    //
    
    bool EjectCD = mfdContext->GetNumSetting("EjectCDAfterRipping",1);
    if (EjectCD) 
    {
        ejectCd(track_url.dirPath());	
    }


    //
    //  Nudge our parent, that they might notice our demise
    //

    parent->wakeUp();
}

bool AudioCdJob::ripAndEncodeTrack(QUrl track_url, QString destination_filename, AudioMetadata *metadata)
{
    QCString destination = destination_filename.local8Bit();
    QCString temp_filename = destination;
    temp_filename.append(".nosweep");


    log(QString("attempting to rip this url: \"%1\"").arg(track_url), 3);

    CdInput   *source = NULL;
    Encoder   *encoder = NULL;

    if (!track_url.isValid())
    {
        warning(QString("passed an invalid url: %1").arg(track_url));
        return false;
    }

    if (track_url.protocol() == "cd")
    {
        source = new CdInput(track_url);
    }
    else if (track_url.protocol() == "file")
    {
        warning("can't do file:// ripping yet");
    }
    else if (track_url.protocol() == "daap")
    {
        warning("can't do daap:// ripping yet");
    }
    else
    {
        warning(QString("unsupported protocol in rip source: \"%1\"")
                        .arg(track_url.protocol()));
    }
    
    if( source == NULL)
    {
        return false;
    }

    if(! source->open())
    {
        warning(QString("could not open source for rip: \"%1\"")
                        .arg(track_url.toString()));
        return false;
        
    }


    encoder = new VorbisEncoder(temp_filename, 3, metadata);

    bool success = true;
    bool done = false;
    char *buffer = new char[4097];
    int total_read = 0;
    int file_size = source->size();
    int amount_read = 0;

    while(!done)
    {
        if(!keep_going)
        {
            success = false;
            done = true;
        } 
        else
        {
            amount_read = source->readBlock(buffer, 4096);
            if(amount_read > 0)
            {
                total_read += amount_read;
                if(encoder->addSamples((int16_t *)buffer, amount_read) != 0)
                {
                    warning("error writing to destination file during encoding");
                    
                    success = false;
                    done = true;
                }
                if(total_read == file_size)
                {
                    done = true;
                }
            }
            else
            {
                warning("error reading from source during CD rip");
                success = false;
                done = true;
            }
        }
    }
  
    
    if (source)
    {
        source->close();
        delete source;
        source = NULL;
    }
    if (encoder)
    {
        delete encoder;
        encoder = NULL;
    }

    if(! success)
    {
        tryToCleanUp(temp_filename);
        return false;
    }

    //
    //  Atomically swap the encoded file for something "sweepable" (ie. change the extension)
    //

    int result = rename((const char *)temp_filename, (const char *)destination);
    
    if(result != 0)
    {
        warning(QString("error moving \"%1\" to \"%2\"")
                        .arg(temp_filename)
                        .arg(destination)); 
        
        tryToCleanUp(temp_filename);
        return false;
    }
    else
    {
        log(QString("moved \"%1\" to \"%2\"")
                    .arg(temp_filename)
                    .arg(destination), 5);
    }

    return true;
}

void AudioCdJob::tryToCleanUp(QCString filename)
{
    //
    //  try to erase this file
    //
    
    QFile the_file(filename);
    if(the_file.exists())
    {
        if(!the_file.remove())
        {
            warning(QString("unable to delete a file I was told to: \"%1\"")
                            .arg(filename));
        }
    }
    else
    {
        warning(QString("asked to clean up a file that does not exist: \"%1\"")
                        .arg(filename));
    }
}

void AudioCdJob::handleFileTokens(QString &filename, AudioMetadata *track)
{
    QString original = filename;

    QString fntempl = mfdContext->GetSetting("FilenameTemplate");
    bool no_ws = mfdContext->GetNumSetting("NoWhitespace", 0);

    QRegExp rx_ws("\\s{1,}");
    QRegExp rx("(?:\\s?)(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)(?:\\s?)/");

    int i = 0;
    while (i >= 0)
    {
        i = rx.search(fntempl, i);
        if (i >= 0)
        {
            i += rx.matchedLength();
            if ((rx.capturedTexts()[1] == "GENRE") && (track->getGenre() != ""))
                filename += fixFileToken(track->getGenre()) + "/";
            if ((rx.capturedTexts()[1] == "ARTIST") && (track->getArtist() != ""))
                filename += fixFileToken(track->getArtist()) + "/";
            if ((rx.capturedTexts()[1] == "ALBUM") && (track->getAlbum() != ""))
                filename += fixFileToken(track->getAlbum()) + "/";
            if ((rx.capturedTexts()[1] == "TRACK") && (track->getTrack() >= 0))
                filename += fixFileToken(QString::number(track->getTrack(), 10)) + "/";
            if ((rx.capturedTexts()[1] == "TITLE") && (track->getTitle() != ""))
                filename += fixFileToken(track->getTitle()) + "/";
            if ((rx.capturedTexts()[1] == "YEAR") && (track->getYear() >= 0))
                filename += fixFileToken(QString::number(track->getYear(), 10)) + "/";

            if (no_ws)
            {
                filename.replace(rx_ws, "_");
            }

            mkdir(filename, 0775);
        }
    }

    // remove the dir part and other cruft
    fntempl.replace(QRegExp("(.*/)|\\s*"), "");
    QString tagsep = mfdContext->GetSetting("TagSeparator");
    QStringList tokens = QStringList::split("-", fntempl);
    QStringList fileparts;
    QString filepart;

    for (unsigned i = 0; i < tokens.size(); i++)
    {
        if ((tokens[i] == "GENRE") && (track->getGenre() != ""))
            fileparts += track->getGenre();
        else if ((tokens[i] == "ARTIST") && (track->getArtist() != ""))
            fileparts += track->getArtist();
        else if ((tokens[i] == "ALBUM") && (track->getAlbum() != ""))
            fileparts += track->getAlbum();
        else if ((tokens[i] == "TRACK") && (track->getTrack() >= 0))
        {
            QString tempstr = QString::number(track->getTrack(), 10);
            if (track->getTrack() < 10)
                tempstr.prepend('0');
            fileparts += tempstr;
        }
        else if ((tokens[i] == "TITLE") && (track->getTitle() != ""))
            fileparts += track->getTitle();
        else if ((tokens[i] == "YEAR") && (track->getYear() >= 0))
            fileparts += QString::number(track->getYear(), 10);
    }

    filepart = fileparts.join( tagsep );
    filename += fixFileToken(filepart);
    
    if (filename == original || filename.length() > FILENAME_MAX)
    {
        QString tempstr = QString::number(track->getTrack(), 10);
        tempstr += " - " + track->getTitle();
        filename += fixFileToken(tempstr);
        warning("something wonky with creating file names");
    }

    if (no_ws)
    {
        filename.replace(rx_ws, "_");
    }
}

inline QString AudioCdJob::fixFileToken(QString token)
{
    token.replace(QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}


void AudioCdJob::ejectCd(QString cddev)
{
    int cdrom_fd;
    cdrom_fd = cd_init_device((char*)cddev.ascii());
    if (cdrom_fd != -1)
    {
        if (cd_eject(cdrom_fd) == -1) 
        {
            warning("eject cd setting is on, but cd_eject() command failed");
            perror("Failed on cd_eject");
        }
        cd_finish(cdrom_fd);              
    } 
    else  
    {
        warning("could not descriptor to cd device for eject");
    }
}



AudioCdJob::~AudioCdJob()
{
}
