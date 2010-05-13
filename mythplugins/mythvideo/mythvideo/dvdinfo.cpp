#include <mythcontext.h>

#include "dvdinfo.h"

DVDAudioInfo::DVDAudioInfo(int track_number, const QString &audio_description) :
    description(audio_description), track(track_number), channels(0)
{
}

/*
---------------------------------------------------------------------
*/

DVDTitleInfo::DVDTitleInfo() : numb_chapters(0), numb_angles(0),
    track_number(0), hours(0), minutes(0), seconds(0), is_selected(false),
    selected_quality(-1), selected_audio(1), selected_subtitle(-1)
{
    use_ac3 = gCoreContext->GetNumSetting("MTDac3flag");
}

void DVDTitleInfo::setTime(uint h, uint m, uint s)
{
    hours = h;
    minutes = m;
    seconds = s;
}

uint DVDTitleInfo::getPlayLength()
{
    return seconds + (60 * minutes) + (60 * 60 * hours);
}

QString DVDTitleInfo::getTimeString()
{
    QString a_string;
    a_string.sprintf("%d:%02d:%02d", hours, minutes, seconds);
    return a_string;
}

void DVDTitleInfo::addAudio(DVDAudioInfo *new_audio_track)
{
    audio_tracks.append(new_audio_track);
}

void DVDTitleInfo::addSubTitle(DVDSubTitleInfo *new_subtitle)
{
    subtitles.append(new_subtitle);
}

DVDTitleInfo::~DVDTitleInfo()
{
    while( !audio_tracks.isEmpty() )
        delete audio_tracks.takeFirst();
    audio_tracks.clear();
    while( !subtitles.isEmpty() )
        delete subtitles.takeFirst();
    subtitles.clear();
}

/*
---------------------------------------------------------------------
*/


DVDInfo::DVDInfo(const QString &new_name) : volume_name(new_name)
{
}

DVDTitleInfo* DVDInfo::getTitle(uint which_one)
{
    QListIterator<DVDTitleInfo *> iter(titles);
    while (iter.hasNext())
    {
        DVDTitleInfo *title = iter.next();
        if(title->getTrack() == which_one)
        {
            return title;
        }
    }
    return NULL;
}


DVDInfo::~DVDInfo()
{
    while( !titles.isEmpty() )
        delete titles.takeFirst();
    titles.clear();
}
