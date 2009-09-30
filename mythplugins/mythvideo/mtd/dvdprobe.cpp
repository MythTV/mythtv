/*
    dvdprobe.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    implementation for dvd probing (libdvdread)
*/

#include <sys/types.h>
#include <sys/stat.h>
#if defined(__linux__) || defined(__FreeBSD__)
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include <cmath>
#include <climits> // for CDSL_CURRENT which is currently INT_MAX

// Qt
#include <QFile>

#include "dvdprobe.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

namespace
{
    // decode a dvd_time_t time value
    uint decdt(uint8_t c)
    {
        return 10 * (c >> 4) + (c & 0xF);
    }

    bool fcomp(float lhs, float rhs, float ep = 0.0001)
    {
        return std::fabs((lhs - rhs) / rhs) <= ep;
    }

    QString lctola(uint16_t lang_code)
    {
        return QString("%1%2").arg(char(lang_code >> 8))
                .arg(char(lang_code & 0xff));
    }
}

DVDSubTitle::DVDSubTitle(int subtitle_id, const QString &a_language) :
    id(subtitle_id), language(a_language.toUtf8())
{
    if (language.isEmpty())
        language = "unknown";
}

DVDAudio::DVDAudio() : audio_format(""), multichannel_extension(false),
    language(""), application_mode(""), quantization(""),
    sample_frequency(""), channels(0), language_extension("")
{
}

void DVDAudio::printYourself(void) const
{
    //
    //  Debugging
    //

    VERBOSE(VB_GENERAL, QString("    Audio track: %1 %2 %3 %4 Ch")
            .arg(language)
            .arg(audio_format)
            .arg(sample_frequency)
            .arg(channels));
}

QString DVDAudio::GetAudioString(void) const
{
    return QString("%1 %2 %3Ch").arg(language).arg(audio_format).arg(channels);
}

void DVDAudio::fill(audio_attr_t *audio_attributes)
{
    //
    //  this just travels down the audio_attributes struct
    //  (defined in libdvdread) and ticks off the right
    //  information
    //

    switch (audio_attributes->audio_format)
    {
        case 0:
            audio_format = "ac3";
            break;
        case 1:
            audio_format = "oops";
            break;
        case 2:
            audio_format = "mpeg1";
            break;
        case 3:
            audio_format = "mpeg2ext";
            break;
        case 4:
            audio_format = "lpcm";
            break;
        case 5:
            audio_format = "oops";
            break;
        case 6:
            audio_format = "dts";
            break;
        default:
            audio_format = "oops";
    }

    if (audio_attributes->multichannel_extension)
    {
        multichannel_extension = true;
    }

    switch (audio_attributes->lang_type)
    {
        case 0:
            language = "nl";
            break;
        case 1:
            language = lctola(audio_attributes->lang_code);
            break;
        default:
            language = "oops";
    }

    switch (audio_attributes->application_mode)
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

    switch (audio_attributes->quantization)
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

    switch (audio_attributes->sample_frequency)
    {
        case 0:
            sample_frequency = "48kHz";
            break;
        default:
            sample_frequency = "oops";
    }

    channels = audio_attributes->channels + 1;

    switch (audio_attributes->lang_extension)
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

DVDTitle::DVDTitle() :
    numb_chapters(0), numb_angles(0), track_number(0),

    hours(0), minutes(0), seconds(0),

    hsize(0), vsize(0), frame_rate(0.0), fr_code(0), ar_numerator(0),
    ar_denominator(0), aspect_ratio(""), letterbox(false),
    video_format("unknown"), dvdinput_id(0)
{
}

bool DVDTitle::IsValid(void) const
{
    return
        numb_chapters || numb_angles || track_number ||
        hours || minutes || seconds ||
        hsize || vsize || frame_rate != 0.0 ||
        fr_code || ar_numerator ||
        ar_denominator || !aspect_ratio.isEmpty() || letterbox ||
        (video_format != "unknown") || dvdinput_id ||
        audio_tracks.size() || subtitles.size();
}

void DVDTitle::SetTime(uint h, uint m, uint s, double fr)
{
    hours = h;
    minutes = m;
    seconds = s;
    frame_rate = fr;

    //
    //  These are transcode frame rate codes
    //

    if (fr > 23.0 && fr < 24.0)
        fr_code = 1;
    else if (fcomp(fr, 24.0))
        fr_code = 2;
    else if (fcomp(fr, 25.0))
        fr_code = 3;
    else if (fr >  20.0 && fr < 30.0)
        fr_code = 4;
    else if (fcomp(fr, 30.0))
        fr_code = 5;
    else if (fcomp(fr, 50.0))
        fr_code = 6;
    else if (fr > 59.0 && fr < 60.0)
        fr_code = 7;
    else if (fcomp(fr, 60.0))
        fr_code = 8;
    else if (fcomp(fr, 1.0))
        fr_code = 9;
    else if (fcomp(fr, 5.0))
        fr_code = 10;
    else if (fcomp(fr, 10.0))
        fr_code = 11;
    else if (fcomp(fr, 12.0))
        fr_code = 12;
    else if (fcomp(fr, 15.0))
        fr_code = 13;
    else
    {
        fr_code = 0;
        VERBOSE(VB_IMPORTANT,
                QString("dvdprobe.o: Could not find a frame rate code given a"
                        " frame rate of %1").arg(fr));
    }
}

void DVDTitle::SetAR(uint n, uint d, const QString &ar)
{
    ar_numerator = n;
    ar_denominator = d;
    aspect_ratio = ar;
}

uint DVDTitle::GetPlayLength(void) const
{
    return seconds + (60 * minutes) + (60 * 60 * hours);
}

QString DVDTitle::GetTimeString(void) const
{
    return QString().sprintf("%d:%02d:%02d", hours, minutes, seconds);
}

void DVDTitle::printYourself(void) const
{
    //
    //  Debugging
    //

    VERBOSE(VB_IMPORTANT, QString("Track %1 has %2 chapters, %3 angles, "
                                  "and runs for %4 hour(s) %5 minute(s) "
                                  "and %6 seconds at %7 fps.")
            .arg(track_number).arg(numb_chapters).arg(numb_angles).arg(hours)
            .arg(minutes).arg(seconds).arg(frame_rate));

    if (!audio_tracks.empty())
    {
        for (uint i = 0; i < (uint)audio_tracks.size(); ++i)
        {
            audio_tracks[i].printYourself();
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "    No Audio Tracks");
    }
}

void DVDTitle::AddAudio(const DVDAudio &new_audio_track)
{
    audio_tracks.push_back(new_audio_track);
}

void DVDTitle::AddSubTitle(const DVDSubTitle &subtitle)
{
    //
    //  Check if this language already exists
    //  (which happens a lot)
    //
    DVDSubTitle new_subtitle = subtitle;

    int lang_count = 0;
    for (uint i = 0; i < (uint)subtitles.size(); ++i)
    {
        if (subtitles[i].GetLanguage() == new_subtitle.GetLanguage())
        {
            ++lang_count;
        }
    }
    if (lang_count == 0)
    {
        new_subtitle.SetName(QString("<%1>").arg(new_subtitle.GetLanguage()));
    }
    else
    {
        ++lang_count;
        new_subtitle.SetName(QString("<%1> (%2)")
                             .arg(new_subtitle.GetLanguage())
                             .arg(lang_count));
    }
    subtitles.push_back(new_subtitle);
}

void DVDTitle::determineInputID(void)
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

    if (a_query.exec() && a_query.isActive() && a_query.size() > 0)
    {
        a_query.next();
        dvdinput_id = a_query.value(0).toInt();
    }
    else
    {
        QString msg =
                QString("You have a title on your dvd in a format myth doesn't"
                    " understand.\n"
                    "Either that, or you haven't installed the dvdinput"
                    " table.\n"
                    "You probably want to report this to a mailing list or"
                    " something:\n"
                    "                  height = %1\n"
                    "                   width = %2\n"
                    "            aspect ratio = %3 (%4/%5)\n"
                    "              frame rate = %6\n"
                    "                 fr_code = %7\n"
                    "             letterboxed = %8\n"
                    "                  format = %9")
                .arg(hsize).arg(vsize)
                .arg(aspect_ratio).arg(ar_numerator).arg(ar_denominator)
                .arg(frame_rate).arg(fr_code).arg(letterbox).arg(video_format);
        VERBOSE(VB_IMPORTANT, msg);
    }
}

DVDTitle::~DVDTitle()
{
}

/*
---------------------------------------------------------------------
*/


/** \class DVDProbe
 *  \brief This object just figures out what is on a disc
 *         and tells whomever asks about it.
 */

DVDProbe::DVDProbe(const QString &dvd_device) :
    dvd(NULL), first_time(true), device(dvd_device),
    volume_name(QObject::tr("Unknown"))
{
    device.detach();
    volume_name.detach();
}

void DVDProbe::Reset(void)
{
    first_time = true;
    volume_name = QObject::tr("Unknown");
    titles.clear();
}

bool DVDProbe::Probe(void)
{
    //  Before touching libdvdread stuff
    //  (below), see if there's actually
    //  a drive with media in it
    QFile dvdDevice(device);
    if (!dvdDevice.exists())
    {
        //  Device doesn't exist. Silly user
        Reset();
        return false;
    }

    if (!dvdDevice.open(QIODevice::ReadOnly))
    {
        // Can't open device.
        Reset();
        return false;
    }

    int drive_handle = dvdDevice.handle();

    if (drive_handle == -1)
    {
        Reset();
        return false;
    }

#if defined(__linux__) || defined(__FreeBSD__)
    //  I have no idea if the following code block
    //  is anywhere close to the "right way" of doing
    //  this, but it seems to work.

    // Sometimes the first result following an open is a lie. Often the
    // first call will return 4, however an immediate second call will
    // return 2. Anecdotally the first result after an open has only
    // a 1 in 8 chance of detecting changes from 4 to 2, while a second call
    // (in an admittedly small test number) seems to make it much more
    // accurate (with only a 1 in 6 chance that the first result was more
    // accurate than the second).
    int status = ioctl(drive_handle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if (status < 0)
    {
        Reset();
        return false;
    }

    status = ioctl(drive_handle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if (status < 4)
    {
        //
        // -1  error (no disc)
        //  1 = no info
        //  2 = tray open
        //  3 = not ready
        //
        Reset();
        return false;
    }

    status = ioctl(drive_handle, CDROM_MEDIA_CHANGED, NULL);

    if (!status && !first_time)
    {
        //  the disc has not changed. but if this is our
        //  first time running, we still need to check it
        //  so return whatever we returned before
        return titles.size();
    }
#endif

    dvdDevice.close();

    //
    //  Try to open the disc
    //  (as far as libdvdread is concerned, the argument
    //  could be a path, file, whatever).
    //

    Reset();
    first_time = false;
    QByteArray dev = device.toLocal8Bit();
    dvd = DVDOpen(dev.constData());
    if (!dvd)
        return false;

    //
    //  Grab the title of this DVD
    //

    const int arbitrary = 1024;
    char volume_name_buffr[arbitrary + 1];
    unsigned char set_name[arbitrary + 1];

    if (DVDUDFVolumeInfo(dvd, volume_name_buffr, arbitrary, set_name,
                         arbitrary) > -1)
    {
        volume_name = volume_name_buffr;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Error getting volume name, setting to"
                "\"Unknown\"");
    }

    ifo_handle_t *ifo_file = ifoOpen(dvd, 0);
    if (!ifo_file)
    {
        DVDClose(dvd);
        dvd = NULL;
        return false;
    }

    //
    //  Most of this is taken from title_info.c and
    //  ifo_dump.c located in the libdvdread ./src/
    //  directory. Some if is also based on dvd_reader.c
    //  in the transcode source.
    //

    tt_srpt_t *title_info = ifo_file->tt_srpt;
    for (int i = 0; i < title_info->nr_of_srpts; ++i )
    {
        DVDTitle new_title;
        new_title.SetTrack(i + 1);
        new_title.SetChapters((uint) title_info->title[i].nr_of_ptts);
        new_title.SetAngles((uint) title_info->title[i].nr_of_angles);

        ifo_handle_t *video_transport_file = NULL;

        video_transport_file =
            ifoOpen(dvd, title_info->title[i].title_set_nr);
        if (!video_transport_file)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Can't get video transport for track %1")
                    .arg(i+1));
        }
        else
        {

            //
            //  See how simple libdvdread makes it to find the playing
            //  time?
            //
            //  Why have a playingTime(track_number) function, when you
            //  could jump through all these hoops:
            //

            int some_index = title_info->title[i].vts_ttn;
            int some_other_index = video_transport_file->vts_ptt_srpt->
                title[some_index - 1].ptt[0].pgcn;
            pgc_t *current_pgc = video_transport_file->vts_pgcit->
                pgci_srp[some_other_index - 1].pgc;

            dvd_time_t *libdvdread_playback_time =
                &current_pgc->playback_time;

            double framerate = 0.0;

            switch ((libdvdread_playback_time->frame_u & 0xc0) >> 6)
            {
                case 1:
                    framerate = 25.0;  // PAL
                    break;
                case 3:
                    framerate = 24000/1001.0;  // NTSC_FILM
                    break;
                default:
                    VERBOSE(VB_IMPORTANT,
                            "For some odd reason (!), I couldn't get a"
                            " video frame rate");
                    break;
            }

            //
            //  Somebody who knows something about how to program a
            //  computer should clean this up
            //

            new_title.SetTime(decdt(libdvdread_playback_time->hour),
                              decdt(libdvdread_playback_time->minute),
                              decdt(libdvdread_playback_time->second),
                              framerate);

            vtsi_mat_t *vtsi_mat = video_transport_file->vtsi_mat;
            if (vtsi_mat)
            {
                //
                //  and now, wave the divining rod over audio bits
                //

                for (int j = 0; j < vtsi_mat->nr_of_vts_audio_streams; j++)
                {
                    audio_attr_t *audio_attributes =
                        &vtsi_mat->vts_audio_attr[j];
                    DVDAudio new_audio;
                    new_audio.fill(audio_attributes);
                    new_title.AddAudio(new_audio);
                }

                //
                //  determine, with any luck, subtitles
                //

                for (int j = 0; j < vtsi_mat->nr_of_vts_subp_streams;
                     j++)
                {
                    subp_attr_t *sub_attributes =
                        &vtsi_mat->vts_subp_attr[j];
                    if (sub_attributes->type           == 0 &&
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
                        //  The Freakin Disney Corporation seems to
                        //  think it's useful to set language codes
                        //  with weird ass characters. This should
                        //  probably handle that.
                        //
                        QString tmp = QString(
                            lctola(sub_attributes->lang_code).toUtf8());
                        new_title.AddSubTitle(DVDSubTitle(j, tmp));

                    }
                }

                //
                //  Figure out size, aspect ratio, etc.
                //

                video_attr_t *video_attributes =
                    &vtsi_mat->vts_video_attr;

                switch (video_attributes->display_aspect_ratio)
                {
                    case 0:
                        new_title.SetAR(4, 3, "4:3");
                        break;
                    case 3:
                        new_title.SetAR(16, 9, "16:9");
                        break;
                    default:
                        VERBOSE(VB_IMPORTANT, "couldn't get aspect"
                                " ratio for a title");
                }

                switch (video_attributes->video_format)
                {
                    case 0:
                        new_title.SetVFormat("ntsc");
                        break;
                    case 1:
                        new_title.SetVFormat("pal");
                        break;
                    default:
                        VERBOSE(VB_IMPORTANT, "Could not get video"
                                " format for a title");
                }

                if (video_attributes->letterboxed)
                {
                    new_title.SetLBox(true);
                }

                uint c_height = 480;
                if (video_attributes->video_format != 0)
                {
                    c_height = 576;
                }

                switch (video_attributes->picture_size)
                {
                    case 0:
                        new_title.SetSize(720, c_height);
                        break;
                    case 1:
                        new_title.SetSize(704, c_height);
                        break;
                    case 2:
                        new_title.SetSize(352, c_height);
                        break;
                    case 3:
                        new_title.SetSize(352, c_height / 2);
                        break;
                    default:
                        VERBOSE(VB_IMPORTANT, "Could not determine for"
                                " video size for a title.");
                }
                ifoClose(video_transport_file);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Couldn't find any audio or video"
                                " information for track %1").arg(i+1));
            }
        }

        //
        //  Debugging output
        //
        //  new_title.printYourself();

        //
        //  Have the new title figure out it's
        //  appropriate id number vis-a-vis the
        //  dvdinput table
        //

        new_title.determineInputID();

        //
        //  Add this new title to the container
        //

        titles.push_back(new_title);
    }

    ifoClose(ifo_file);
    ifo_file = NULL;

    DVDClose(dvd);
    dvd = NULL;

    return true;
}

DVDTitle DVDProbe::GetTitle(uint which_one) const
{
    DVDTitleList::const_iterator it = titles.begin();
    for (; it != titles.end(); ++it)
    {
        if ((*it).GetTrack() == which_one)
            return *it;
    }
    return DVDTitle();
}

DVDProbe::~DVDProbe(void)
{
    if (dvd)
    {
        DVDClose(dvd);
        dvd = NULL;
    }
    Reset();
}
