// -*- Mode: c++ -*-

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QString>
#include <QtCore>
#include <QtGui>

// MythTV headers
#include "mythccextractorplayer.h"
#include "commandlineparser.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "ringbuffer.h"
#include "exitcodes.h"

namespace {
    void cleanup()
    {
        delete gContext;
        gContext = NULL;

    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

/**
 * Class to write SubRip files.
 */

class SrtWriter
{
  public:
    SrtWriter(const QString &fileName) :
        m_outFile(fileName),
        m_outStream(&m_outFile),
        m_srtCounter(0)
    {
    }

    bool init()
    {
        return m_outFile.open(QFile::WriteOnly);
    }

    /**
     * Adds next subtitle.
     */
    void addSubtitle(const OneSubtitle &sub)
    {
        const char *endl = "\r\n";
        m_outStream << ++m_srtCounter << endl;

        m_outStream << formatTimeForSrt(sub.start_time) << " --> ";
        m_outStream << formatTimeForSrt(sub.start_time + sub.length) << endl;

        foreach (QString line, sub.text)
            m_outStream << line << endl;

        m_outStream << endl;
    }

  private:
    /**
     * Formats time to format appropriate to SubRip file.
     */
    static QString formatTimeForSrt(qint64 time_in_msec)
    {
        qint64 hh, mm, ss, msec;

        msec = time_in_msec % 1000;
        time_in_msec /= 1000;

        ss = time_in_msec % 60;
        time_in_msec /= 60;

        mm = time_in_msec % 60;
        time_in_msec /= 60;

        hh = time_in_msec;

        QString res;
        QTextStream stream(&res);

        stream << qSetPadChar('0') << qSetFieldWidth(2) << hh;
        stream << qSetFieldWidth(0) << ':';
        stream << qSetPadChar('0') << qSetFieldWidth(2) << mm;
        stream << qSetFieldWidth(0) << ':';
        stream << qSetPadChar('0') << qSetFieldWidth(2) << ss;
        stream << qSetFieldWidth(0) << ',';
        stream << qSetPadChar('0') << qSetFieldWidth(3) << msec;

        return res;
    }

    /// Output file.
    QFile m_outFile;
    /// Output stream associated with m_outFile.
    QTextStream m_outStream;

    /// Count of subtitles we already have written.
    int m_srtCounter;
};

/**
 * Sumps all subtitle information to SubRip files.
 */

static void DumpSubtitleInformation(
    const MythCCExtractorPlayer *player, const QString &fileName)
{
    // Determine where we will put extracted info.
    QStringList comps = QFileInfo(fileName).fileName().split(".");
    if (!comps.empty())
        comps.removeLast();

    QDir working_dir(QFileInfo(fileName).path());
    const QString base_name = comps.join(".");

    // Process (DVB) subtitle streams.
    foreach (uint streamId, player->GetSubtitleStreamsList())
    {
        MythCCExtractorPlayer::DVBStreamType subs =
            player->GetDVBSubtitles(streamId);

        const QString dir_name = QString(base_name + ".dvb-%1").arg(streamId);
        if (!working_dir.mkdir(dir_name))
        {
            qDebug() << "Can't make dir " << dir_name;
        }

        const QDir stream_dir(working_dir.filePath(dir_name));

        int cnt = 0;
        foreach (OneSubtitle cur_sub, subs)
        {
            const QString file_name = stream_dir.filePath(
                QString("%1_%2-to-%3.png")
                .arg(cnt)
                .arg(cur_sub.start_time)
                .arg(cur_sub.start_time + cur_sub.length)
                );

            if (!cur_sub.img.save(file_name))
                qDebug() << "Can't write file " << file_name;

            ++cnt;
        }
    }

    // Process CC608.
    foreach (uint streamId, player->GetCC608StreamsList())
    {
        MythCCExtractorPlayer::CC608StreamType subs =
            player->GetCC608Subtitles(streamId);

        foreach (int idx, subs.keys())
        {
            if (subs.value(idx).isEmpty())
                continue;  // Skip empty subtitle streams.

            SrtWriter srtWriter(
                working_dir.filePath(
                    base_name +
                    QString(".cc608-cc%1.srt").arg(idx + 1)));

            if (srtWriter.init())
            {
                foreach (const OneSubtitle &sub, subs.value(idx))
                {
                    srtWriter.addSubtitle(sub);
                }
            }
        }
    }

    // Process CC708.
    foreach (uint streamId, player->GetCC708StreamsList())
    {
        MythCCExtractorPlayer::CC708StreamType subs =
            player->GetCC708Subtitles(streamId);

        foreach (int idx, subs.keys())
        {
            if (subs.value(idx).isEmpty())
                continue;  // Skip empty subtitle streams.

            SrtWriter srtWriter(working_dir.filePath(
                                    base_name
                                    + QString(".cc708-service%1.srt")
                                    .arg(idx, 2, 10, QChar('0'))));
            if (srtWriter.init())
            {
                foreach (const OneSubtitle &sub, subs.value(idx))
                {
                    srtWriter.addSubtitle(sub);
                }
            }
        }
    }

    // Process teletext.
    foreach (uint streamId, player->GetTTXStreamsList())
    {
        MythCCExtractorPlayer::TTXStreamType subs =
            player->GetTTXSubtitles(streamId);

        foreach (int page, subs.keys())
        {
            if (subs.value(page).isEmpty())
                continue;  // Skip empty subtitle streams.

            SrtWriter srtWriter(
                working_dir.filePath(
                    base_name + QString(".ttx-0x%1.srt")
                    .arg(page, 3, 16, QChar('0'))));

            if (srtWriter.init())
            {
                foreach (const OneSubtitle &sub, subs.value(page))
                {
                    srtWriter.addSubtitle(sub);
                }
            }
        }
    }
}

static int RunCCExtract(const ProgramInfo &program_info)
{
    if (!program_info.IsLocal())
    {
        QString msg =
            QString("Only locally accessible files are supported (%1).")
            .arg(program_info.GetPathname());
        cerr << qPrintable(msg) << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QString filename = program_info.GetPathname();
    if (!QFile::exists(filename))
    {
        cerr << qPrintable(
            QString("Could not open input file (%1).").arg(filename)) << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    RingBuffer *tmprbuf = RingBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        cerr << qPrintable(QString("Unable to create RingBuffer for %1")
                           .arg(filename)) << endl;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    bool showProgress = true;
    MythCCExtractorPlayer *ccp = new MythCCExtractorPlayer(showProgress);
    PlayerContext *ctx = new PlayerContext(kCCExtractorInUseID);
    ctx->SetSpecialDecode(kAVSpecialDecode_NoDecode);
    ctx->SetPlayingInfo(&program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(ccp);

    ccp->SetPlayerInfo(NULL, NULL, true, ctx);
    if (ccp->OpenFile() < 0)
    {
        cerr << "Failed to open " << qPrintable(filename) << endl;
        return GENERIC_EXIT_NOT_OK;
    }
    if (!ccp->run())
    {
        cerr << "Failed to decode " << qPrintable(filename) << endl;
        return GENERIC_EXIT_NOT_OK;
    }

    DumpSubtitleInformation(ccp, program_info.GetPathname());

    delete ctx;

    return GENERIC_EXIT_OK;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    bool useDB = false;

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHCCEXTRACTOR);

    MythCCExtractorCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    int retval = cmdline.ConfigureLogging("none");
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QString infile = cmdline.toString("inputfile");
    if (infile.isEmpty())
    {
        cerr << "The input file --infile is required" << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    CleanupGuard callCleanup(cleanup);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(
            false/*use gui*/, false/*prompt for backend*/,
            false/*bypass auto discovery*/, !useDB/*ignoreDB*/))
    {
        cerr << "Failed to init MythContext, exiting." << endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    ProgramInfo pginfo(infile);
    return RunCCExtract(pginfo);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
