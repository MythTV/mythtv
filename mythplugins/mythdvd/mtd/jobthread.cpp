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

#include "jobthread.h"
#include "mtd.h"


JobThread::JobThread(MTD *owner, const QString &start_string, int nice_priority)
          :QThread()
{
    job_name = "";
    subjob_name = "";
    overall_progress = 0.0;
    subjob_progress = 0.0;
    parent = owner;
    problem_string = "";
    job_string = start_string;
    nice_level = nice_priority;
}

void JobThread::run()
{
    cerr << "jobthread.o: Somebody ran an actual (base class) JobThread. I don't think that's supposed to happen." << endl;
}

bool JobThread::keepGoing()
{
    if(parent->threadsShouldContinue())
    {
        return true;
    }
    return false;
}


DVDPerfectThread::DVDPerfectThread(MTD *owner, 
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
    job_name = QString("Perfect DVD Rip of %1").arg(name);
    dvd_device_location = dvd_device;
    dvd_title = track - 1;
    ripfile = new RipFile(dest_file, ".mpg");
}

void DVDPerfectThread::run()
{
    //
    //  Be nice
    //
    
    nice(nice_level);

    //
    //  Can't do much until I have a
    //  lock on the device
    //

    bool loop = true;
    
    subjob_name = "Waiting For Access to DVD";

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
                return;
            }
        }
    }    

    subjob_name = "Ripping to destination file";
    
    int overall_size; 
    int amount_read;   
    
    //
    //  OK, we got the device. 
    //  Lets open our destination file
    //
    
    if(!ripfile->open(IO_WriteOnly | IO_Raw | IO_Truncate))
    {
        problem(QString("DVDPerfectThread could not open output file: %1").arg(ripfile->name()));
        dvd_device_access->unlock();
        return;
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
        dvd_device_access->unlock();
        return;
    }


    ifo_handle_t *vmg_file = ifoOpen( the_dvd, 0);
    if(!vmg_file)
    {
        problem("DVDPerfectThread could not open VMG info.");
        ripfile->remove();
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return;
    }
    tt_srpt_t *tt_srpt = vmg_file->tt_srpt;

    //
    //  Check title # is valid
    //

    if(dvd_title < 0 || dvd_title > tt_srpt->nr_of_srpts )
    {
        problem(QString("DVDPerfectThread could not open title number %1").arg(dvd_title + 1));
        ripfile->remove();
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return;
    }

    ifo_handle_t *vts_file = ifoOpen(the_dvd, tt_srpt->title[dvd_title].title_set_nr);
    if(!vts_file)
    {
        problem("DVDPerfectThread could not open the title's info file");
        ripfile->remove();
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return;
    }

    //
    //  Determine the program chain (?)
    //
    int ttn = tt_srpt->title[dvd_title].vts_ttn;
    vts_ptt_srpt_t *vts_ptt_srpt = vts_file->vts_ptt_srpt;
    int pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[ 0 ].pgcn;
    int pgn = vts_ptt_srpt->title[ ttn - 1 ].ptt[ 0 ].pgn;
    pgc_t *cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    int start_cell = cur_pgc->program_map[ pgn - 1 ] - 1;


    //
    //  OK ... now actually open
    //    
    
    title = DVDOpenFile(the_dvd, 
                        tt_srpt->title[ dvd_title ].title_set_nr, 
                        DVD_READ_TITLE_VOBS );
    if(!title)
    {
        problem("DVDPerfectThread could not open the title's actual VOB(s)");
        ripfile->remove();
        ifoClose(vts_file);
        ifoClose(vmg_file);
        DVDClose(the_dvd);
        dvd_device_access->unlock();
        return;
    }
    
    overall_size = DVDFileSize(title);
    amount_read = 0;

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
                return;
            }
            
            ++amount_read;
            
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
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDCloseFile( title );
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                return;
            }
            
            amount_read += len;


            overall_progress = (double) (amount_read) / (double) (overall_size);
            subjob_progress = overall_progress;
            
            // write(ripfile->handle(), video_data, cur_output_size * DVD_VIDEO_LB_LEN);

            ripfile->writeBlocks(video_data, cur_output_size * DVD_VIDEO_LB_LEN);

            cur_pack = next_vobu;

            //
            //  Escape out and clean up if mtd main thread
            //  tells us to
            //
            
            if(!keepGoing())
            {
                problem("abandoned job because master control said we need to shut down");
                ripfile->remove();
                ifoClose(vts_file);
                ifoClose(vmg_file);
                DVDCloseFile( title );
                DVDClose(the_dvd);
                dvd_device_access->unlock();
                return;
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
    return;
}

DVDPerfectThread::~DVDPerfectThread()
{
    if(ripfile);
    {
        delete ripfile;
    }
}
