// C++ includes
#include <iostream>
#include <sys/stat.h>

// Qt
#include <QFileInfo>
#include <QScopedPointer>

// libmyth* includes
#include "exitcodes.h"
#include "mythlogging.h"
#include "remoteutil.h"
#include "remotefile.h"
#include "mythsystem.h"

// Local includes
#include "recordingutils.h"

static QString formatSize(int64_t sizeKB, int prec)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

static QString CreateProgramInfoString(const ProgramInfo &pginfo)
{
    QDateTime recstartts = pginfo.GetRecordingStartTime();
    QDateTime recendts   = pginfo.GetRecordingEndTime();

    QString timedate = QString("%1 - %2")
        .arg(MythDate::toString(
                 recstartts, MythDate::kDateTimeFull | MythDate::kSimplify))
        .arg(MythDate::toString(recendts, MythDate::kTime));

    QString title = pginfo.GetTitle();

    QString extra;

    if (!pginfo.GetSubtitle().isEmpty())
    {
        extra = QString(" ~ ") + pginfo.GetSubtitle();
        int maxll = max(title.length(), 20);
        if (extra.length() > maxll)
            extra = extra.left(maxll - 3) + "...";
    }

    return QString("%1%2 - %3").arg(title).arg(extra).arg(timedate);
}

static int CheckRecordings(const MythUtilCommandLineParser &cmdline)
{
    cout << "Checking Recordings" << endl;

    ProgramInfo *p;
    std::vector<ProgramInfo *> *recordingList = RemoteGetRecordedList(-1);
    std::vector<ProgramInfo *>  missingRecordings;
    std::vector<ProgramInfo *>  zeroByteRecordings;
    std::vector<ProgramInfo *>  noSeektableRecordings;

    bool foundFile = false;
    bool fixSeektable = cmdline.toBool("fixseektable");

    cout << "Fix seektable is: " << fixSeektable << endl;

    if (recordingList && !recordingList->empty())
    {
        vector<ProgramInfo *>::iterator i = recordingList->begin();
        for ( ; i != recordingList->end(); ++i)
        {
            p = *i;
            // ignore live tv and deleted recordings
            if (p->GetRecordingGroup() == "LiveTV" ||
                p->GetRecordingGroup() == "Deleted")
            {
                i = recordingList->erase(i);
                --i;
                continue;
            }

            cout << "Checking: " << qPrintable(CreateProgramInfoString(*p)) << endl;
            foundFile = true;

            QString url = p->GetPlaybackURL();

            if (url.startsWith('/'))
            {
                QFileInfo fi(url);
                if (!fi.exists())
                {
                    cout << "File not found" << endl;
                    missingRecordings.push_back(p);
                    foundFile = false;
                }
                else
                {
                    if (fi.size() == 0)
                    {
                        cout << "File was found but has zero length" << endl;
                        zeroByteRecordings.push_back(p);
                    }
                }
            }
            else if (url.startsWith("myth:"))
            {
                if (!RemoteFile::Exists(url))
                {
                    cout << "File not found" << endl;
                    missingRecordings.push_back(p);
                    foundFile = false;
                }
                else
                {
                    RemoteFile rf(url);
                    if (rf.GetFileSize() == 0)
                    {
                        cout << "File was found but has zero length" << endl;
                        zeroByteRecordings.push_back(p);
                    }
                }
            }

            frm_pos_map_t posMap;
            p->QueryPositionMap(posMap, MARK_GOP_BYFRAME);
            if (posMap.isEmpty())
                p->QueryPositionMap(posMap, MARK_GOP_START);
                if (posMap.isEmpty())
                    p->QueryPositionMap(posMap, MARK_KEYFRAME);

            if (posMap.isEmpty())
            {
                cout << "No seektable found" << endl;

                noSeektableRecordings.push_back(p);

                if (foundFile && fixSeektable)
                {
                    QString command = QString("mythcommflag --rebuild --chanid %1 --starttime %2")
                                              .arg(p->GetChanID())
                                              .arg(p->GetRecordingStartTime(MythDate::ISODate));
                    cout << "Running - " << qPrintable(command) << endl;
                    QScopedPointer<MythSystem> cmd(MythSystem::Create(command));
                    cmd->Wait(0);
                    if (cmd.data()->GetExitCode() != GENERIC_EXIT_OK)
                    {
                        cout << "ERROR - mythcommflag exited with result: " << cmd.data()->GetExitCode() << endl; 
                    }
                }
            }

            cout << "-------------------------------------------------------------------" << endl;
        }
    }

    if (!missingRecordings.empty())
    {
        cout << endl << endl;
        cout << "MISSING RECORDINGS" << endl;
        cout << "------------------" << endl;
        vector<ProgramInfo *>::iterator i = missingRecordings.begin();
        for ( ; i != missingRecordings.end(); ++i)
        {
            p = *i;
            cout << qPrintable(CreateProgramInfoString(*p)) << endl;
            cout << qPrintable(p->GetPlaybackURL()) << endl;
            cout << "-------------------------------------------------------------------" << endl;
        }
    }

    if (!zeroByteRecordings.empty())
    {
        cout << endl << endl;
        cout << "ZERO BYTE RECORDINGS" << endl;
        cout << "--------------------" << endl;
        vector<ProgramInfo *>::iterator i = zeroByteRecordings.begin();
        for ( ; i != zeroByteRecordings.end(); ++i)
        {
            p = *i;
            cout << qPrintable(CreateProgramInfoString(*p)) << endl;
            cout << qPrintable(p->GetPlaybackURL()) << endl;
            cout << "-------------------------------------------------------------------" << endl;
        }
    }

    if (!noSeektableRecordings.empty())
    {
        cout << endl << endl;
        cout << "NO SEEKTABLE RECORDINGS" << endl;
        cout << "-----------------------" << endl;
        vector<ProgramInfo *>::iterator i = noSeektableRecordings.begin();
        for ( ; i != noSeektableRecordings.end(); ++i)
        {
            p = *i;
            cout << qPrintable(CreateProgramInfoString(*p)) << endl;
            cout << qPrintable(p->GetPlaybackURL()) << endl;
            cout << "File size is " << qPrintable(formatSize(p->GetFilesize(), 2)) << endl;
            cout << "-------------------------------------------------------------------" << endl;
        }
    }

    cout << endl << endl << "SUMMARY" << endl;
    cout << "Recordings           : " << recordingList->size() << endl;
    cout << "Missing Recordings   : " << missingRecordings.size() << endl;
    cout << "Zero byte Recordings : " << zeroByteRecordings.size() << endl;
    cout << "Missing Seektables   : " << noSeektableRecordings.size() << endl;

    return GENERIC_EXIT_OK;
}

void registerRecordingUtils(UtilMap &utilMap)
{
    utilMap["checkrecordings"]         = &CheckRecordings;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
