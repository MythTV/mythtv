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
using namespace std;

#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QWaitCondition>
#include <Q3Process>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/compat.h>

#include "jobthread.h"
#include "mtd.h"
#include "threadevents.h"
#include "dvdprobe.h"

namespace {
    struct delete_file {
        bool operator()(const QString &filename)
        {
            VERBOSE(VB_GENERAL, QString("Deleting file: %1").arg(filename));
            return QDir::current().remove(filename);
        }
    };
}

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
            if (!(m_mutex && m_mutex->locked()))
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

    std::vector<unsigned char> video_data(1024 * DVD_VIDEO_LB_LEN);

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
    nice(nice_level);
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
    nice(nice_level);

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
    tc_process(NULL),
    two_pass(false),
    audio_track(which_audio),
    length_in_seconds((numb_seconds) ? numb_seconds : 1),
    ac3_flag(do_ac3),
    subtitle_track(subtitle_track_numb)
{
}

void DVDTranscodeThread::run(void)
{
    nice(nice_level);

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

    if (!gContext->GetNumSetting("mythdvd.mtd.SaveTranscodeIntermediates", 0))
    {
        // remove any temporary titles that are now transcoded
        std::for_each(output_files.begin(), output_files.end(), delete_file());
    }

    cleanUp();
}

bool DVDTranscodeThread::makeWorkingDirectory(void)
{
    QString dir_name = gContext->GetSetting("DVDRipLocation");
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
    if (!working_directory->mkdir(rip_name))
    {
        SendProblemEvent(QString("could not create directory called '%1' "
                                 "in rip directory").arg(rip_name));
        return false;
    }
    if (!working_directory->cd(rip_name))
    {
        SendProblemEvent(QString("could not cd into '%1'").arg(rip_name));
        return false;
    }
    if (!working_directory->mkdir("vob"))
    {
        SendProblemEvent(QString("could not create a vob subdirectory "
                                 "in the working directory '%1'")
                         .arg(rip_name));
        return false;
    }

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

    QString tc_command = gContext->GetSetting("TranscodeCommand");
    if (tc_command.isEmpty())
    {
        SendProblemEvent(
            "There is no TranscodeCommand setting for this system");
        return false;
    }

    tc_arguments.clear();
    tc_arguments.push_back(tc_command);

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
    bool       use_yv12 = a_query.value(1).toBool();
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

    if (!gContext->GetNumSetting("mythvideo.TrustTranscodeFRDetect"))
    {
        tc_arguments.push_back("-f");
        tc_arguments.push_back(QString("0,%1").arg(fr_code));
    }

    tc_arguments.push_back("-M");
    tc_arguments.push_back(QString("%1").arg(sync_mode));

    if (use_yv12)
    {
        tc_arguments.push_back("-V");
    }

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

    if (codec.contains("divx") && gContext->GetNumSetting("MTDxvidFlag"))
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

    // in two pass, the video from the first run is garbage
    if (two_pass && which_run == 1)
        tc_arguments.push_back(QString("/dev/null"));
    else
        tc_arguments.push_back(QString("%1.avi").arg(destination_file_string));

    tc_arguments.push_back("--print_status");
    tc_arguments.push_back("20");
    tc_arguments.push_back("--color");
    tc_arguments.push_back("0");

    if (two_pass)
    {
        tc_arguments.push_back("-R");
        tc_arguments.push_back(QString("%1,twopass.log").arg(which_run));
    }

    QString transcode_command_string =
        QString("transcode command will be: '%1'").arg(tc_arguments.join(" "));
    SendLoggingEvent(transcode_command_string);

    tc_process = new Q3Process(tc_arguments);
    tc_process->setWorkingDirectory(*working_directory);
    return true;
}

bool DVDTranscodeThread::runTranscode(int which_run)
{
    // Set description strings to let the user
    // know what is going on.

    SetSubName("Transcode is thinking ...", 1);
    SetSubProgress(0.0, 1);
    uint tick_tock = 3;
    uint seconds_so_far = 0;
    double percent_transcoded = 0.0;
    QTime job_time;
    bool finally = true;

    if (which_run > 1)
    {
        //  Second pass
        if (tc_process)
        {
            delete tc_process;
            tc_process = NULL;
        }

        if (!buildTranscodeCommandLine(which_run))
        {
            SendProblemEvent("Problem building second pass command line.");
            return false;
        }
    }

    if (!tc_process->start())
    {
        SendProblemEvent("Could not start transcode");
        return false;
    }

    while (true)
    {
        if (IsCancelled())
        {
            SendProblemEvent("abandoned job because master control "
                             "said we need to shut down");

            tc_process->tryTerminate();
            sleep(3);
            tc_process->kill();
            delete tc_process;
            tc_process = NULL;
            return false;
        }

        if (!tc_process->isRunning())
        {
            bool flag = tc_process->normalExit();
            delete tc_process;
            tc_process = NULL;
            return flag;
        }

        while (tc_process->canReadLineStdout())
        {
            //  Would be nice to just connect a SIGNAL here
            //  rather than polling, but we're talking to a
            //  a process from inside a thread, so Q_OBJECT
            //  is not possible

            QString status_line = tc_process->readLineStdout();
            status_line = status_line.section("EMT: ", 1, 1);
            status_line = status_line.section(",",0,0);
            QString h_string = status_line.section(":",0,0);
            QString m_string = status_line.section(":",1,1);
            QString s_string = status_line.section(":", 2,2);

            seconds_so_far = s_string.toUInt() +
            (60 * m_string.toUInt()) +
            (60 * 60 * h_string.toUInt());

            percent_transcoded = (double)
            ( (double) seconds_so_far / (double) length_in_seconds);
        }

        if (seconds_so_far > 0)
        {
            if (finally)
            {
                finally = false;
                job_time.start();
            }
            if (two_pass)
            {
                if (which_run == 1)
                {
                    SetSubProgress(percent_transcoded, 1);
                    overall_progress = 0.333333 +
                        (0.333333 * percent_transcoded);
                    UpdateSubjobString(job_time.elapsed() / 1000,
                                       QObject::tr(
                                           "Transcoding Pass 1 of 2 ~"));
                }
                else if (which_run == 2)
                {
                    SetSubProgress(percent_transcoded, 1);
                    overall_progress = 0.666666 +
                        (0.333333 * percent_transcoded);
                    UpdateSubjobString(job_time.elapsed() / 1000,
                                       QObject::tr(
                                           "Transcoding Pass 2 of 2 ~"));
                }
            }
            else
            {
                // Set feedback strings and calculate
                // estimated time left
                SetSubProgress(percent_transcoded, 1);
                overall_progress = 0.50 + (0.50 * percent_transcoded);
                UpdateSubjobString(job_time.elapsed() / 1000,
                                   QObject::tr("Transcoding ~"));
            }
        }
        else
        {
            tick_tock = ((tick_tock) % 3) + 1;

            QString a_string = QObject::tr("Transcode is thinking ");
            for (uint i = 0; i < tick_tock; i++)
                a_string += ".";

            SetSubName(a_string, 1);
        }

        QMutexLocker locker(cancelLock);
        if (!cancel_me)
            cancelWaitCond->wait(cancelLock, 2000);
    }

    // We should never get here, but cleanup anyway
    delete tc_process;
    tc_process = NULL;
    return false;
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
