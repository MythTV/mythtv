#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <map>
#include <qstring.h>
#include "mythcontext.h"
#include "channelbase.h"

using namespace std;

class TVRec;

typedef vector<int> vector_int;

class DVBChannel : public ChannelBase
{
  public:
    DVBChannel(TVRec *parent, int cardnum, // DVB hw device num, see below
               bool use_ts, char dvb_type);
    virtual ~DVBChannel();

    virtual bool Open();
    virtual void Close();
    virtual void GetPID(vector_int& pid) const;

    virtual bool SetChannelByString(const QString &chan); // chan is channum?

    virtual void SetFreqTable(const QString &name);

    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
                      { (void)newcapchannel; (void)setstarting; }

    // Empty functions that we don't need
    virtual void SetFormat(const QString &format) { (void)format; }
    virtual void ToggleInputs() {}

    enum DVB_Type {DVB_S, DVB_T, DVB_C};

  protected:
    virtual bool Open(unsigned int number_of_pids);
    virtual bool SetPID();

    vector_int pid; /* Program IDs, to filter out the right channel from
                       the transport stream we get from the tuner/card.
                       Video and audio stream, respectively. */
    bool use_ts; /* If true, grab the whole transport stream (TS) from the card
                    and do the filtering/demuxing ourselves. This allows us to
                    record several channels from the same TS at the same time,
                    by reading the TS several times in different DVBRecorder
                    instances. Full (not budget) cards with MPEG decoder
                    don't give us the full TS, so we have to let the card
                    filter out the right program streams, and they usually
                    can do that for one channel at the same time only.
                    So, set this to false for full cards and true for budget
                    cards, while the latter ironically gives more
                    functionality. */

    enum DVB_Type dvb_type;
    int cardnum; /* 0..3; for N in /dev/dvb/adapterN/frontend0,
                                   /dev/dvb/adapterN/dvr0 etc. */
    int dvr_fd;
    vector_int demux_fd;
};

#endif
