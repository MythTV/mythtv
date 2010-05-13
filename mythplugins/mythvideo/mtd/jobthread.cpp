/*
    jobthread.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Where things actually happen

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <vector>
using namespace std;

#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QWaitCondition>
#include <QProcess>

#include <mythcontext.h>
#include <mythdbcon.h>
#include <compat.h>
#include <util.h>

#include "jobthread.h"
#include "mtd.h"
#include "threadevents.h"
#include "dvdprobe.h"

//#define DEBUG_STAGE_2 "/path/to/vob/from/rip/stage/filaname.vob"

namespace {
    struct delete_file {
        bool operator()(const QString &filename)
        {
            VERBOSE(VB_GENERAL, QString("Deleting file: %1").arg(filename));
            return QDir::current().remove(filename);
        }
    };
}

static QString shell_escape(const QString &orig)
{
    if (!orig.contains(' ') && !orig.contains('\t') &&
        !orig.contains("'") && !orig.contains('\n') &&
        !orig.contains('`') && !orig.contains('\r') &&
        !orig.contains('"'))
    {
        return orig;
    }

    QString tmp = orig;
    tmp.replace('"', "\\\"");
    tmp.replace("`", "\\`");
    return QString("\"%1\"").arg(tmp);
};

JobThread::JobThread(
    MythTranscodeDaemon *owner,
    const QString       &start_string,
    int                  nice_priority) :
    QThread(),

    problem_string(""),
    job_name(""),
    subjob_name(""),
    job_string(start_string),

    overall_progress(0.0),
    subjob_progress(0.0),
    sub_to_overall_multiple(1.0),
    parent(owner),
    nice_level(nice_priority),

    cancelLock(new QMutex()),
    cancelWaitCond(new QWaitCondition()),
    cancel_me(false)
{
}

JobThread::~JobThread()
{
    delete cancelLock;
    delete cancelWaitCond;
}

void JobThread::run(void)
{
    VERBOSE(VB_IMPORTANT,
            "Programmer Error: JobThread::run() needs to be subclassed");
}

void JobThread::Cancel(bool chatty)
{
    if (chatty)
    {
        SetSubProgress(0.0, 0);
        SetSubName("Cancelling ...", 0);
    }

    QMutexLocker locker(cancelLock);
    cancel_me = true;
    cancelWaitCond->wakeAll();
}

bool JobThread::IsCancelled(void) const
{
    QMutexLocker locker(cancelLock);
    return cancel_me;
}

QString JobThread::GetLastProblem(void) const
{
    QMutexLocker qml(&problem_string_mutex);
    QString tmp = problem_string; tmp.detach();
    return tmp;
}

QString JobThread::GetJobString(void) const
{
    QMutexLocker qml(&job_string_mutex);
    QString tmp = job_string; tmp.detach();
    return tmp;
}

void JobThread::UpdateSubjobString(int seconds_elapsed,
                                   const QString &pre_string)
{
    int estimated_job_time = 0;
    if (subjob_progress > 0.0)
    {
        estimated_job_time = (int)
            ( (double) seconds_elapsed / subjob_progress );
    }
    else
    {
        estimated_job_time = (int)
            ( (double) seconds_elapsed / 0.000001 );
    }

    QString new_name = "";
    if (estimated_job_time >= 3600)
    {
        new_name.sprintf(" %d:%02d:%02d/%d:%02d:%02d",
                         seconds_elapsed / 3600,
                         (seconds_elapsed % 3600) / 60,
                         (seconds_elapsed % 3600) % 60,
                         estimated_job_time / 3600,
                         (estimated_job_time % 3600) / 60,
                         (estimated_job_time % 3600) % 60);
    }
    else
    {
        new_name.sprintf(" %02d:%02d/%02d:%02d",
                         seconds_elapsed / 60,
                         seconds_elapsed % 60,
                         estimated_job_time / 60,
                         estimated_job_time % 60);
    }
    new_name.prepend(pre_string);
    SetSubName(new_name, 1);
}

void JobThread::SetSubProgress(double some_value, uint priority)
{
    if (priority > 0)
    {
        while (!subjob_progress_mutex.tryLock())
        {
            QMutexLocker locker(cancelLock);
            if (!cancel_me)
                cancelWaitCond->wait(cancelLock, 1000 * priority);
        }

        if (!IsCancelled())
            subjob_progress = some_value;
    }
    else
    {
        subjob_progress_mutex.lock();
        if (!IsCancelled())
            subjob_progress = some_value;
    }
    subjob_progress_mutex.unlock();
}

void JobThread::SetSubName(const QString &new_name, uint priority)
{
    if (priority > 0)
    {
        while (!subjob_name_mutex.tryLock())
        {
            QMutexLocker locker(cancelLock);
            if (!cancel_me)
                cancelWaitCond->wait(cancelLock, 1000 * priority);
        }

        if (!IsCancelled())
            subjob_name = new_name;
    }
    else
    {
        subjob_name_mutex.lock();
        if (!IsCancelled())
            subjob_name = new_name;
    }
    subjob_name_mutex.unlock();

}

QString JobThread::GetJobName(void) const
{
    QMutexLocker qml(&job_name_mutex);
    QString tmp = job_name; tmp.detach();
    return tmp;
}

QString JobThread::GetSubName(void) const
{
    QMutexLocker qml(&subjob_name_mutex);
    QString tmp = subjob_name; tmp.detach();
    return tmp;
}

void JobThread::SendProblemEvent(const QString &a_problem)
{
    ErrorEvent *ee = new ErrorEvent(a_problem);
    QApplication::postEvent(parent, ee);

    SetLastProblem(a_problem);
}

void JobThread::SendLoggingEvent(const QString &event_string)
{
    LoggingEvent *le = new LoggingEvent(event_string);
    QApplication::postEvent(parent, le);
}

void JobThread::SetJobName(const QString &jname)
{
    QMutexLocker qml(&job_name_mutex);
    job_name = jname;
    job_name.detach();
}

void JobThread::SetLastProblem(const QString &prob)
{
    QMutexLocker qml(&problem_string_mutex);
    problem_string = prob;
    problem_string.detach();
}

/*
---------------------------------------------------------------------
*/

namespace
{
    class MutexUnlocker
    {
      public:
        MutexUnlocker(QMutex *mutex) : m_mutex(mutex)
        {
            if (!m_mutex)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("%1: Invalid mutex passed to MutexUnlocker")
                        .arg(__FILE__));
            }
        }

        ~MutexUnlocker()
        {
            m_mutex->unlock();
        }

      private:
        QMutex *m_mutex;
    };

    template <typename HTYPE>
    struct is_handle_null
    {
        bool operator()(HTYPE handle)
        {
            return handle == NULL;
        }
    };

    template <typename HTYPE>
    struct is_bad_stdio_handle
    {
        bool operator()(HTYPE handle)
        {
            return handle == -1;
        }
    };

    template <typename HTYPE, typename CLEANF_RET = void,
             typename handle_checker = is_handle_null<HTYPE> >
    class SmartHandle
    {
      private:
        typedef CLEANF_RET (*clean_fun_t)(HTYPE);

      public:
        SmartHandle(HTYPE handle, clean_fun_t cleaner) : m_handle(handle),
                                                         m_cleaner(cleaner)
        {
        }

        ~SmartHandle()
        {
            handle_checker hc;
            if (!hc(m_handle))
            {
                m_cleaner(m_handle);
            }
        }

        HTYPE get()
        {
            return m_handle;
        }

        HTYPE operator->()
        {
            return m_handle;
        }

      private:
        HTYPE m_handle;
        clean_fun_t m_cleaner;
    };
}

DVDThread::DVDThread(
    MythTranscodeDaemon *owner,
    QMutex              *drive_mutex,
    const QString       &dvd_device,
    int                  track,
    const QString       &dest_file,
    const QString       &name,
    const QString       &start_string,
    int                  nice_priority) :
    JobThread(owner, start_string, nice_priority),
    dvd_device_access(drive_mutex),
    dvd_device_location(dvd_device),
    destination_file_string(dest_file),
    dvd_title(track - 1),
    rip_name(name)
{
}

void DVDThread::run(void)
{
    VERBOSE(VB_IMPORTANT,
            "Programmer Error: DVDThread::run() needs to be subclassed");
}

bool DVDThread::ripTitle(int            title_number,
                         const QString &to_location,
                         const QString &extension,
                         bool           multiple_files,
                         QStringList   *output_files)
{
    //  Can't do much until I have a lock on the device
    SetSubName(QObject::tr("Waiting For Access to DVD"), 1);
    bool loop = true;
    while (loop)
    {
        if (dvd_device_access->tryLock())
        {
            loop = false;
        }
        else
        {
            QMutexLocker locker(cancelLock);
            if (!cancel_me)
                cancelWaitCond->wait(cancelLock, 5000);

            if (cancel_me)
            {
                SendProblemEvent("abandoned job because master control "
                                 "said we need to shut down");
                return false;
            }
        }
    }

    MutexUnlocker qmul(dvd_device_access);

    if (IsCancelled())
    {
        SendProblemEvent("abandoned job because master control "
                         "said we need to shut down");
        return false;
    }

    SendLoggingEvent("job thread beginning to rip dvd title");

#ifdef DEBUG_STAGE_2
    if (output_files)
        *output_files = QStringList(DEBUG_STAGE_2);
    SendLoggingEvent("job thread finished ripping dvd title");
    return true;
#endif // DEBUG_STAGE_2

    // OK, we got the device lock. Lets open our destination file
    RipFile ripfile(to_location, extension, true);
    if (!ripfile.open(QIODevice::WriteOnly | QIODevice::Unbuffered |
                      QIODevice::Truncate, multiple_files))
    {
        SendProblemEvent(
            QString("DVDPerfectThread could not open output file: '%1'")
            .arg(ripfile.name()));
        return false;
    }

    // Time to open up access with all the funky structs from libdvdnav
    int angle = 0; // TODO Change this at some point
    QByteArray dvd_dev_loc = dvd_device_location.toLocal8Bit();
    SmartHandle<dvd_reader_t *> the_dvd(
        DVDOpen(dvd_dev_loc.constData()), DVDClose);

    if (!the_dvd.get())
    {
        SendProblemEvent(
            QString("DVDPerfectThread could not access this dvd device: '%1'")
            .arg(dvd_device_location));
        return false;
    }

    SmartHandle<ifo_handle_t *> vmg_file(ifoOpen(the_dvd.get(), 0), ifoClose);
    if (!vmg_file.get())
    {
        SendProblemEvent("DVDPerfectThread could not open VMG info.");
        return false;
    }
    tt_srpt_t *tt_srpt = vmg_file->tt_srpt;

    //  Check title # is valid
    if (title_number < 0 || title_number > tt_srpt->nr_of_srpts )
    {
        SendProblemEvent(
            QString("DVDPerfectThread could not open title number %1")
            .arg(title_number + 1));
        return false;
    }

    SmartHandle<ifo_handle_t *> vts_file(
            ifoOpen(the_dvd.get(), tt_srpt->title[title_number].title_set_nr),
            ifoClose);
    if (!vts_file.get())
    {
        SendProblemEvent(
            "DVDPerfectThread could not open the title's info file");
        return false;
    }

    //  Determine the program chain (?)
    int             ttn          = tt_srpt->title[title_number].vts_ttn;
    vts_ptt_srpt_t *vts_ptt_srpt = vts_file->vts_ptt_srpt;
    int             pgc_id       = vts_ptt_srpt->title[ttn-1].ptt[0].pgcn;
    int             pgn          = vts_ptt_srpt->title[ttn-1].ptt[0].pgn;
    pgc_t          *cur_pgc      = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    int             start_cell   = cur_pgc->program_map[pgn-1]-1;

    //  Hmmmm ... need some sort of total to calculate
    //  progress display against .... I guess disc
    //  sectors will have to do ....
    int total_sectors = 0;
    for (int i = start_cell; i < cur_pgc->nr_of_cells; i++)
    {
        total_sectors += (cur_pgc->cell_playback[i].last_sector -
                          cur_pgc->cell_playback[i].first_sector);
    }

    //  OK ... now actually open
    SmartHandle<dvd_file_t *> title(
            DVDOpenFile(the_dvd.get(),
                    tt_srpt->title[title_number].title_set_nr,
                    DVD_READ_TITLE_VOBS),
            DVDCloseFile);

    if (!title.get())
    {
        SendProblemEvent(
            "DVDPerfectThread could not open the title's actual VOB(s)");
        return false;
    }

    int sector_counter = 0;

    QTime job_time;
    job_time.start();

    vector<unsigned char> video_data(1024 * DVD_VIDEO_LB_LEN);

    int next_cell = start_cell;
    for (int cur_cell = start_cell; next_cell < cur_pgc->nr_of_cells; )
    {
        cur_cell = next_cell;
        if (cur_pgc->cell_playback[cur_cell].block_type ==
            BLOCK_TYPE_ANGLE_BLOCK)
        {

            cur_cell += angle;
            for (int i=0;; ++i)
            {
                if (cur_pgc->cell_playback[cur_cell + i].block_mode ==
                    BLOCK_MODE_LAST_CELL)
                {
                    next_cell = cur_cell + i + 1;
                    break;
                }
             }
         }
         else
         {
            next_cell = cur_cell + 1;
         }

         // Loop until we're out of this cell
         for (uint cur_pack = cur_pgc->cell_playback[cur_cell].first_sector;
              cur_pack < cur_pgc->cell_playback[cur_cell].last_sector; )
         {
            dsi_t dsi_pack;
            unsigned int next_vobu, next_ilvu_start, cur_output_size;

            //  Read the NAV packet.
            int len = DVDReadBlocks(title.get(), (int) cur_pack, 1,
                                    &video_data[0]);
            if ( len != 1)
            {
                SendProblemEvent(
                    QString("DVDPerfectThread read failed for block %1")
                    .arg(cur_pack));
                return false;
            }

            //  Parse the contained dsi packet
            navRead_DSI(&dsi_pack, &video_data[DSI_START_BYTE]);

            //  Figure out where to go next
            next_ilvu_start = cur_pack + dsi_pack.sml_agli.data[angle].address;
            cur_output_size = dsi_pack.dsi_gi.vobu_ea;

            if (dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL )
            {
                next_vobu = cur_pack +
                    ( dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
            }
            else
            {
                next_vobu = cur_pack + cur_output_size + 1;
            }

            cur_pack++;
            sector_counter++;

            //  Read in cursize packs
            len = DVDReadBlocks(
                title.get(), (int)cur_pack, cur_output_size, &video_data[0]);
            if (len != (int) cur_output_size)
            {
                SendProblemEvent(QString("DVDPerfectThread read failed for %1 "
                                         "blocks at %2")
                                 .arg(cur_output_size).arg(cur_pack));
                return false;
            }

            if (IsCancelled())
            {
                SendProblemEvent("abandoned job because master control "
                                 "said we need to shut down");
                return false;
            }

            // Set the progress and write out the blocks..
            SetSubProgress((double) (sector_counter) /
                           (double) (total_sectors), 1);
            overall_progress = subjob_progress * sub_to_overall_multiple;
            UpdateSubjobString(job_time.elapsed() / 1000,
                               QObject::tr("Ripping to file ~"));
            if (!ripfile.writeBlocks(&video_data[0],
                            cur_output_size * DVD_VIDEO_LB_LEN))
            {
                SendProblemEvent("Couldn't write blocks during a rip. "
                                 "Filesystem size exceeded? Disc full?");
                return false;
            }

            sector_counter += next_vobu - cur_pack;
            cur_pack = next_vobu;

            if (IsCancelled())
            {
                SendProblemEvent("abandoned job because master control "
                                 "said we need to shut down");
                return false;
            }
        }
    }

    // Wow, we're done.
    QStringList sl = ripfile.close();

    if (output_files)
        *output_files = sl;

    SendLoggingEvent("job thread finished ripping dvd title");

    VERBOSE(VB_GENERAL, QString("Output files from rip: '") +
            sl.join("','") + "'");

    return true;
}

DVDThread::~DVDThread()
{
}

/*
---------------------------------------------------------------------
*/

DVDISOCopyThread::DVDISOCopyThread(
    MythTranscodeDaemon *owner,
    QMutex              *drive_mutex,
    const QString       &dvd_device,
    int                  track,
    const QString       &dest_file,
    const QString       &name,
    const QString       &start_string,
    int                  nice_priority) :
    DVDThread(owner, drive_mutex, dvd_device, track, dest_file, name,
              start_string, nice_priority)
{
    SendLoggingEvent(QString("Using DVD source: %1").arg(dvd_device));
}

void DVDISOCopyThread::run(void)
{
    myth_nice(nice_level);
    SetJobName(QString(QObject::tr("ISO copy of %1")).arg(rip_name));
    if (!IsCancelled())
    {
        copyFullDisc();
    }
}

bool DVDISOCopyThread::copyFullDisc(void)
{
    bool loop = true;

    SetSubName(QObject::tr("Waiting for access to DVD"), 1);

    while (loop)
    {
        if (dvd_device_access->tryLock())
        {
            loop = false;
        }
        else
        {
            QMutexLocker locker(cancelLock);
            if (!cancel_me)
                cancelWaitCond->wait(cancelLock, 5000);

            if (cancel_me)
            {
                SendProblemEvent("abandoned job because master control "
                                 "said we need to shut down");
                return false;
            }
        }
    }

    MutexUnlocker qmul(dvd_device_access);
    if (IsCancelled())
    {
        SendProblemEvent("abandoned job because master control "
                         "said we need to shut down");
        return false;
    }

    RipFile ripfile(destination_file_string, ".iso", true);
    if (!ripfile.open(QIODevice::WriteOnly | QIODevice::Unbuffered |
                      QIODevice::Truncate, false))
    {
        SendProblemEvent(
            QString("DVDISOCopyThread could not open output file: %1")
            .arg(ripfile.name()));
        return false;
    }

    SendLoggingEvent(QString("ISO DVD image copy to: %1").arg(ripfile.name()));

    QByteArray dvd_dev_loc = dvd_device_location.toLocal8Bit();
    SmartHandle<int, int, is_bad_stdio_handle<int> > file(
        open(dvd_dev_loc.constData(), O_RDONLY), close);

    if (file.get() == -1)
    {
        SendProblemEvent(
            QString("DVDISOCopyThread could not open dvd device: %1")
            .arg(dvd_device_location));
        return false;
    }

    off_t dvd_size = lseek(file.get(), 0, SEEK_END);
    lseek(file.get(), 0, SEEK_SET);

    // Only happens on Darwin?
    if (dvd_size < 2048)
        SendLoggingEvent(QString("DVDISOCopyThread: bad disk size (%1 bytes)")
                         .arg(dvd_size));

    const int buf_size = 1024 * 1024;
    std::vector<unsigned char> buffer(buf_size);
    long long total_bytes(0);

    QTime job_time;
    job_time.start();

    while (1)
    {
        int bytes_read = read(file.get(), &buffer[0], buf_size);
        if (bytes_read == -1)
        {
            perror("read");
            SendProblemEvent("DVDISOCopyThread dvd device read error");
            return false;
        }
        if (bytes_read == 0)
        {
            break;
        }

        if (!ripfile.writeBlocks(&buffer[0], bytes_read))
        {
            SendProblemEvent("DVDISOCopyThread rip file write error");
            return false;
        }

        total_bytes += bytes_read;

        SetSubProgress((double) (total_bytes) / (double) (dvd_size), 1);
        overall_progress = subjob_progress * sub_to_overall_multiple;
        UpdateSubjobString(job_time.elapsed() / 1000,
                           QObject::tr("Ripping to file ~"));

        //  Escape out and clean up if mtd main thread tells us to
        if (IsCancelled())
        {
            SendProblemEvent("abandoned job because master control "
                             "said we need to shut down");
            return false;
        }
    }

    ripfile.close();
    SendLoggingEvent("job thread finished copying ISO image");
    return true;
}

DVDISOCopyThread::~DVDISOCopyThread()
{
}

/*
---------------------------------------------------------------------
*/

DVDPerfectThread::DVDPerfectThread(
    MythTranscodeDaemon *owner,
    QMutex              *drive_mutex,
    const QString       &dvd_device,
    int                  track,
    const QString       &dest_file,
    const QString       &name,
    const QString       &start_string,
    int nice_priority) :
    DVDThread(owner, drive_mutex, dvd_device, track, dest_file, name,
              start_string, nice_priority)
{
}

void DVDPerfectThread::run(void)
{
    myth_nice(nice_level);

    SetJobName(QString(QObject::tr("Perfect DVD Rip of %1")).arg(rip_name));

    if (!IsCancelled())
        ripTitle(dvd_title, destination_file_string, ".vob", true);
}

DVDPerfectThread::~DVDPerfectThread()
{
}

/*
---------------------------------------------------------------------
*/

DVDTranscodeThread::DVDTranscodeThread(
    MythTranscodeDaemon *owner,
    QMutex              *drive_mutex,
    const QString       &dvd_device,
    int                  track,
    const QString       &dest_file,
    const QString       &name,
    const QString       &start_string,
    int                  nice_priority,
    int                  quality_level,
    bool                 do_ac3,
    int                  which_audio,
    int                  numb_seconds,
    int                  subtitle_track_numb) :
    DVDThread(owner, drive_mutex, dvd_device, track, dest_file, name,
              start_string, nice_priority),
    used_transcode_slot(false),

    quality(quality_level),
    working_directory(NULL),
    two_pass(false),
    audio_track(which_audio),
    ac3_flag(do_ac3),
    subtitle_track(subtitle_track_numb),
    secs_mult(1.0f),
    tc_process(NULL),
    tc_command(QString::null),
    tc_current_pass(0),
    tc_tick_tock(0),
    tc_seconds_encoded(0),
    tc_timer(0)
{
    uint length_in_seconds = (numb_seconds) ? numb_seconds : 1;
    secs_mult = 1.0f / length_in_seconds;
}

void DVDTranscodeThread::run(void)
{
    myth_nice(nice_level);

    // Make working directory
    if (IsCancelled() || !makeWorkingDirectory())
        return;

    if (IsCancelled())
        return;

    // Build the transcode command line. We do this early
    // (before ripping) so we can figure out if this is
    // a two pass job or not (for the progress display)
    if (!buildTranscodeCommandLine(1))
    {
        cleanUp();
        return;
    }

    if (two_pass)
    {
        SetJobName(QString(QObject::tr("Transcode of %1")).arg(rip_name));
        sub_to_overall_multiple = 0.333333333;
    }
    else
    {
        SetJobName(QString(QObject::tr("Transcode of %1")).arg(rip_name));
        sub_to_overall_multiple = 0.50;
    }

    if (IsCancelled())
        return;

    //  Rip VOB to working directory
    QStringList output_files;
    QString rip_file_string = QString("%1/vob/%2")
        .arg(working_directory->path()).arg(rip_name);
    if (!ripTitle(dvd_title, rip_file_string, ".vob", true, &output_files))
    {
        cleanUp();
        return;
    }

    // Get permission to start transcoding from MTD instance.
    SetSubName(QObject::tr("Waiting for Permission to Start Transcoding"), 1);
    bool loop = true;
    while (loop)
    {
        if (parent->IncrConcurrentTranscodeCounter() && !IsCancelled())
        {
            used_transcode_slot = true;
            loop = false;
        }
        else
        {
            QMutexLocker locker(cancelLock);
            if (!cancel_me)
                cancelWaitCond->wait(cancelLock, 5000);

            if (cancel_me)
            {
                SendProblemEvent("abandoned job because master control "
                                 "said we need to shut down");
                return;
            }
        }
    }

    //  Run the first (only?) crack at transcoding
    if (!IsCancelled() && !runTranscode(1))
    {
        wipeClean();
        return;
    }

    // Second pass, if enabled
    if (two_pass && !IsCancelled() && !runTranscode(2))
        wipeClean();

#ifndef DEBUG_STAGE_2
    if (!gCoreContext->GetNumSetting("mythdvd.mtd.SaveTranscodeIntermediates", 0))
    {
        // remove any temporary titles that are now transcoded
        std::for_each(output_files.begin(), output_files.end(), delete_file());
    }
#endif

    cleanUp();
}

void DVDTranscodeThread::Cancel(bool chatty)
{
    DVDThread::Cancel(chatty);
    QMutexLocker locker(cancelLock);
    if (tc_timer)
    {
        killTimer(tc_timer);
        tc_timer = QObject::startTimer(0);
    }
}

bool DVDTranscodeThread::makeWorkingDirectory(void)
{
    QString dir_name = gCoreContext->GetSetting("DVDRipLocation");
    if (dir_name.isEmpty())
    {
        SendProblemEvent("could not find rip directory in settings");
        return false;
    }

    working_directory = new QDir(dir_name);
    if (!working_directory->exists())
    {
        SendProblemEvent(QString("rip directory '%1' does not seem to exist")
                         .arg(dir_name));
        return false;
    }

#ifndef DEBUG_STAGE_2
    if (!working_directory->mkdir(rip_name))
    {
        SendProblemEvent(QString("Could not create directory called '%1/%2' ")
                         .arg(dir_name).arg(rip_name));
        return false;
    }
#endif

    if (!working_directory->cd(rip_name))
    {
        SendProblemEvent(QString("Could not cd into '%1/%2'")
                         .arg(dir_name).arg(rip_name));
        return false;
    }

#ifndef DEBUG_STAGE_2
    if (!working_directory->mkdir("vob"))
    {
        SendProblemEvent(QString("could not create a vob subdirectory "
                                 "in the working directory '%1/%2'")
                         .arg(dir_name).arg(rip_name));
        return false;
    }
#endif

    return true;
}

bool DVDTranscodeThread::buildTranscodeCommandLine(int which_run)
{
    //  If our destination file already exists, bail out
    QFile a_file(QString("%1.avi").arg(destination_file_string));
    if (a_file.exists())
    {
        SendProblemEvent("Transcode cannot run, "
                         "destination file already exists");
        return false;
    }

    tc_command = gCoreContext->GetSetting("TranscodeCommand");
    if (tc_command.isEmpty())
    {
        SendProblemEvent(
            "There is no TranscodeCommand setting for this system");
        return false;
    }

    tc_arguments.clear();

    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.prepare(
        "SELECT sync_mode, use_yv12,    "
        "   cliptop,     clipbottom,  clipleft,    clipright,   "
        "   f_resize_h,  f_resize_w,  hq_resize_h, hq_resize_w, "
        "   grow_h,      grow_w,                                "
        "   clip2top,    clip2bottom, clip2left,   clip2right,  "
        "   codec,       codec_param, "
        "   bitrate,     a_sample_r,  a_bitrate,   "
        "   input,       name,        two_pass,    tc_param     "
        "FROM dvdtranscode "
        "WHERE intid = :INTID");
    a_query.bindValue(":INTID", quality);

    if (!a_query.exec())
    {
        SendProblemEvent("buildTranscodeCommandLine query failed 1");
        return false;
    }

    if (!a_query.next())
    {
        SendProblemEvent("buildTranscodeCommandLine query null return 1");
        return false;
    }

    // Convert query results to named variables
    int       sync_mode = a_query.value(0).toInt();
    int         cliptop = a_query.value(2).toInt();
    int      clipbottom = a_query.value(3).toInt();
    int        clipleft = a_query.value(4).toInt();
    int       clipright = a_query.value(5).toInt();
    int      f_resize_h = a_query.value(6).toInt();
    int      f_resize_w = a_query.value(7).toInt();
    int     hq_resize_h = a_query.value(8).toInt();
    int     hq_resize_w = a_query.value(9).toInt();
    int          grow_h = a_query.value(10).toInt();
    int          grow_w = a_query.value(11).toInt();
    int        clipttop = a_query.value(12).toInt();
    int     cliptbottom = a_query.value(13).toInt();
    int       cliptleft = a_query.value(14).toInt();
    int      cliptright = a_query.value(15).toInt();
    QString       codec = a_query.value(16).toString();
    QString codec_param = a_query.value(17).toString();
    int         bitrate = a_query.value(18).toInt();
    int      a_sample_r = a_query.value(19).toInt();
    int       a_bitrate = a_query.value(20).toInt();
    int   input_setting = a_query.value(21).toInt();
    QString        name = a_query.value(22).toString();
    two_pass = a_query.value(23).toBool();
    QString    tc_param = a_query.value(24).toString();

    // And now, another query to get frame rate code and
    //input video dimensions from the dvdinput table
    a_query.prepare(
        "SELECT hsize, vsize, fr_code "
        "FROM dvdinput "
        "WHERE intid = :INTID");
    a_query.bindValue(":INTID", input_setting);

    if (!a_query.exec())
    {
        SendProblemEvent("buildTranscodeCommandLine query failed 2");
        return false;
    }

    if (!a_query.next())
    {
        SendProblemEvent("buildTranscodeCommandLine query null return 2");
        return false;
    }

    int input_hsize = a_query.value(0).toInt();
    int input_vsize = a_query.value(1).toInt();
    int fr_code = a_query.value(2).toInt();

    QProcess versionCheck;

    QString version;
    versionCheck.setReadChannelMode(QProcess::MergedChannels);
    versionCheck.start("transcode", QStringList() << "-v");
    versionCheck.waitForFinished();
    QByteArray result = versionCheck.readAll();
    QString resultString(result);

    if (resultString.contains("v1.0"))
        version = "1.0";
    else if (resultString.contains("v1.1"))
        version = "1.1";

    VERBOSE(VB_GENERAL, QString("Found Transcode Version: %1").arg(version));

    //  Check if we are doing subtitles
    if (subtitle_track > -1)
    {
        QString subtitle_arguments = QString("extsub=track=%1")
            .arg(subtitle_track);

        if (cliptbottom > 0)
        {
            subtitle_arguments.push_back(QString(":vershift=%1")
                                         .arg(cliptbottom));
        }
        tc_arguments.push_back("-x");
        tc_arguments.push_back("vob");
        tc_arguments.push_back("-J");

        tc_arguments.push_back(subtitle_arguments);
    }

    tc_arguments.push_back("-i");
    tc_arguments.push_back(QString("%1/vob/").arg(working_directory->path()));
    tc_arguments.push_back("-g");
    tc_arguments.push_back(QString("%1x%2").arg(input_hsize).arg(input_vsize));

    if (!gCoreContext->GetNumSetting("mythvideo.TrustTranscodeFRDetect"))
    {
        tc_arguments.push_back("-f");
        tc_arguments.push_back(QString("0,%1").arg(fr_code));
    }

    tc_arguments.push_back("-M");
    tc_arguments.push_back(QString("%1").arg(sync_mode));

    //  The order of these is defined by transcode
    if (clipbottom || cliptop || clipleft || clipright)
    {
        tc_arguments.push_back("-j");
        tc_arguments.push_back(
            QString("%1,%2,%3,%4")
            .arg(cliptop).arg(clipleft).arg(clipbottom).arg(clipright));
    }
    if (grow_h > 0 || grow_w > 0)
    {
        tc_arguments.push_back("-X");
        tc_arguments.push_back(QString("%1,%2").arg(grow_h).arg(grow_w));
    }
    if (f_resize_h > 0 || f_resize_w > 0)
    {
        tc_arguments.push_back("-B");
        tc_arguments.push_back(
            QString("%1,%2").arg(f_resize_h).arg(f_resize_w));
    }
    if (hq_resize_h > 0 && hq_resize_w > 0)
    {
        tc_arguments.push_back("-Z");
        tc_arguments.push_back(
            QString("%1x%2").arg(hq_resize_w).arg(hq_resize_h));
    }
    if (cliptbottom || clipttop || cliptleft || cliptright)
    {
        tc_arguments.push_back("-Y");
        tc_arguments.push_back(
            QString("%1,%2,%3,%4")
            .arg(clipttop).arg(cliptleft).arg(cliptbottom).arg(cliptright));
    }

    if (codec.isEmpty())
    {
        SendProblemEvent("Yo! Kaka-brain! Can't transcode without a codec");
        return false;
    }

    if (codec.contains("divx") && gCoreContext->GetNumSetting("MTDxvidFlag"))
    {
        codec = "xvid";
    }

    tc_arguments.push_back("-y");
    // in two pass, the audio from first pass is garbage
    if (two_pass && which_run == 1)
        tc_arguments.push_back(QString("%1,null").arg(codec));
    else
        tc_arguments.push_back(codec);

    if (codec_param.length())
    {
        tc_arguments.push_back("-F");
        tc_arguments.push_back(codec_param);
    }

    if (tc_param.length())
    {
        tc_arguments += tc_param.split(" ", QString::SkipEmptyParts);
    }

    if (bitrate > 0)
    {
        tc_arguments.push_back("-w");
        tc_arguments.push_back(QString("%1").arg(bitrate));
    }

    if (ac3_flag && !name.contains("VCD", Qt::CaseInsensitive))
    {
        tc_arguments.push_back("-A");
        tc_arguments.push_back("-N");
        tc_arguments.push_back("0x2000");
    }
    else
    {
        if (a_sample_r > 0)
        {
            tc_arguments.push_back("-E");
            tc_arguments.push_back(QString("%1").arg(a_sample_r));
        }
        if (a_bitrate > 0)
        {
            tc_arguments.push_back("-b");
            tc_arguments.push_back(QString("%1").arg(a_bitrate));
        }
    }
    if (audio_track > 1)
    {
        tc_arguments.push_back("-a");
        tc_arguments.push_back(QString("%1").arg(audio_track - 1));
    }

    tc_arguments.push_back("-o");

    // In a two pass encoding the video from the first run is garbage,
    // so redirect it to /dev/null
    tc_arguments.push_back(
        (two_pass && which_run == 1) ?
        QString("/dev/null") : QString("%1.avi").arg(destination_file_string));

    if (version == "1.0")
    {
        tc_arguments.push_back("--color");
        tc_arguments.push_back("0");
    }
    else
    {
        tc_arguments.push_back("--progress_rate"); 
        tc_arguments.push_back("20"); 
        tc_arguments.push_back("--log_no_color");
    }

    if (two_pass)
    {
        tc_arguments.push_back("-R");
        tc_arguments.push_back(QString("%1,twopass.log").arg(which_run));
    }

    // Produce shell safe version of command for user cut-n-paste
    QStringList args;
    for (int i = 0; i < tc_arguments.size(); i++)
        args.push_back(shell_escape(tc_arguments[i]));
    QString transcode_command_string =
        QString("transcode command will be: '%1 %2'")
        .arg(tc_command).arg(args.join(" "));

    SendLoggingEvent(transcode_command_string);

    return true;
}

bool DVDTranscodeThread::runTranscode(int which_run)
{
    // Set description strings to let the user
    // know what is going on.

    SetSubName(QObject::tr("Transcode is thinking..."), 1);
    SetSubProgress(0.0, 1);

    // Second pass?
    if ((which_run > 1) && !buildTranscodeCommandLine(which_run))
    {
        SendProblemEvent("Problem building second pass command line.");
        return false;
    }

    tc_process = new QProcess();
    tc_process->setWorkingDirectory(working_directory->path());
    tc_process->setProcessChannelMode(QProcess::MergedChannels);
    tc_process->setReadChannel(QProcess::StandardOutput);
    tc_process->start(tc_command, tc_arguments);

    if (!tc_process->waitForStarted())
    {
        SendProblemEvent("Could not start transcode");
        tc_process->kill();
        tc_process->waitForFinished();
        delete tc_process; // don't use deleteLater(), no event loop...
        tc_process = NULL;
        return false;
    }

    tc_current_pass    = which_run;
    tc_seconds_encoded = 0;
    tc_tick_tock       = 99;          // sentinel value, 99
    tc_job_start_time  = QDateTime(); // sentinel value, null date time

    {
        QMutexLocker locker(cancelLock);
        tc_timer = QObject::startTimer(0);
    }

    //VERBOSE(VB_IMPORTANT, "entering event loop");
    bool ok = (0 == DVDTranscodeThread::exec());
    //VERBOSE(VB_IMPORTANT, "exiting event loop");

    {
        QMutexLocker locker(cancelLock);
        killTimer(tc_timer);
        tc_timer = 0;
    }
    tc_current_pass = 0;

    return ok;
}

void DVDTranscodeThread::timerEvent(QTimerEvent *te)
{
    // Ignore timers we are not listening to...
    if (te->timerId() != tc_timer)
        return;

    // These vars should always be set when in event loop...
    if (!tc_process || !tc_current_pass)
        return;

    if (99 == tc_tick_tock)
    {
        tc_tick_tock = 3;
        QMutexLocker locker(cancelLock);
        killTimer(tc_timer);
        tc_timer = QObject::startTimer(kCheckFrequency);
    }

    if (IsCancelled())
    {
        SendProblemEvent("abandoned job because master control "
                         "said we need to shut down");

        tc_process->terminate();
        if (!tc_process->waitForFinished(3000))
            tc_process->kill();
        tc_process->waitForFinished();
        tc_process->deleteLater();
        tc_process = NULL;
        DVDTranscodeThread::exit(1);
        return;
    }

    if (QProcess::NotRunning == tc_process->state())
    {
        QString err = QString::null;
        if (QProcess::CrashExit == tc_process->exitStatus() ||
            tc_process->exitCode())
        {
            err = QString("Exiting runTranscode(%1) ")
                .arg(tc_current_pass);
            if (QProcess::CrashExit == tc_process->exitStatus())
            {
                err += QString("QProcess::error(): %1")
                    .arg(tc_process->error());
            }
            else
            {
                err += QString("%1 exit code: %2\n")
                    .arg(tc_command).arg(tc_process->exitCode());
                QByteArray err_out =
                    tc_process->readAllStandardError();
                if (err_out.size())
                {
                    err += "stderr output was:\n\n";
                    err += QString(err_out);
                }
            }
        }

        VERBOSE(VB_IMPORTANT, "Error: " + err);
        SendProblemEvent(err);

        tc_process->deleteLater();
        tc_process = NULL;
        DVDTranscodeThread::exit((err.isEmpty()) ? 0: 1);
        return;
    }

    QString status_line = QString::null;
    while (tc_process->canReadLine())
    {
        QByteArray stat_buf = tc_process->readLine();
        QString stat = QString(stat_buf);
        if (stat.contains("EMT:"))
            status_line = stat;
    }

    if (!status_line.isEmpty())
    {
        //QString orig_stat = status_line;
        status_line      = status_line.section("EMT: ", 1, 1);
        status_line      = status_line.section(",",     0, 0);
        QString h_string = status_line.section(":",     0, 0);
        QString m_string = status_line.section(":",     1, 1);
        QString s_string = status_line.section(":",     2, 2);

        tc_seconds_encoded = s_string.toUInt() +
            (60 * m_string.toUInt()) +
            (60 * 60 * h_string.toUInt());

        //VERBOSE(VB_IMPORTANT,
        //        QString("status_line: '%1' secs: %2")
        //        .arg(orig_stat).arg(tc_seconds_encoded));
    }
    else
    {
        //VERBOSE(VB_IMPORTANT,
        //        QString("no status_line, secs: %1")
        //        .arg(tc_seconds_encoded));
    }

    if (tc_seconds_encoded)
    {
        long long elapsed = 0;
        QDateTime now = QDateTime::currentDateTime();
        if (tc_job_start_time.isValid())
            elapsed = tc_job_start_time.secsTo(now);
        else
            tc_job_start_time = now;

        float percent_transcoded = tc_seconds_encoded * secs_mult;

        if (two_pass)
        {
            if (tc_current_pass == 1)
            {
                SetSubProgress(percent_transcoded, 1);
                overall_progress = 0.333333 +
                    (0.333333 * percent_transcoded);
                UpdateSubjobString(
                    elapsed, QObject::tr("Transcoding Pass 1 of 2 ~"));
            }
            else if (tc_current_pass == 2)
            {
                SetSubProgress(percent_transcoded, 1);
                overall_progress = 0.666666 +
                    (0.333333 * percent_transcoded);
                UpdateSubjobString(
                    elapsed, QObject::tr("Transcoding Pass 2 of 2 ~"));
            }
        }
        else
        {
            // Set feedback strings and calculate
            // estimated time left
            SetSubProgress(percent_transcoded, 1);
            overall_progress = 0.50 + (0.50 * percent_transcoded);
            UpdateSubjobString(elapsed, QObject::tr("Transcoding ~"));
        }
    }
    else
    {
        QString a_string = QObject::tr("Transcode is thinking");
        tc_tick_tock = (tc_tick_tock % 3) + 1;
        for (uint i = 0; i < 3; i++)
            a_string += (i < tc_tick_tock) ? " ." : "   ";

        SetSubName(a_string, 1);
    }
}

/// \brief Erase rip file(s) and temporary working directory
void DVDTranscodeThread::cleanUp(void)
{
    if (!working_directory)
        return;

    if (two_pass)
        working_directory->remove("twopass.log");

    working_directory->rmdir("vob");
    working_directory->cd("..");
    working_directory->rmdir(rip_name);
    delete working_directory;
    working_directory = NULL;
    SetSubProgress(1, 1);
    SetSubName(QObject::tr("Transcode complete."), 1);
}

/** \brief Clean up and remove any output files that may have been
 *         partially created.
 */
void DVDTranscodeThread::wipeClean(void)
{
    cleanUp();
    QDir::current().remove(QString("%1.avi").arg(destination_file_string));
}

DVDTranscodeThread::~DVDTranscodeThread(void)
{
    if (working_directory)
        delete working_directory;
    if (tc_process)
        delete tc_process;
}
