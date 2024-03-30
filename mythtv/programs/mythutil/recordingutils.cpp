
// C++ includes
#include <algorithm> // for max
#include <iostream> // for cout, endl
#include <sys/stat.h>

// Qt
#include <QFileInfo>
#include <QScopedPointer>

// libmyth* includes
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystem.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/stringutil.h"

// Local includes
#include "recordingutils.h"

static QString CreateProgramInfoString(const ProgramInfo &pginfo)
{
    QDateTime recstartts = pginfo.GetRecordingStartTime();
    QDateTime recendts   = pginfo.GetRecordingEndTime();

    QString timedate = QString("%1 - %2")
        .arg(MythDate::toString(
                 recstartts, MythDate::kDateTimeFull | MythDate::kSimplify),
             MythDate::toString(recendts, MythDate::kTime));

    QString title = pginfo.GetTitle();

    QString extra;

    if (!pginfo.GetSubtitle().isEmpty())
    {
        extra = QString(" ~ ") + pginfo.GetSubtitle();
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int maxll = std::max(title.length(), 20);
#else
        int maxll = std::max(title.length(), 20LL);
#endif
        if (extra.length() > maxll)
            extra = extra.left(maxll - 3) + "...";
    }

    return QString("%1%2 - %3").arg(title, extra, timedate);
}

static int CheckRecordings(const MythUtilCommandLineParser &cmdline)
{
    std::cout << "Checking Recordings" << std::endl;

    std::vector<ProgramInfo *> *recordingList = RemoteGetRecordedList(-1);
    std::vector<ProgramInfo *>  missingRecordings;
    std::vector<ProgramInfo *>  zeroByteRecordings;
    std::vector<ProgramInfo *>  noSeektableRecordings;

    if (!recordingList)
    {
        std::cout << "ERROR - failed to get recording list from backend" << std::endl;
        return GENERIC_EXIT_NOT_OK;
    }

    bool fixSeektable = cmdline.toBool("fixseektable");

    std::cout << "Fix seektable is: " << fixSeektable << std::endl;

    if (!recordingList->empty())
    {
        for (auto i = recordingList->begin(); i != recordingList->end(); ++i)
        {
            ProgramInfo *p = *i;
            // ignore live tv and deleted recordings
            if (p->GetRecordingGroup() == "LiveTV" ||
                p->GetRecordingGroup() == "Deleted")
            {
                i = recordingList->erase(i);
                --i;
                continue;
            }

            std::cout << "Checking: " << qPrintable(CreateProgramInfoString(*p)) << std::endl;
            bool foundFile = true;

            QString url = p->GetPlaybackURL();

            if (url.startsWith('/'))
            {
                QFileInfo fi(url);
                if (!fi.exists())
                {
                    std::cout << "File not found" << std::endl;
                    missingRecordings.push_back(p);
                    foundFile = false;
                }
                else
                {
                    if (fi.size() == 0)
                    {
                        std::cout << "File was found but has zero length" << std::endl;
                        zeroByteRecordings.push_back(p);
                    }
                }
            }
            else if (url.startsWith("myth:"))
            {
                if (!RemoteFile::Exists(url))
                {
                    std::cout << "File not found" << std::endl;
                    missingRecordings.push_back(p);
                    foundFile = false;
                }
                else
                {
                    RemoteFile rf(url);
                    if (rf.GetFileSize() == 0)
                    {
                        std::cout << "File was found but has zero length" << std::endl;
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
                std::cout << "No seektable found" << std::endl;

                noSeektableRecordings.push_back(p);

                if (foundFile && fixSeektable)
                {
                    QString command = GetAppBinDir() + "mythcommflag " +
                                              QString("--rebuild --chanid %1 --starttime %2")
                                              .arg(p->GetChanID())
                                              .arg(p->GetRecordingStartTime(MythDate::ISODate));
                    std::cout << "Running - " << qPrintable(command) << std::endl;
                    QScopedPointer<MythSystem> cmd(MythSystem::Create(command));
                    cmd->Wait(0s);
                    if (cmd.data()->GetExitCode() != GENERIC_EXIT_OK)
                    {
                        std::cout << "ERROR - mythcommflag exited with result: " << cmd.data()->GetExitCode() << std::endl;
                    }
                }
            }

            std::cout << "-------------------------------------------------------------------" << std::endl;
        }
    }

    if (!missingRecordings.empty())
    {
        std::cout << std::endl << std::endl;
        std::cout << "MISSING RECORDINGS" << std::endl;
        std::cout << "------------------" << std::endl;
        for (auto *p : missingRecordings)
        {
            std::cout << qPrintable(CreateProgramInfoString(*p)) << std::endl;
            std::cout << qPrintable(p->GetPlaybackURL()) << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
        }
    }

    if (!zeroByteRecordings.empty())
    {
        std::cout << std::endl << std::endl;
        std::cout << "ZERO BYTE RECORDINGS" << std::endl;
        std::cout << "--------------------" << std::endl;
        for (auto *p : zeroByteRecordings)
        {
            std::cout << qPrintable(CreateProgramInfoString(*p)) << std::endl;
            std::cout << qPrintable(p->GetPlaybackURL()) << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
        }
    }

    if (!noSeektableRecordings.empty())
    {
        std::cout << std::endl << std::endl;
        std::cout << "NO SEEKTABLE RECORDINGS" << std::endl;
        std::cout << "-----------------------" << std::endl;
        for (auto *p : noSeektableRecordings)
        {
            std::cout << qPrintable(CreateProgramInfoString(*p)) << std::endl;
            std::cout << qPrintable(p->GetPlaybackURL()) << std::endl;
            std::cout << "File size is " << qPrintable(StringUtil::formatBytes(p->GetFilesize(), 2)) << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
        }
    }

    std::cout << std::endl << std::endl << "SUMMARY" << std::endl;
    std::cout << "Recordings           : " << recordingList->size() << std::endl;
    std::cout << "Missing Recordings   : " << missingRecordings.size() << std::endl;
    std::cout << "Zero byte Recordings : " << zeroByteRecordings.size() << std::endl;
    std::cout << "Missing Seektables   : " << noSeektableRecordings.size() << std::endl;

    return GENERIC_EXIT_OK;
}

void registerRecordingUtils(UtilMap &utilMap)
{
    utilMap["checkrecordings"]         = &CheckRecordings;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
