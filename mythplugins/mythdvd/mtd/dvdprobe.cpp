/*
	dvdprobe.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    implementation for dvd probing (libdvdread)
*/

#include <iostream>
using namespace std;
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <unistd.h>
#include "dvdprobe.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

DVDSubTitle::DVDSubTitle(int subtitle_id, const QString &a_language)
{
    id = subtitle_id; 
    language = a_language.utf8();
    if(language.length() < 1)
    {
        language = "unknown";
    }    
}

DVDAudio::DVDAudio()
{
    audio_format = "";
    multichannel_extension = false;
    language = "";
    application_mode = "";
    quantization = "";
    sample_frequency = "";
    channels = 0;
    language_extension = "";

}

void DVDAudio::printYourself()
{
    //
    //  Debugging
    //
    
    cout << "    Audio track: " << language 
         << " " << audio_format
         << " " << sample_frequency
         << " " << channels
         << "Ch" << endl;
}

QString DVDAudio::getAudioString()
{
    QString a_string = QString("%1 %2 %3Ch").arg(language).arg(audio_format).arg(channels);
    return a_string;
}

void DVDAudio::fill(audio_attr_t *audio_attributes)
{
    char a_string[1025];

    //
    //  this just travels down the audio_attributes stuct
    //  (defined in libdvdread) and ticks off the right
    //  information
    //

    switch(audio_attributes->audio_format)
    {
        case 0:
            audio_format="ac3";
            break;
        case 1:
            audio_format="oops";
            break;
        case 2:
            audio_format="mpeg1";
            break;
        case 3:
            audio_format="mpeg2ext";
            break;
        case 4:
            audio_format="lpcm";
            break;
        case 5:
            audio_format="oops";
            break;
        case 6:
            audio_format="dts";
            break;
        default:
            audio_format="oops";
    }
    
    if(audio_attributes->multichannel_extension)
    {
        multichannel_extension = true;
    }
    
    switch(audio_attributes->lang_type)
    {
        case 0:
            language="nl";
            break;
        case 1:
            snprintf(a_string, 1024, "%c%c", 
                     audio_attributes->lang_code>>8,
                     audio_attributes->lang_code & 0xff);
            language = a_string;
            break;
        default:
            language = "oops";
    }
    
    switch(audio_attributes->application_mode)
    {
        case 0:
            application_mode = "unknown";
            break;
        case 1:
            application_mode = "karaoke";
            break;
        case 2:
            application_mode = "surround sound";
            break;
        default:
            application_mode = "oops";
    }
    
    switch(audio_attributes->quantization)
    {
        case 0:
            quantization = "16bit";
            break;
        case 1:
            quantization = "20bit";
            break;
        case 2:
            quantization = "24bit";
            break;
        case 3:
            quantization = "drc";
            break;
        default:
            quantization = "oops";
    }

    switch(audio_attributes->sample_frequency)
    {
        case 0:
            sample_frequency = "48kHz";
            break;
        default:
            sample_frequency = "oops";
    }    
    
    channels = audio_attributes->channels + 1;

    switch(audio_attributes->lang_extension)
    {
        case 0:
            language_extension = "Unknown";
            break;
        case 1:
            language_extension = "Normal Caption";
            break;
        case 2:
            language_extension = "Audio for Visually Impaired";
            break;
        case 3:
            language_extension = "Directors 1";
            break;
        case 4:
            language_extension = "Directors 2 or Music Score";
            break;
        default:
            language_extension = "oops";
    }
}

DVDAudio::~DVDAudio()
{
}

/*
---------------------------------------------------------------------
*/

DVDTitle::DVDTitle()
{
    numb_chapters = 0;
    numb_angles = 0;
    track_number = 0;
    hours = 0;
    minutes = 0;
    seconds = 0;
    frame_rate = 0.0;
    hsize = 0;
    vsize = 0;
    audio_tracks.clear();
    audio_tracks.setAutoDelete(true);
    subtitles.clear();
    subtitles.setAutoDelete(true);

    hsize = 0;
    vsize = 0;
    fr_code = 0;
    ar_numerator = 0;
    ar_denominator = 0;
    aspect_ratio = "";

    letterbox = false;
    video_format = "unkown";
    
    dvdinput_id = 0;
}

void DVDTitle::setTime(uint h, uint m, uint s, double fr)
{
    hours = h;
    minutes = m;
    seconds = s;
    frame_rate = fr;
    
    //
    //  These are transocde frame rate codes
    //
    
    if(fr > 23.0 && fr < 24.0)
    {
        fr_code = 1;
    }
    else if(fr == 24.0)
    {
        fr_code = 2;
    }
    else if(fr == 25.0)
    {
        fr_code = 3;
    }
    else if(fr >  20.0 && fr < 30.0)
    {
        fr_code = 4;
    }
    else if(fr == 30.0)
    {
        fr_code = 5;
    }
    else if(fr == 50.0)
    {
        fr_code = 6;
    }
    else if(fr > 59.0 && fr < 60.0)
    {
        fr_code = 7;
    }
    else if(fr == 60.0)
    {
        fr_code = 8;
    }
    else if(fr == 1.0)
    {
        fr_code = 9;
    }
    else if(fr == 5.0)
    {
        fr_code = 10;
    }
    else if(fr == 10.0)
    {
        fr_code = 11;
    }
    else if(fr == 12.0)
    {
        fr_code = 12;
    }
    else if(fr == 15.0)
    {
        fr_code = 13;
    }
    else
    {
        fr_code = 0;
        cerr << "dvdprobe.o: Could not find a frame rate code given a frame rate of " << fr << endl ;
    }
    
    
}

void DVDTitle::setAR(uint n, uint d, const QString &ar)
{
    ar_numerator = n;
    ar_denominator = d;
    aspect_ratio = ar;
}

uint DVDTitle::getPlayLength()
{
    return seconds + (60 * minutes) + (60 * 60 * hours);
}

QString DVDTitle::getTimeString()
{
    QString a_string;
    a_string.sprintf("%d:%02d:%02d", hours, minutes, seconds);
    return a_string;
}


void DVDTitle::printYourself()
{
    //
    //  Debugging
    //

    cout << "Track " << track_number
         << " has " << numb_chapters
         << " chapters, " << numb_angles
         << " angles, and runs for " << hours
         << " hour(s) " << minutes
         << " minute(s) and " << seconds
         << " seconds at " << frame_rate
         << " fps." << endl;
    if(audio_tracks.count() > 0)
    {
        for(uint i = 0; i < audio_tracks.count(); ++i)
        {
            audio_tracks.at(i)->printYourself();
        }
    }
    else
    {
        cout << "    No Audio Tracks" << endl;
    }
}

void DVDTitle::addAudio(DVDAudio *new_audio_track)
{
    audio_tracks.append(new_audio_track);
}

void DVDTitle::addSubTitle(DVDSubTitle *new_subtitle)
{
    //
    //  Check if this language already exists
    //  (which happens a lot)
    //
    
    int lang_count = 0;
    for(uint i = 0; i < subtitles.count(); ++i)
    {
        if(subtitles.at(i)->getLanguage() == 
           new_subtitle->getLanguage())
        {
            ++lang_count;
        }
    }
    if(lang_count == 0)
    {
        new_subtitle->setName(QString("<%1>").arg(new_subtitle->getLanguage()));
    }
    else
    {
        ++lang_count;
        new_subtitle->setName(QString("<%1> (%2)").arg(new_subtitle->getLanguage()).arg(lang_count));
    }
    subtitles.append(new_subtitle);
}

void DVDTitle::determineInputID()
{
    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.prepare("SELECT intid FROM dvdinput WHERE "
                              "hsize = :HSIZE and "
                              "vsize = :VSIZE and "
                              "ar_num = :ARNUM and "
                              "ar_denom = :ARDENOM and "
                              "fr_code = :FRCODE and "
                              "letterbox = :LETTERBOX and "
                              "v_format = :VFORMAT;");
    a_query.bindValue(":HSIZE", hsize);
    a_query.bindValue(":VSIZE", vsize);
    a_query.bindValue(":ARNUM", ar_numerator);
    a_query.bindValue(":ARDENOM", ar_denominator);
    a_query.bindValue(":FRCODE", fr_code);
    a_query.bindValue(":LETTERBOX", letterbox);
    a_query.bindValue(":VFORMAT", video_format);
    
    if(a_query.exec() && a_query.isActive() && a_query.size() > 0)
    {
        a_query.next();
        dvdinput_id = a_query.value(0).toInt();
    }
    else
    {
        cerr << "dvdprobe.o: You have a title on your dvd in a format myth doesn't understand." << endl;
        cerr << "dvdprobe.o: Either that, or you haven't installed the dvdinput table." << endl;
        cerr << "dvdprobe.o: You probably want to report this to a mailing list or something: " << endl;
        cerr << "                  height = " << hsize << endl;
        cerr << "                   width = " << vsize << endl;
        cerr << "            aspect ratio = " << aspect_ratio << " (" << ar_numerator << "/"<< ar_denominator << ")" << endl;
        cerr << "              frame rate = " << frame_rate << endl;
        cerr << "                 fr_code = " << fr_code << endl;
        if(letterbox)
        {
            cerr << "             letterboxed = true " << endl;
        }
        else
        {
            cerr << "             letterboxed = false " << endl;
        }
        cerr << "                  format = " << video_format << endl;
    }    
}

DVDTitle::~DVDTitle()
{
    audio_tracks.clear();
    subtitles.clear();
}

/*
---------------------------------------------------------------------
*/


DVDProbe::DVDProbe(const QString &dvd_device)
{
    //
    //  This object just figures out what's on a disc
    //  and tells whomever asks about it.
    //
   
    device = dvd_device;
    dvd = NULL;
    titles.setAutoDelete(true);
    titles.clear();
    volume_name = QObject::tr("Unknown");
    first_time = true;
}

void DVDProbe::wipeClean()
{
    titles.clear();
    volume_name = QObject::tr("Unknown");
}

bool DVDProbe::probe()
{
    //
    //  Before touching libdvdread stuff
    //  (below), see if there's actually
    //  a drive with media in it
    //
    
    struct stat fileinfo;
    
    int ret = stat(device, &fileinfo);
    if(ret < 0)
    {
        //
        //  Can't even stat the device, it probably
        //  doesn't exist. Silly user
        //
        wipeClean();
        return false;
    }

    //
    //  I have no idea if the following code block
    //  is anywhere close to the "right way" of doing
    //  this, but it seems to work.
    //

    int drive_handle = open(device, O_RDONLY | O_NONBLOCK);
    int status = ioctl(drive_handle, CDROM_DRIVE_STATUS, NULL);
    if(status < 4)
    {
        //
        // -1  error (no disc)
        //  1 = no info
        //  2 = tray open
        //  3 = not ready
        //
        wipeClean();
        close(drive_handle);
        return false;
        
    }

    status = ioctl(drive_handle, CDROM_MEDIA_CHANGED, NULL);
    close(drive_handle);

    if(!status)
    {
        //
        //  the disc has not changed. but if this is our
        //  first time running, we still need to check it
        //
        
        if(!first_time)
        {
            //
            //  Return whatever we returned before
            //

            if(titles.count() > 0)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    //
    //  Try to open the disc
    //  (as far libdvdread is concerned, the argument
    //  could be a path, file, whatever).
    //
    
    first_time = false;
    wipeClean();
    dvd = DVDOpen(device);
    if(dvd)
    {
        //
        //  Grab the title of this DVD
        //
        
        int arbitrary = 1024;
        char volume_name_buffr[arbitrary + 1];
        unsigned char set_name[arbitrary + 1];
        
        if(DVDUDFVolumeInfo(dvd, volume_name_buffr, arbitrary, set_name, arbitrary) > -1)
        {
            volume_name = volume_name_buffr;
        }
        else
        {
            cerr << "dvdprobe.o: Error getting volume name, setting to \"Unknown\"" << endl;
        }
        
        ifo_handle_t *ifo_file = ifoOpen(dvd, 0);
        if(ifo_file)
        {
            //
            //  Most of this is taken from title_info.c and
            //  ifo_dump.c located in the libdvdread ./src/ 
            //  directory. Some if is also based on dvd_reader.c
            //  in the transcode source.
            //

            tt_srpt_t *title_info = ifo_file->tt_srpt;
            for(int i = 0; i < title_info->nr_of_srpts; ++i )
            {
                DVDTitle *new_title = new DVDTitle();
                new_title->setTrack(i + 1);
                new_title->setChapters((uint) title_info->title[i].nr_of_ptts);
                new_title->setAngles((uint) title_info->title[i].nr_of_angles);

                ifo_handle_t *video_transport_file = NULL;
                
                video_transport_file = ifoOpen(dvd, title_info->title[i].title_set_nr);
                if(!video_transport_file)
                {
                    cerr << "dvdprobe.o: Can't get video transport for track " << i+1 << endl;
                }
                else
                {
                
                    //
                    //  See how simple libdvdread makes it to find the playing time?
                    //
                    //  Why have a playingTime(track_number) function, when you could jump
                    //  through all these hoops:
                    //
                
                    int some_index = title_info->title[i].vts_ttn;
                    int some_other_index = video_transport_file->vts_ptt_srpt->title[some_index - 1].ptt[0].pgcn;
                    pgc_t *current_pgc = video_transport_file->vts_pgcit->pgci_srp[some_other_index - 1].pgc;
                
                    dvd_time_t *libdvdread_playback_time = &current_pgc->playback_time;
                    
                    double framerate = 0.0;
                    
                    switch((libdvdread_playback_time->frame_u & 0xc0) >> 6)
                    {
                        case 1:
                            framerate = 25.0;  // PAL 
                            break;
                        case 3:
                            framerate = 24000/1001.0;  // NTSC_FILM
                            break;
                        default:
                            cerr << "dvdprobe.o: For some odd reason (!), I couldn't get a video frame rate" << endl;
                            break;
                    }
                    
                    //
                    //  Somebody who knows something about how to program a computer
                    //  should clean this up
                    //
                    
                    char bloody_stupid_libdvdread[1024 + 1];

                    snprintf(bloody_stupid_libdvdread, 1024, "%02x", libdvdread_playback_time->hour);
                    QString hour = bloody_stupid_libdvdread;
                    snprintf(bloody_stupid_libdvdread, 1024, "%02x", libdvdread_playback_time->minute);
                    QString minute = bloody_stupid_libdvdread;
                    snprintf(bloody_stupid_libdvdread, 1024, "%02x", libdvdread_playback_time->second);
                    QString second = bloody_stupid_libdvdread;

                    new_title->setTime(hour.toUInt(), minute.toUInt(), second.toUInt(),
                                       framerate);
                    
                    vtsi_mat_t *vtsi_mat = NULL;
                    vtsi_mat = video_transport_file->vtsi_mat;
                    if(vtsi_mat)
                    {
                        //
                        //  and now, wave the divining rod over audio bits
                        //
                    
                        for(int j=0; j < vtsi_mat->nr_of_vts_audio_streams; j++)
                        {
                            audio_attr_t *audio_attributes = &vtsi_mat->vts_audio_attr[j];
                            DVDAudio *new_audio = new DVDAudio();
                            new_audio->fill(audio_attributes);
                            new_title->addAudio(new_audio);
                        }

                        //
                        //  determine, with any luck, subtitles
                        //
                        
                        for(int j=0; j < vtsi_mat->nr_of_vts_subp_streams; j++)
                        {
                            subp_attr_t *sub_attributes = &vtsi_mat->vts_subp_attr[j];
                            if( sub_attributes->type           == 0 &&
                                sub_attributes->zero1          == 0 &&
                                sub_attributes->lang_code      == 0 &&
                                sub_attributes->lang_extension == 0 &&
                                sub_attributes->zero2          == 0)
                            {
                                // ignore this sub attribute
                            }
                            else
                            {
                                //
                                //  The Freakin Disney Corporation seems to think it's
                                //  useful to set language codes with weird ass 
                                //  characters. This should probably handle that.
                                //
                                
                                QString zee_language = "";
                                zee_language.sprintf("%c%c", sub_attributes->lang_code>>8, 
                                                             sub_attributes->lang_code & 0xff);
                                DVDSubTitle *new_subtitle = new DVDSubTitle(j, zee_language.utf8());
                                new_title->addSubTitle(new_subtitle);
                                //printf("subtitle %02d=<%c%c> ", j, sub_attributes->lang_code>>8, sub_attributes->lang_code & 0xff);
                                //if(sub_attributes->lang_extension) printf("ext=%d", sub_attributes->lang_extension);
                            }
                        }
                                                
                        //
                        //  Figure out size, aspect ratio, etc.
                        //
                        
                        video_attr_t *video_attributes = &vtsi_mat->vts_video_attr;
                      
                        switch(video_attributes->display_aspect_ratio)
                        {
                            case 0:
                                new_title->setAR(4, 3, "4:3");   
                                break;
                            case 3:
                                new_title->setAR(16, 9, "16:9");
                                break;
                            default:
                                cerr << "dvdprobe.o: couldn't get aspect ratio for a title" << endl;
                        }
                        
                        switch(video_attributes->video_format)
                        {
                            case 0:
                                new_title->setVFormat("ntsc");
                                break;
                            case 1:
                                new_title->setVFormat("pal");
                                break;
                            default:
                                cerr << "dvdprobe.o: Could not get video format for a title" << endl;
                        }
                        
                        if(video_attributes->letterboxed)
                        {
                            new_title->setLBox(true);
                        }
                        
                        uint c_height = 480;
                        if(video_attributes->video_format != 0)
                        {
                            c_height = 576;
                        }
                        switch(video_attributes->picture_size)
                        {
                            case 0:
                                new_title->setSize(720, c_height);
                                break;
                            case 1:
                                new_title->setSize(704, c_height);
                                break;
                            case 2:
                                new_title->setSize(352, c_height);
                                break;
                            case 3:
                                new_title->setSize(352, c_height / 2);
                                break;
                            default:
                                cerr << "dvdprobe.o: Could not determine for video size for a title." << endl ;
                        }
                        ifoClose(video_transport_file);
                    }
                    else
                    {
                        cerr << "Couldn't find any audio or video information for track " << i+1 << endl;
                    }            
                }
                
                                        
                //
                //  Debugging output
                //
                //  new_title->printYourself();        
        
                //
                //  Have the new title figure out it's
                //  appropriate id number vis-a-vis the
                //  dvdinput table
                //
                
                new_title->determineInputID();
        
                //
                //  Add this new title to the container
                //

                titles.append(new_title);

            }
            
            ifoClose(ifo_file);
            ifo_file = NULL;
            
            DVDClose(dvd);
            dvd = NULL;
            return true;
        }
        else
        {
        
            DVDClose(dvd);
            dvd = NULL;   
            return false;
        }
    }
    return false;
}

DVDTitle* DVDProbe::getTitle(uint which_one)
{

    DVDTitle *iter;
    for(iter = titles.first(); iter; iter = titles.next())
    {
        if(iter->getTrack() == which_one)
        {
            return iter;
        }
    }
    return NULL;

}



DVDProbe::~DVDProbe()
{
    if(dvd)
    {
        DVDClose(dvd);
    }
    wipeClean();
}

