#include "tvosdmenuentry.h"
#include "libmyth/mythcontext.h"
#include "libmythdb/mythdb.h"
#include "libmythdb/mythverbose.h"
#define LOC QString("OSDMenuEntry:")
#define LOC_ERR QString("OSDMenuEntry Error:")

/**\class TVOSDMenuEntry
 * \brief  Holds information on which tv state one can view
 * a tv osd menu option. There are 4 states. LiveTV, prerecorded , Video,
 * and DVD. Values can be directly edited in the tvosdmenu table
 * or through the TV OSD Menu Editor
 */

/**
 *  \brief  returns the contents of the osd menu entry
 *          in the form of a qstringlist
 */
QStringList TVOSDMenuEntry::GetData(void)
{
    QStringList values;
    values << category
                << QString("%1").arg(livetv)
                << QString("%1").arg(recorded)
                << QString("%1").arg(video)
                << QString("%1").arg(dvd)
                << description;
    return values;
}

/**
 *  \brief returns whether or not this osd menu entry
 *  should be displayed in the mentioned TVState
 *  return -1 if not applicable to the TVState provided
 */
int TVOSDMenuEntry::GetEntry(TVState state)
{
    switch (state)
    {
        case kState_WatchingLiveTV:
            return livetv;
        case kState_WatchingPreRecorded:
            return recorded;
        case kState_WatchingRecording:
            return recorded;
        case kState_WatchingVideo:
            return video;
        case kState_WatchingDVD:
            return dvd;
        default:
            return -1;
    }

    return -1;
}

/**
 *  \brief  update the OSD Menu Entry livetv, recorded, video and
 *          dvd settings
 */
void TVOSDMenuEntry::UpdateEntry(int livetv_setting, int recorded_setting,
                                int video_setting, int dvd_setting)
{
    QMutexLocker locker(&updateEntryLock);
    livetv = livetv_setting;
    recorded = recorded_setting;
    video = video_setting;
    dvd = dvd_setting;
}

/**
 *  \brief  update the tv state option in the tv osd menu entry
 */
void TVOSDMenuEntry::UpdateEntry(int change, TVState state)
{
    QMutexLocker locker(&updateEntryLock);
    switch (state)
    {
        case kState_WatchingLiveTV:
            livetv = change;
            break;
        case kState_WatchingPreRecorded:
            recorded = change;
            break;
        case kState_WatchingVideo:
            video = change;
            break;
        case kState_WatchingDVD:
            dvd = change;
            break;
        default:
            break;
    }
}

/**
 *  \brief  save the entry to the tvosdmenu table
 */
void TVOSDMenuEntry::UpdateDBEntry(void)
{
    QMutexLocker locker(&updateEntryLock);
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    query.prepare(  " UPDATE tvosdmenu "
                    " SET livetv = :LIVETV, recorded = :RECORDED, "
                    " video = :VIDEO, dvd = :DVD "
                    " WHERE osdcategory = :OSDCATEGORY;");

    query.bindValue(":LIVETV", QString("%1").arg(livetv));
    query.bindValue(":RECORDED", QString("%1").arg(recorded));
    query.bindValue(":VIDEO", QString("%1").arg(video));
    query.bindValue(":DVD", QString("%1").arg(dvd));
    query.bindValue(":OSDCATEGORY", QString("%1").arg(category));

    if (!query.exec())
        MythDB::DBError(LOC + "UpdateDBEntry", query);
}

/**
 * \brief add the tv osd menu entry to the tvosdmenu db table
 */
void TVOSDMenuEntry::CreateDBEntry(void)
{
    QMutexLocker locker(&updateEntryLock);
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    query.prepare(  "INSERT IGNORE INTO tvosdmenu "
                    " (osdcategory, livetv, recorded, "
                    " video, dvd, description) "
                    " VALUES ( :OSDCATEGORY, :LIVETV, "
                    " :RECORDED, :VIDEO, :DVD, :DESCRIPTION);");

    query.bindValue(":OSDCATEGORY", category);
    query.bindValue(":LIVETV", QString("%1").arg(livetv));
    query.bindValue(":RECORDED", QString("%1").arg(recorded));
    query.bindValue(":VIDEO", QString("%1").arg(video));
    query.bindValue(":DVD", QString("%1").arg(dvd));
    query.bindValue(":DESCRIPTION", description);

    if (!query.exec())
        MythDB::DBError(LOC + "CreateDBEntry", query);
}

TVOSDMenuEntryList::TVOSDMenuEntryList(void)
{
    InitDefaultEntries();
}

TVOSDMenuEntryList::~TVOSDMenuEntryList(void)
{
    if (curMenuEntries.isEmpty())
        return;
    while (!curMenuEntries.isEmpty())
        delete curMenuEntries.takeFirst();
}

bool TVOSDMenuEntryList::ShowOSDMenuOption(QString category, TVState state)
{
    TVOSDMenuEntry *entry = FindEntry(category);
    if (!entry)
        return false;
    return (entry->GetEntry(state) > 0);
}

TVOSDMenuEntry *TVOSDMenuEntryList::FindEntry(QString category)
{
    TVOSDMenuEntry *entry = NULL;
    QListIterator<TVOSDMenuEntry*> cm = GetIterator();
    while(cm.hasNext())
    {
        entry = cm.next();
        if (category.compare(entry->GetCategory()) == 0)
            return entry;
    }
    return NULL;
}


/* \brief List default settings for the osd menu. settings are as follows:
*  -1: not allowed.
*   0: disabled, can be enabled through osd menu editor,
*   1: active by default
*/
void TVOSDMenuEntryList::InitDefaultEntries(void)
{
    curMenuEntries.append(new TVOSDMenuEntry(
        "DVD",               -1, -1, -1,  1, "DVD Menu"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "GUIDE",              1,  1,  0,  0, "Program Guide"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "PIP",                1,  1,  1, -1, "Picture-in-Picture"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "INPUTSWITCHING",     1, -1, -1, -1, "Change TV Input"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "EDITCHANNEL",        1, -1, -1, -1, "Edit Channel"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "EDITRECORDING",     -1,  1, -1, -1, "Edit Recording"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "JUMPREC",            1,  1,  1,  0, "List of Recorded Shows"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "CHANNELGROUP",       1, -1, -1, -1, "Channel Groups"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "TOGGLEBROWSE",       1, -1, -1, -1, "Live TV Browse Mode"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "PREVCHAN",           1, -1, -1, -1, "Previous TV Channel"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "TRANSCODE",          0,  1, -1, -1, "Transcode Show"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "COMMSKIP",           0,  1,  0, -1, "Commercial Skip"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "TOGGLEEXPIRE",      -1,  1, -1, -1, "Toggle Expire"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "SCHEDULERECORDING",  1,  1,  0,  0, "Schedule Recordings"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "AUDIOTRACKS",        1,  1,  1,  1, "Audio Tracks"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "SUBTITLETRACKS",     1,  1,  1,  1, "Subtitle Tracks"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "CCTRACKS",           1,  1,  1,  1, "Closed Caption Tracks"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "VIDEOASPECT",        1,  1,  1,  1, "Video Aspect"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "ADJUSTFILL",         1,  1,  1,  1, "Adjust Fill"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "MANUALZOOM",         1,  1,  1,  1, "Manual Zoom"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "ADJUSTPICTURE",      1,  1,  1,  1, "Adjust Picture"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "AUDIOSYNC",          1,  1,  1,  1, "Audio Sync"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "TIMESTRETCH",        1,  1,  1,  1, "Time Stretch"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "VIDEOSCAN",          1,  1,  1,  1, "Video Scan"));
    curMenuEntries.append(new TVOSDMenuEntry(
        "SLEEP",              1,  1,  1,  1, "Sleep"));

    GetEntriesFromDB();
    if (GetCount() != curMenuEntries.count())
        UpdateDB();
}

int TVOSDMenuEntryList::GetCount(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return curMenuEntries.count();

    query.prepare ("SELECT COUNT(osdcategory) FROM tvosdmenu;");
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toUInt();
    }

    MythDB::DBError(LOC + "GetCount()", query);
    return 0;
}

void TVOSDMenuEntryList::GetEntriesFromDB(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    TVOSDMenuEntry *entry = NULL;
    QString osdcategory;
    QListIterator<TVOSDMenuEntry *> cm = GetIterator();
    while (cm.hasNext())
    {
        entry = cm.next();
        osdcategory = entry->GetCategory();
        query.prepare("SELECT livetv, recorded, video, dvd "
                    "FROM tvosdmenu WHERE osdcategory = :OSDCATEGORY;");
        query.bindValue(":OSDCATEGORY", osdcategory);

        if (query.exec() && query.isActive() && query.size() == 1)
        {
            query.next();
            entry->UpdateEntry(query.value(0).toInt(),
                               query.value(1).toInt(),
                               query.value(2).toInt(),
                               query.value(3).toInt());
        }
    }
}

void TVOSDMenuEntryList::UpdateDB(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    TVOSDMenuEntry *entry = NULL;
    QString osdcategory;
    QListIterator<TVOSDMenuEntry *> cm = GetIterator();
    while (cm.hasNext())
    {
        entry = cm.next();
        osdcategory = entry->GetCategory();
        query.prepare("SELECT osdcategory FROM tvosdmenu "
                        "WHERE osdcategory = :OSDCATEGORY;");
        query.bindValue(":OSDCATEGORY", osdcategory);
        if (query.exec() && query.isActive() && query.size() > 0)
            entry->UpdateDBEntry();
        else
            entry->CreateDBEntry();
    }
}


QListIterator<TVOSDMenuEntry*> TVOSDMenuEntryList::GetIterator(void)
{
    return QListIterator<TVOSDMenuEntry*>(curMenuEntries);
}
