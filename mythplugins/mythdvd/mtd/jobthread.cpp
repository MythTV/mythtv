/*
	jobthread.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Where things actually happen

*/

#include <iostream>
using namespace std;

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

#include <qdatetime.h>
#include <qdir.h>
#include <qapplication.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "jobthread.h"
#include "mtd.h"
#include "threadevents.h"

JobThread::JobThread(MTD *owner, const QString &start_string, int nice_priority)
          :QThread()
{
    problem_string = "";
    job_name = "";
    setSubName("", 1);
    overall_progress = 0.0;
    setSubProgress(0.0, 1);
    parent = owner;
    job_string = start_string;
    nice_level = nice_priority;
    sub_to_overall_multiple = 1.0;
    cancel_me = false;
}

void JobThread::run()
{
    cerr << "jobthread.o: Somebody ran an actual (base class) JobThread. I don't think that's supposed to happen." << endl;
}

bool JobThread::keepGoing()
{
    if(parent->threadsShouldContinue() && ! cancel_me)
    {
        return true;
    }
    return false;
}

void JobThread::updateSubjobString( int seconds_elapsed, 
                                    const QString & pre_string)
{
    int estimated_job_time = 0;
    if(subjob_progress > 0.0)
    {
        estimated_job_time = (int) ( (double) seconds_elapsed / subjob_progress );
    }
    else
    {
        estimated_job_time = (int) ( (double) seconds_elapsed / 0.000001 );
    }
        
    QString new_name = "";    
    if(estimated_job_time >= 3600)
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
    setSubName(new_name, 1);
}

void JobThread::setSubProgress(double some_value, uint priority)
{
    if(priority > 0)
    {
        while(!sub_progress_mutex.tryLock())
        {
            sleep(priority);
        }
        if(!cancel_me)
        {
            subjob_progress = some_value;
        }
    }
    else
    {
        sub_progress_mutex.lock();
        if(!cancel_me)
        {
            subjob_progress = some_value;
        }
    }
    sub_progress_mutex.unlock();
}

void JobThread::setSubName(const QString &new_name, uint priority)
{
    if(priority > 0)
    {
        while(!sub_name_mutex.tryLock())
        {
            sleep(1);
        }
        if(!cancel_me)
        {
            subjob_name = new_name;
        }
    }
    else
    {
        sub_name_mutex.lock();
        if(!cancel_me)
        {
            subjob_name = new_name;
        }
    }
    sub_name_mutex.unlock();
    
}

void JobThread::problem(const QString &a_problem)
{
    //
    //  Send out an error event
    //

    ErrorEvent *ee = new ErrorEvent(a_problem);
    QApplication::postEvent(parent, ee);
    
    problem_string = a_problem;
    
}


void JobThread::sendLoggingEvent(const QString &event_string)
{
    //
    //  Spit out an event that contains the
    //  logging string
    //
    
    LoggingEvent *le = new LoggingEvent(event_string);
    QApplication::postEvent(parent, le);
}


/*
---------------------------------------------------------------------
*/

DVDThread::DVDThread(MTD *owner, 
                     QMutex *drive_mutex, 
                     const QString &dvd_device, 
                     int track, 
                     const QString &dest_file, 
                     const QString &name,
                     const QString &start_string,
                     int nice_priority)
                 :JobThread(owner, start_string, nice_priority)
{
    dvd_device_access = drive_mutex;
    dvd_device_location = dvd_device;
    dvd_title = track - 1;
    destination_file_string = dest_file;
    ripfile = NULL;
    rip_name = name;
}

void DVDThread::run()
{
    cerr << "jobthread.o: Somebody ran an actual (base class) DVDThread. I don't think that's supposed to happen." << endl;
}


bool DVDThread::ripTitle(int title_number,
                         const QString &to_location,
                         const QString &extension,
                         bool multiple_files)
{
    //
    //  Can't do much until I have a
    //  lock on the device
    //

    bool loop = true;
    
    setSubName(QObject::tr("Waiting For Access to DVD"), 1);

    while(loop)
    {
        if(dvd_device_access->tryLock())
        {
            loop = false;
        }
        else
        {
            if(keepGoing())
            {
                sleep(5);
            }
            else
            {
                problem("abandoned job because master control said we need to shut down");
                return false;
            }
        }
    }    

    if(!keepGoing())
    {
        problem("abandoned job because master control said we need to shut down");
        dvd_device_access->unlock();
        return false;
    }

    sendLoggingEvent("job thread beginning to rip dvd title");

    
    //
    //  OK, we got the device. 
    //  Lets open our destination file
    //
    
    if(ripfile)
    {
        cerr << "jobthread.o: How is it that you already have a ripfile set?" << endl;
        delete ripfile;
        ripfile = NULL;
    }
    ripfile = new RipFile(to_location, extension);
    if(!ripfile->open(IO_WriteOnly | IO_Raw | IO_Truncate, multiple_files))
    {
        problem(QString("DVDPerfectThread could not open output file: %1").arg(ripfile->name()));
        dvd_device_access->unlock();
        return false;
    }

    //
    //  Time to open up access with all 
    //  the funky structs from libdvdnav
    //
    
    int angle = 0;  // Change that at some point

    the_dvd = DVDOpen(dvd_device_location);
    if(!the_dvd)
    {
        problem(QString("DVDPerfectThread could not access this dvd device: %1").arg(dvd_device_location));
        ripfile->remove();
        delete ripfile;
        ripfile = NULL;
        dvd_device_access->unlock();
        return false;
    }


    ifo_handle_t *vmg_file = ifoOpen( the_dvd, 0);
    if(!vmg_file)
    {
        problem("DVDPerfectThread could not open VMG info.");
        ripfile->remove();
        delete ripfile;
        ripfile = NULL;
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return false;
    }
    tt_srpt_t *tt_srpt = vmg_file->tt_srpt;

    //
    //  Check title # is valid
    //

    if(title_number < 0 || title_number > tt_srpt->nr_of_srpts )
    {
        problem(QString("DVDPerfectThread could not open title number %1").arg(title_number + 1));
        ripfile->remove();
        delete ripfile;
        ripfile = NULL;
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return false;
    }

    ifo_handle_t *vts_file = ifoOpen(the_dvd, tt_srpt->title[title_number].title_set_nr);
    if(!vts_file)
    {
        problem("DVDPerfectThread could not open the title's info file");
        ripfile->remove();
        delete ripfile;
        ripfile = NULL;
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return false;
    }

    //
    //  Determine the program chain (?)
    //
    int ttn = tt_srpt->title[title_number].vts_ttn;
    vts_ptt_srpt_t *vts_ptt_srpt = vts_file->vts_ptt_srpt;
    int pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[ 0 ].pgcn;
    int pgn = vts_ptt_srpt->title[ ttn - 1 ].ptt[ 0 ].pgn;
    pgc_t *cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    int start_cell = cur_pgc->program_map[ pgn - 1 ] - 1;


    //
    //  Hmmmm ... need some sort of total to calculate
    //  progress display against .... I guess disc
    //  sectors will have to do .... 
    //

    int total_sectors = 0;

    for(int i = start_cell; i < cur_pgc->nr_of_cells; i++)
    {
        total_sectors += cur_pgc->cell_playback[i].last_sector -
                         cur_pgc->cell_playback[i].first_sector;
    }

    //
    //  OK ... now actually open
    //    
    
    title = DVDOpenFile(the_dvd, 
                        tt_srpt->title[ title_number ].title_set_nr, 
                        DVD_READ_TITLE_VOBS );
    if(!title)
    {
        problem("DVDPerfectThread could not open the title's actual VOB(s)");
        ripfile->remove();
        delete ripfile;
        ripfile = NULL;
        ifoClose(vts_file);
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return false;
    }
    
    int sector_counter = 0;

    QTime job_time;
    job_time.start();
    
    int next_cell = start_cell;    
    for(int cur_cell = start_cell; next_cell < cur_pgc->nr_of_cells; )
    {
        cur_cell = next_cell;
        if(cur_pgc->cell_playback[ cur_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK)
        {
            
            cur_cell += angle;
            for(int i=0;; ++i)
            {
                if(cur_pgc->cell_playback[ cur_cell + i ].block_mode == BLOCK_MODE_LAST_CELL)
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

         //
         // Loop until we're out of this cell
         //
         
         for(uint cur_pack = cur_pgc->cell_playback[ cur_cell ].first_sector;
                  cur_pack < cur_pgc->cell_playback[ cur_cell ].last_sector; )
         {
            dsi_t dsi_pack;
            unsigned int next_vobu, next_ilvu_start, cur_output_size;
                        
            //
            //  Read the NAV packet.
            //            
            
            int len = DVDReadBlocks( title, (int) cur_pack, 1, video_data );
            if( len != 1)
            {
                problem(QString("DVDPerfectThread read failed for block %1").arg(cur_pack));
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDCloseFile( title );
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                ripfile->remove();
                delete ripfile;
                ripfile = NULL;
                return false;
            }
            
            //
            //  libdvdread example code has an assertion here
            //
            //        assert( is_nav_pack( video_data ) );
            
            //
            //  Parse the contained dsi packet
            //
            
            navRead_DSI(&dsi_pack, &(video_data[ DSI_START_BYTE ]) );
            //  and another assertion here: assert( cur_pack == dsi_pack.dsi_gi.nv_pck_lbn );
            
            //
            //  Figure out where to go next
            //
            
            next_ilvu_start = cur_pack+ dsi_pack.sml_agli.data[ angle ].address;
            cur_output_size = dsi_pack.dsi_gi.vobu_ea;

            if(dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL )
            {
                next_vobu = cur_pack + ( dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
            }
            else
            {
                next_vobu = cur_pack + cur_output_size + 1;
            }            
            
            //  another assert:  assert( cur_output_size < 1024 );
            cur_pack++;
            sector_counter++;

            //
            //  Read in and output cursize packs
            //
            
            len = DVDReadBlocks( title, (int)cur_pack, cur_output_size, video_data );
            if( len != (int) cur_output_size )
            {
                problem(QString("DVDPerfectThread read failed for %1 blocks at %2") 
                        .arg(cur_output_size)
                        .arg(cur_pack)
                       );
                ripfile->remove();
                delete ripfile;
                ripfile = NULL;
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDCloseFile( title );
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                return false;
            }
            
            setSubProgress((double) (sector_counter) / (double) (total_sectors), 1);
            overall_progress = subjob_progress * sub_to_overall_multiple;
            updateSubjobString(job_time.elapsed() / 1000, 
                               QObject::tr("Ripping to file ~"));
            if(!ripfile->writeBlocks(video_data, cur_output_size * DVD_VIDEO_LB_LEN))
            {
                problem("Couldn't write blocks during a rip. Filesystem size exceeded? Disc full?");
                ripfile->remove();
                delete ripfile;
                ripfile = NULL;
                DVDCloseFile( title );
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                return false;
            }

            sector_counter += next_vobu - cur_pack;
            cur_pack = next_vobu;


            //
            //  Escape out and clean up if mtd main thread
            //  tells us to
            //
            
            if(!keepGoing())
            {
                problem("abandoned job because master control said we need to shut down");
                ripfile->remove();
                delete ripfile;
                ripfile = NULL;
                DVDCloseFile( title );
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                return false;
            }
        }
    }

    //
    //  Wow, we're done.
    //

    ifoClose(vts_file);
    ifoClose(vmg_file);
    DVDCloseFile( title );
    DVDClose(the_dvd);
    ripfile->close();
    dvd_device_access->unlock();
    sendLoggingEvent("job thread finished ripping dvd title");
    return true;
}

DVDThread::~DVDThread()
{
    if(ripfile);
    {
        delete ripfile;
    }
}

/*
---------------------------------------------------------------------
*/

DVDPerfectThread::DVDPerfectThread(MTD *owner, 
                                   QMutex *drive_mutex, 
                                   const QString &dvd_device, 
                                   int track, 
                                   const QString &dest_file, 
                                   const QString &name,
                                   const QString &start_string,
                                   int nice_priority)
                 :DVDThread(owner,
                            drive_mutex, 
                            dvd_device, 
                            track, 
                            dest_file, 
                            name,
                            start_string,
                            nice_priority)

{
}

void DVDPerfectThread::run()
{
    //
    //  Be nice
    //
    
    nice(nice_level);
    job_name = QString(QObject::tr("Perfect DVD Rip of %1")).arg(rip_name);
    if(keepGoing())
    {
        ripTitle(dvd_title, destination_file_string, ".vob", true);
    }
}


DVDPerfectThread::~DVDPerfectThread()
{
}

/*
---------------------------------------------------------------------
*/

DVDTranscodeThread::DVDTranscodeThread(MTD *owner, 
                                       QMutex *drive_mutex, 
                                       const QString &dvd_device, 
                                       int track, 
                                       const QString &dest_file, 
                                       const QString &name,
                                       const QString &start_string,
                                       int nice_priority,
                                       int quality_level,
                                       bool do_ac3,
                                       int which_audio,
                                       int numb_seconds,
                                       int subtitle_track_numb)
                 :DVDThread(owner,
                            drive_mutex, 
                            dvd_device, 
                            track, 
                            dest_file, 
                            name,
                            start_string,
                            nice_priority)

{
    quality = quality_level;
    ac3_flag = do_ac3;
    working_directory = NULL;
    tc_process = NULL;
    two_pass = false;
    audio_track = which_audio;
    length_in_seconds = numb_seconds;
    if(length_in_seconds == 0)
    {
        length_in_seconds = 1;
    }
    subtitle_track = subtitle_track_numb;
    used_transcode_slot = false;
}

void DVDTranscodeThread::run()
{
    //
    //  Be nice
    //
    
    nice(nice_level);

    //
    //  Make working directory
    //

    if(keepGoing())
    {
        if(!makeWorkingDirectory())
        {
            return;
        }    
    }

    //
    //  Build the transcode command line. We do this early
    //  (before ripping) so we can figure out if this is
    //  a two pass job or not (for the progress display)
    //

    if(keepGoing())
    {
        if(!buildTranscodeCommandLine(1))
        {
            cleanUp();
            return;
        }
        if(two_pass)
        {
            job_name = QString(QObject::tr("Transcode of %1")).arg(rip_name);
            sub_to_overall_multiple = 0.333333333;
        }
        else
        {
            job_name = QString(QObject::tr("Transcode of %1")).arg(rip_name);
            sub_to_overall_multiple = 0.50;
        }
    }

    //
    //  Rip VOB to working directory
    //

    if(keepGoing())
    {
        QString rip_file_string = QString("%1/vob/%2").arg(working_directory->path()).arg(rip_name);
        if(!ripTitle(dvd_title, rip_file_string, ".vob", true))
        {
            cleanUp(); 
            return;
        }
    }
    
    //
    //  Ask if we can start transcoding
    //  (ask mtd about # of concurrent 
    //  transcoding jobs)
    //
    
    setSubName(QObject::tr("Waiting for Permission to Start Transcoding"), 1);
    
    bool loop = true;
    
    while(loop)
    {
        if(parent->isItOkToStartTranscoding() && keepGoing())
        {
            used_transcode_slot = true;
            loop = false;
        }
        else
        {
            if(keepGoing())
            {
                sleep(5);
            }
            else
            {
                problem("abandonded job because master control said we need to shut down");
                return;
            }
        }
    }
    
    

    //
    //  Run the first (only?) crack at transcoding
    //    
    
    if(keepGoing())
    {
        if(!runTranscode(1))
        {
            wipeClean();
            return;
        }
    }
    
    if(two_pass)
    {
        if(keepGoing())
        {
            if(!runTranscode(2))
            {
                wipeClean();
            }
        }
    }    
    cleanUp();
}

bool DVDTranscodeThread::makeWorkingDirectory()
{
    QString dir_name = gContext->GetSetting("DVDRipLocation");
    if( dir_name.length() < 1)
    {
        problem("could not find rip directory in settings");
        return false;
    }
    
    working_directory = new QDir (dir_name);
    if(!working_directory->exists())
    {
        problem("rip directory does not seem to exist");
        return false;
    }
    if(!working_directory->mkdir(rip_name))
    {
        problem(QString("could not create directory called \"%1\" in rip directory").arg(rip_name));
        return false;
    }
    if(!working_directory->cd(rip_name))
    {
        problem(QString("could not cd into \"%1\"").arg(rip_name));
        return false;
    }
    if(!working_directory->mkdir("vob"))
    {
        problem("could not create a vob subdirectory in the working directory");
        return false;
    }
    
    return true;
}

bool DVDTranscodeThread::buildTranscodeCommandLine(int which_run)
{
    //
    //  If our destination file already exists, bail out
    //
    
    QFile a_file(QString("%1.avi").arg(destination_file_string));
    if(a_file.exists())
    {
        problem("transcode cannot run, destination file already exists");
        return false;
    }
    
    
    
    QString tc_command = gContext->GetSetting("TranscodeCommand");
    if(tc_command.length() < 1)
    {
        problem("there is no TranscodeCommand setting for this system");
        return false;
    }
    
    tc_arguments.clear();
    tc_arguments.append(tc_command);
    
    
    //
    //  Now *that* is a query string !
    //
    
    QString q_string = QString("SELECT sync_mode,   "
                                     " use_yv12,    "
                                     " cliptop,     "
                                     " clipbottom,  "
                                     " clipleft,    "
                                     " clipright,   "
                                     " f_resize_h,  "
                                     " f_resize_w,  "
                                     " hq_resize_h, "
                                     " hq_resize_w, "
                                     " grow_h,      "
                                     " grow_w,      "
                                     " clip2top,    "
                                     " clip2bottom, "
                                     " clip2left,   "
                                     " clip2right,  "
                                     " codec,       "
                                     " codec_param, "
                                     " bitrate,     "
                                     " a_sample_r,  "
                                     " a_bitrate,   "
                                     " input,       "
                                     " name,        "
                                     " two_pass     "
                                     
                                     " FROM dvdtranscode WHERE intid = %1 ;")
                                     .arg(quality);
                                     
    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.exec(q_string);

    if(a_query.isActive() && a_query.size() > 0)
    {
        a_query.next();
    }
    else
    {
        problem(QString("sql query failed: %1").arg(q_string));
        return false;
    }       
    
    //
    //  Convert query results to useful variables
    //                           
    
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
    
    //
    //  And now, another query to get frame rate code and
    //  input video dimensions from the dvdinput table
    //
    
    q_string = QString("SELECT hsize, vsize, fr_code FROM dvdinput WHERE intid = %1 ;").arg(input_setting);
    a_query.exec(q_string);

    if(a_query.isActive() && a_query.size() > 0)
    {
        a_query.next();
    }
    else
    {
        problem(QString("couldn't get dvdinput tuple for an intid of %1").arg(input_setting));
        return false;
    }

    int input_hsize = a_query.value(0).toInt();
    int input_vsize = a_query.value(1).toInt();
    int fr_code = a_query.value(2).toInt();
    
    //
    //  Check if we are doing subtitles
    //
    
    if(subtitle_track > -1)
    {
    
        QString subtitle_arguments = QString("extsub=track=%1")
                                            .arg(subtitle_track);
        if(cliptbottom > 0)
        {
            subtitle_arguments.append(QString(":vershift=%1")
                                             .arg(cliptbottom));
        }
        tc_arguments.append("-x");
        tc_arguments.append("vob");
        tc_arguments.append("-J");
        
        tc_arguments.append(subtitle_arguments);
    }
    
    
    tc_arguments.append("-i");
    tc_arguments.append(QString("%1/vob/").arg(working_directory->path()));
    tc_arguments.append("-g");
    tc_arguments.append(QString("%1x%2").arg(input_hsize).arg(input_vsize));
    tc_arguments.append("-f");
    tc_arguments.append(QString("0,%1").arg(fr_code));
                         
                         
                         
    tc_arguments.append("-M");
    tc_arguments.append(QString("%1").arg(sync_mode));
    
    if(use_yv12)
    {
        tc_arguments.append("-V");
    }    

    //
    //  The order of these is defined by transcode
    //
    if(clipbottom != 0 ||
       cliptop != 0 ||
       clipleft != 0 ||
       clipright != 0)
    {
        tc_arguments.append("-j");
        tc_arguments.append(QString("%1,%2,%3,%4")
                             .arg(cliptop)
                             .arg(clipleft)
                             .arg(clipbottom)
                             .arg(clipright));
    }
    if(grow_h > 0 || grow_w > 0)
    {
        tc_arguments.append("-X");
        tc_arguments.append(QString("%1,%2")
                             .arg(grow_h)
                             .arg(grow_w));
    }
    if(f_resize_h > 0 || f_resize_w > 0)
    {
        tc_arguments.append("-B");
        tc_arguments.append(QString("%1,%2")
                             .arg(f_resize_h)
                             .arg(f_resize_w));
    }
    if(hq_resize_h > 0 && hq_resize_w > 0)
    {
        tc_arguments.append("-Z");
        tc_arguments.append(QString("%1x%2")
                             .arg(hq_resize_w)
                             .arg(hq_resize_h));
    }
    if(cliptbottom != 0 ||
       clipttop != 0 ||
       cliptleft != 0 ||
       cliptright != 0)
    {
        tc_arguments.append("-Y");
        tc_arguments.append(QString("%1,%2,%3,%4")
                             .arg(clipttop)
                             .arg(cliptleft)
                             .arg(cliptbottom)
                             .arg(cliptright));
    }
    
    if(codec.length() < 1)
    {
        problem("Yo! Kaka-brain! Can't transcode without a codec");
        return false;
    }
    if(codec.contains("divx") && gContext->GetNumSetting("MTDxvidFlag"))
    {
        codec = "xvid";
    }
    
    tc_arguments.append("-y");
    // in two pass, the audio from first pass is garbage
    if (two_pass && which_run == 1)
        tc_arguments.append(QString("%1,null").arg(codec));
    else
        tc_arguments.append(codec);

    if(codec_param.length() > 0)
    {
        tc_arguments.append("-F");
        tc_arguments.append(codec_param);
    }
    
    if(bitrate > 0)
    {
        tc_arguments.append("-w");
        tc_arguments.append(QString("%1").arg(bitrate));
    }
    
    
    if(ac3_flag && !name.contains("VCD", false))
    {
        tc_arguments.append("-A");
        tc_arguments.append("-N");
        tc_arguments.append("0x2000");
    }
    else
    {
        if(a_sample_r > 0)
        {
            tc_arguments.append("-E");
            tc_arguments.append(QString("%1").arg(a_sample_r));
        }
        if(a_bitrate > 0)
        {
            tc_arguments.append("-b");
            tc_arguments.append(QString("%1").arg(a_bitrate));
        }
    }
    if(audio_track > 1)
    {
        tc_arguments.append("-a");
        tc_arguments.append(QString("%1").arg(audio_track - 1));
    }

    tc_arguments.append("-o");
    // in two pass, the video from the first run is garbage
    if (two_pass && which_run == 1)
        tc_arguments.append(QString("/dev/null"));
    else
        tc_arguments.append(QString("%1.avi").arg(destination_file_string));

    tc_arguments.append("--print_status");
    tc_arguments.append("20");
    tc_arguments.append("--color");
    tc_arguments.append("0");
    

    if(two_pass)
    {
        tc_arguments.append("-R");
        tc_arguments.append(QString("%1,twopass.log").arg(which_run));
    }    
    

    QString transcode_command_string = "transcode command will be: " + tc_arguments.join(" ");
    sendLoggingEvent(transcode_command_string);

    tc_process = new QProcess(tc_arguments);
    tc_process->setWorkingDirectory(*working_directory);
    return true;
}

bool DVDTranscodeThread::runTranscode(int which_run)
{
    //
    //  Set description strings to let the user 
    //  know what is going on.
    //

    setSubName("Transcode is thinking ...", 1);
    setSubProgress(0.0, 1);
    uint tick_tock = 3;
    uint seconds_so_far = 0;
    double percent_transcoded = 0.0;
    QTime job_time;
    bool finally = true;
    
    if(which_run > 1)
    {
        //
        //  Second pass
        //
        
        if(tc_process)
        {
            delete tc_process;
            tc_process = NULL;
        }

        if (!buildTranscodeCommandLine(which_run))        
        {
            problem( "Problem building second pass command line." );
            return false;
        }
    }

    //  Debugging
    //
    //cout << "About to run \"" << tc_process->arguments().join(" ") << "\"" << endl;
    //cout << "with workdir = " << tc_process->workingDirectory().path() << endl;

    if(!tc_process->start())
    {
        problem("Could not start transcode");
        return false;
    }
    while(true)
    {
        if(!keepGoing())
        {
            //
            //  oh darn ... the mtd wants to shut down
            //

            problem("was transcoding, but the mtd shut me down");
            tc_process->tryTerminate();
            sleep(3);
            tc_process->kill();
            delete tc_process;
            tc_process = NULL;
            return FALSE;
        }
        if(tc_process->isRunning())
        {
            while(tc_process->canReadLineStdout())
            {
                //
                //  Would be nice to just connect a SIGNAL here
                //  rather than polling, but we're talking to a
                //  a process from inside a thread, so Q_OBJECT
                //  is not possible
                //

                QString status_line = tc_process->readLineStdout();
                status_line = status_line.section("EMT: ", 1, 1);
                status_line = status_line.section(",",0,0);
                QString h_string = status_line.section(":",0,0);
                QString m_string = status_line.section(":",1,1);
                QString s_string = status_line.section(":", 2,2);

                seconds_so_far = s_string.toUInt() +
                                 (60 * m_string.toUInt()) +
                                 (60 * 60 * h_string.toUInt());
                                      
                percent_transcoded = (double) ( (double) seconds_so_far / (double) length_in_seconds);
            }
            if(seconds_so_far > 0)
            {
                if(finally)
                {
                    finally = false;
                    job_time.start();
                }
                if(two_pass)
                {
                    if(which_run == 1)
                    {
                        setSubProgress(percent_transcoded, 1);
                        overall_progress = 0.333333 + (0.333333 * percent_transcoded);
                        updateSubjobString(job_time.elapsed() / 1000, 
                                           QObject::tr("Transcoding Pass 1 of 2 ~"));
                    }
                    else if(which_run == 2)
                    {
                        setSubProgress(percent_transcoded, 1);
                        overall_progress = 0.666666 + (0.333333 * percent_transcoded);
                        updateSubjobString(job_time.elapsed() / 1000, 
                                           QObject::tr("Transcoding Pass 2 of 2 ~"));
                    }
                }
                else
                {
                    //
                    //  Set feedback strings and calculate
                    //  estimated time left
                    //
                    
                    setSubProgress(percent_transcoded, 1);
                    overall_progress = 0.50 + (0.50 * percent_transcoded);
                    updateSubjobString(job_time.elapsed() / 1000, 
                                       QObject::tr("Transcoding ~"));
                }
            }
            else
            {
                ++tick_tock;
                if (tick_tock > 3)
                {
                    tick_tock = 1;
                }
                QString a_string = QObject::tr("Transcode is thinking ");
                for(uint i = 0; i < tick_tock; i++)
                {
                    a_string += ".";
                }
                setSubName(a_string, 1);
            }
            sleep(2);
        }
        else
        {
            bool flag = tc_process->normalExit();
            delete tc_process;
            tc_process = NULL;
            return flag;
        }
    }

    //
    //  Should never get here
    //

    delete tc_process;
    tc_process = NULL;
    return false;
}

void DVDTranscodeThread::cleanUp()
{
    //
    //  Erase rip file(s) and temporary
    //  working directory
    //
    if(working_directory)
    {
        if(ripfile)
        {
            ripfile->remove();
            delete ripfile;
            ripfile = NULL;
        }
            
        if(two_pass)
        {
            working_directory->remove("twopass.log");
        }
        working_directory->rmdir("vob");
        working_directory->cd("..");
        working_directory->rmdir(rip_name);
        delete working_directory;
        working_directory = NULL;
    }
}

void DVDTranscodeThread::wipeClean()
{
    //
    //  Clean up and remove any output
    //  files that may have been
    //  partially created
    //
    cleanUp();
    QDir d("stupid");
    d.remove(QString("%1.avi").arg(destination_file_string));
}

DVDTranscodeThread::~DVDTranscodeThread()
{
    if(working_directory)
    {
        delete working_directory;
    }
    if(tc_process)
    {
        delete tc_process;
    }
    if(ripfile)
    {
        delete ripfile;
    }
}

