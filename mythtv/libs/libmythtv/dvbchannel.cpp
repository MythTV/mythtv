#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <qsqldatabase.h>

#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "mythcontext.h"
#include "RingBuffer.h"
#include "tv_rec.h"

#ifdef USING_DVB
    #include "dvbdev.h"
#endif

using namespace std;

DVBChannel::DVBChannel(TVRec *parent, int aCardnum,
                       bool a_use_ts, char a_dvb_type)
  : ChannelBase(parent),
    use_ts(a_use_ts),
    cardnum(aCardnum)
{
    if (a_dvb_type == 's' || a_dvb_type == 'S')
      dvb_type = DVB_S;
    else if (a_dvb_type == 't' || a_dvb_type == 'T')
      dvb_type = DVB_T;
    else if (a_dvb_type == 'c' || a_dvb_type == 'C')
      dvb_type = DVB_C;
    else
    {
      cerr <<  "invalid DVB card type " << a_dvb_type << endl;
      dvb_type = DVB_S;
    }
}

DVBChannel::~DVBChannel()
{
    Close();
}

bool DVBChannel::Open()
{
    return Open(pid.size());
}

bool DVBChannel::Open(unsigned int npids)
{
    if (use_ts && demux_fd.size() > 0 ||
        demux_fd.size() >= npids)
        return true;

#ifdef USING_DVB
    /* demux stuff only - the tune() function from the lib does the opening
       of the dvr device by itself */

    /* See use_ts declaration in dvbchannel.h.
       If we have a budget card capable, fetch the whole TS
       (special PID 8192) and leave the channel filtering
       (pids) to ts_to_ps() later.
       If we have a full card, it doesn't give us the TS, so we let it do the
       channel filtering in hardware, implying that we can only record one
       channel per card (not n channels on one TS per card). */

    if (use_ts)
        npids = 1;
    for (unsigned int i = demux_fd.size(); i < npids; i++)
    {
        demux_fd.push_back(open(devicenodename(dvbdev_demux, cardnum), O_RDWR));
        if (demux_fd.back() < 0)
        {
            cerr <<  "open of demux device (" << i << ") failed" << endl;
            return false;
        }
    }
    cout << "opened DVB demux devices" << endl;

    if (use_ts)
    {
        set_ts_filt(demux_fd[0], 8192, DMX_PES_OTHER);
          /* 8192 is the magic PID to tell the hardware/driver not to filter,
             but to give us the full TS. */
        cout << "set demux device for TS" << endl;
    }
    // else wait for SetChannelByString() to use SetPIDs()

    return true;
#else
    cerr << "DVB support not compiled in" << endl;
    return false;
#endif
}

void DVBChannel::Close()
{
    for (vector_int::iterator i = pid.begin(); i != pid.end(); i++)
        if (*i > 0)
            close(*i);
    cout << "closed DVB demux devices" << endl;
}

bool DVBChannel::SetPID()
{
    cout << "SetPID:";
    for (vector_int::iterator j = pid.begin(); j != pid.end(); j++)
        cout << " " << *j;
    cout << endl;

    if (!Open(pid.size()))
        return false;

    // Set filters in device (hardware or driver)
    if (!use_ts)
    {
#ifdef USING_DVB
        // we already made sure using Open() that demux_fd.size() == pid.size()
        if (demux_fd.size() != pid.size())
            cerr << "aaarggg! sizes don't match" <<endl;
        for (unsigned int i = 0; i < pid.size(); i++)
          {
            cout << "demux" << i << "=" << demux_fd[i] << " pid " << pid[i] << endl;
            set_ts_filt(demux_fd[i], pid[i], DMX_PES_OTHER);
            /* if we used DMX_PES_VIDEO/AUDIO, it would allow the full cards
               to decode, but we don't need that anyways. */
          }
#endif
    }
    // else we did that in Open() already

    cout << "set demux devices" << endl;

    // Notify DVBRecorder (which does the filtering) of the PID change
    DVBRecorder* rec = dynamic_cast<DVBRecorder*>(pParent->GetRecorder());
    if (rec)
    {
        cout << "got recorder" << endl;
        rec->SetPID(pid);
        cout << "notifed recorder" << endl;
    }
    // else ignore error, e.g. when there is no recorder yet

    return true;
}

void DVBChannel::GetPID(vector_int& some_pid) const
{
    some_pid = pid;
}

void DVBChannel::SetFreqTable(const QString &name)
{
    // Might be used for DVB-S vs. -C vs. -T UK vs. -T FI
}

#ifdef USING_DVB

struct DVBTunerSettings
{
    // all
    int freq;
    // DVB-S
    bool pol_v;
    int symbol_rate;
    unsigned int diseqc;
    int tone;
    // DVB-T
    fe_spectral_inversion_t inversion;
    fe_bandwidth_t bandwidth;
    fe_code_rate_t hp_code_rate;
    fe_code_rate_t lp_code_rate;
    fe_modulation_t modulation; // constellation
    fe_transmit_mode_t transmission_mode;
    fe_guard_interval_t guard_interval;
    fe_hierarchy_t hierarchy;
    // PIDs for TS (not stricting tuning, but convient)
    vector_int pid;
};

/* Parse the settings from the DB.

   This is a lot of boring stuff (mostly for DVB-T) to convert the
   text entries to the enums that the driver expects, I have to hardcode
   all possible values here.
   I don't see how this could be done more elegantly.

   @param tuningList  Settings from DB
   @param s  Result, i.e. values extracted from |options|
   @result succeeded, i.e. all mandatory values could be read
*/
bool FetchDVBTuningOptions(QSqlDatabase* db_conn, pthread_mutex_t db_lock,
                           DVBChannel::DVB_Type dvb_type,
                           const QString& chan,
                           /*out*/ DVBTunerSettings& s)
{
    // all
    s.freq = 0;
    // DVB-S
    s.pol_v = true;
    s.symbol_rate = 0;
    s.diseqc = 0;
    s.tone = -1; // what is that and what the heck does -1 mean?
    // DVB-T
    s.inversion = INVERSION_AUTO;
    s.bandwidth = BANDWIDTH_AUTO;
    s.hp_code_rate = FEC_AUTO;
    s.lp_code_rate = FEC_NONE; // unused with hierarchy none
    s.modulation = QAM_AUTO; // constellation
    s.transmission_mode = TRANSMISSION_MODE_AUTO;
    s.guard_interval = GUARD_INTERVAL_AUTO;
    s.hierarchy = HIERARCHY_NONE; // neither UK nor FI use that
    s.pid.clear();

    // Query DB
    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    /* First find out the chanid. This step should go away when we use chanids
       instead of channums everywhere to identify channels. */
    QString thequery = QString("SELECT chanid "
                               " FROM channel WHERE channum = \"%1\";")
                               .arg(chan);
    QSqlQuery query = db_conn->exec(thequery);
    if (!query.isActive())
        MythContext::DBError("fetchtuningparamschanid", query);
    if (query.numRowsAffected() <= 0)
    {
        cerr << "didn't find channel" << endl;
        pthread_mutex_unlock(&db_lock);
        return false;
    }
    query.next();
    int chanid = query.value(0).toInt();

    thequery = QString("SELECT freq, pol, symbol_rate,diseqc, tone, "
                  " inversion, bandwidth, hp_code_rate, lp_code_rate, "
                  " modulation, transmission_mode, guard_interval, hierarchy, "
                  " pids "
                  " FROM channel_dvb WHERE chanid = %1;")
                  .arg(chanid);

    query = db_conn->exec(thequery);
    if (!query.isActive())
        MythContext::DBError("fetchtuningparams", query);
    if (query.numRowsAffected() <= 0)
    {
        cerr << "didn't find dvb tuning parameters for channel" << endl;
        pthread_mutex_unlock(&db_lock);
        return false;
    }
    query.next();
    pthread_mutex_unlock(&db_lock);

    // all
    s.freq = query.value(0).toInt();
    if (s.freq == 0)
        return false;

    QString option;
    if (dvb_type == DVBChannel::DVB_S)
    {
        int opt_int = query.value(1).toInt();
        if (opt_int == 'V' || opt_int == 'v')
            s.pol_v = true;
        else if (opt_int == 'H' || opt_int == 'h')
            s.pol_v = false;
        else
            return false;

        s.symbol_rate = query.value(2).toInt();
        if (s.symbol_rate == 0)
            return false;

        s.tone = query.value(3).toInt();
        s.diseqc = query.value(4).toInt();
    }
    else if (dvb_type == DVBChannel::DVB_T)
    {
        /* All DVB-T settings are optional in the channel settings,
           they are usually the same all over the country and we'll use
           cardcoded defaults (see above),
           maybe make them configureable per card.
           The driver defines all the settings as enums, so I have to hardcode
           all possible value here *sigh* */

        if ((option = query.value(5).toString().lower()) != QString::null)
        {
            if (option == "1" || option == "on"
                     || option == "yes") s.inversion = INVERSION_ON;
            else if (option == "0" || option == "off"
                     || option == "no") s.inversion = INVERSION_OFF;
            else if (option == "auto") s.inversion = INVERSION_AUTO;
            else cerr << "invalid inversion option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(6).toString().lower()) != QString::null)
        {
            if (option == "6") s.bandwidth = BANDWIDTH_6_MHZ;
            else if (option == "7") s.bandwidth = BANDWIDTH_7_MHZ;
            else if (option == "8") s.bandwidth = BANDWIDTH_8_MHZ;
            else if (option == "auto") s.bandwidth = BANDWIDTH_AUTO;
            else cerr << "invalid bandwidth option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(7).toString().lower()) != QString::null)
        {
            if (option == "0" || option == "off"
                || option == "no" || option == "none")
                                      s.hp_code_rate = FEC_NONE;
            else if (option == "1_2") s.hp_code_rate = FEC_1_2;
            else if (option == "2_3") s.hp_code_rate = FEC_2_3;
            else if (option == "3_4") s.hp_code_rate = FEC_3_4;
            else if (option == "4_5") s.hp_code_rate = FEC_4_5;
            else if (option == "5_6") s.hp_code_rate = FEC_5_6;
            else if (option == "6_7") s.hp_code_rate = FEC_6_7;
            else if (option == "8_9") s.hp_code_rate = FEC_8_9;
            else if (option == "auto") s.hp_code_rate = FEC_AUTO;
            else cerr << "invalid hp code rate option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(8).toString().lower()) != QString::null)
        {
            if (option == "0" || option == "off"
                || option == "no" || option == "none")
                                      s.lp_code_rate = FEC_NONE;
            else if (option == "1_2") s.lp_code_rate = FEC_1_2;
            else if (option == "2_3") s.lp_code_rate = FEC_2_3;
            else if (option == "3_4") s.lp_code_rate = FEC_3_4;
            else if (option == "4_5") s.lp_code_rate = FEC_4_5;
            else if (option == "5_6") s.lp_code_rate = FEC_5_6;
            else if (option == "6_7") s.lp_code_rate = FEC_6_7;
            else if (option == "8_9") s.lp_code_rate = FEC_8_9;
            else if (option == "auto") s.lp_code_rate = FEC_AUTO;
            else cerr << "invalid lp code rate option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(9).toString().lower()) != QString::null)
        {
            if (option == "qpsk") s.modulation = QPSK;
            else if (option == "qam_16") s.modulation = QAM_16;
            else if (option == "qam_32") s.modulation = QAM_32;
            else if (option == "qam_64") s.modulation = QAM_64;
            else if (option == "qam_128") s.modulation = QAM_128;
            else if (option == "qam_256") s.modulation = QAM_256;
            else if (option == "qam_auto") s.modulation = QAM_AUTO;
            else cerr << "invalid modulation option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(10).toString().lower()) != QString::null)
        {
            if (option == "2") s.transmission_mode = TRANSMISSION_MODE_2K;
            else if (option == "8") s.transmission_mode = TRANSMISSION_MODE_8K;
            else if (option == "auto") s.transmission_mode
                                                      = TRANSMISSION_MODE_AUTO;
            else cerr << "invalid transmission mode option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(11).toString().lower()) != QString::null)
        {
            if (option == "4") s.guard_interval = GUARD_INTERVAL_1_4;
            else if (option == "8") s.guard_interval = GUARD_INTERVAL_1_8;
            else if (option == "16") s.guard_interval = GUARD_INTERVAL_1_16;
            else if (option == "32") s.guard_interval = GUARD_INTERVAL_1_32;
            else if (option == "auto") s.guard_interval = GUARD_INTERVAL_AUTO;
            else cerr << "invalid guard interval option " << option
                      << " for channel " << chan << endl;
        }
        if ((option = query.value(12).toString().lower()) != QString::null)
        {
            if (option == "0" || option == "off"
                || option == "no" || option == "none")
                                    s.hierarchy = HIERARCHY_NONE;
            else if (option == "1") s.hierarchy = HIERARCHY_1;
            else if (option == "2") s.hierarchy = HIERARCHY_2;
            else if (option == "4") s.hierarchy = HIERARCHY_4;
            else if (option == "auto") s.hierarchy = HIERARCHY_AUTO;
            else cerr << "invalid hierarchy option " << option
                      << " for channel " << chan << endl;
        }
    }

    // PIDs
    option = query.value(13).toString();
    if (option.isEmpty())
        return false; // mandatory
    QStringList option_list = QStringList::split(",", option);
    for (unsigned int i = 0; i != option_list.size(); i++)
        s.pid.push_back(option_list[i].toInt());

    return true;
}

// Do the real tuning
bool DVBTune(const DVBTunerSettings& s, int cardnum)
{
    // Open
    int fd_frontend = open(devicenodename(dvbdev_frontend, cardnum), O_RDWR);
    if(fd_frontend < 0)
    {
        cerr << "Opening of DVB frontend device failed";
        return -1;
    }
    int fd_sec = 0;
#ifdef OLDSTRUCT
    fd_sec = open(devicenodename(dvbdev_sec, cardnum), O_RDWR);
    if(fd_sec < 0)
    {
        cerr << "Opening of DVB sec device failed";
        return -1;
    }
#endif
    cout << "trying to change to freq " << s.freq << endl;

    // tune; from tune.h/c
    int err = tune_it(fd_frontend, fd_sec,
                      s.freq, s.symbol_rate, s.pol_v?'v':'h',
                      s.tone, s.inversion, s.diseqc, s.modulation,
                      s.hp_code_rate, s.lp_code_rate, s.transmission_mode,
                      s.guard_interval, s.bandwidth, s.hierarchy);

    // close
    if (fd_frontend > 0)
        close(fd_frontend);
#ifdef OLDSTRUCT
    if (fd_sec > 0)
        close(fd_sec);
#endif

    // return
    if (err < 0)
    {
        cerr << "tuning failed" << endl;
        return false;
    }
    cout << "tuning succeeded" << endl;

    return true;
}

#endif // USING_DVB

bool DVBChannel::SetChannelByString(const QString &chan)
{
    if (curchannelname == chan)
        return true;
    cout << "trying to change to channel " << chan << endl;

#ifdef USING_DVB
    QSqlDatabase* db_conn;
    pthread_mutex_t db_lock;
    if (!pParent->CheckChannel(this, chan, db_conn, db_lock))
        return false;
    cout << "channel exists" << endl;

    DVBTunerSettings tunerSettings;

    if (!FetchDVBTuningOptions(db_conn, db_lock, dvb_type, chan,
                               /*out*/ tunerSettings))
        return false;
    cout << "got tuning parameters" << endl;

    if (!DVBTune(tunerSettings, cardnum))
        return false;

    pid = tunerSettings.pid;
    if (!SetPID())
        return false;

    cout << "successfully changed channel" << endl;

    curchannelname = chan;
    inputChannel[currentcapchannel] = curchannelname;

    return true;
#else
    cerr << "DVB support not compiled in" << endl;
    return false;
#endif
}
