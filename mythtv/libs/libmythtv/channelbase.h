#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// C++ headers
#include <vector>

// Qt headers
#include <qmap.h>
#include <qstring.h>
#include <qsqldatabase.h>

// MythTV headers

#include "frequencies.h"
#include "tv.h"

class TVRec;

typedef std::pair<uint,uint> pid_cache_item_t;
typedef std::vector<pid_cache_item_t> pid_cache_t;

/** \class ChannelBase
 *  \brief Abstract class providing a generic interface to tuning hardware.
 *
 *   This class abstracts channel implementations for analog TV, ATSC, DVB, etc.
 *   Also implements many generic functions needed by most derived classes.
 *   It is responsible for tuning, i.e. switching channels.
 */

class ChannelBase
{
 public:
    ChannelBase(TVRec *parent);
    virtual ~ChannelBase();

    // Methods that must be implemented.
    /// \brief Opens the channel changing hardware for use.
    virtual bool Open(void) = 0;
    /// \brief Closes the channel changing hardware to use.
    virtual void Close(void) = 0;
    /// \brief Switches to another input on hardware, 
    ///        and sets the channel is setstarting is true.
    virtual bool SwitchToInput(int newcapchannel, bool setstarting) = 0;
    virtual bool SetChannelByString(const QString &chan) = 0;
    /// \brief Reports whether channel is already open
    virtual bool IsOpen(void) const = 0;

    // Methods that one might want to specialize
    /// \brief Sets file descriptor.
    virtual void SetFd(int fd) { (void)fd; };
    /// \brief Returns file descriptor, -1 if it does not exist.
    virtual int GetFd(void) const { return -1; };

    // Gets
    virtual int GetInputByName(const QString &input) const;
    virtual QString GetInputByNum(int capchannel) const;
    virtual QString GetCurrentName(void) const
        { return curchannelname; }
    virtual int GetCurrentInputNum(void) const
        { return currentcapchannel; }
    virtual QString GetCurrentInput(void) const
        { return channelnames[GetCurrentInputNum()]; }
    virtual QString GetOrdering(void) const
        { return channelorder; }
    /// \brief Returns true iff commercial detection is not required
    //         on current channel, for BBC, CBC, etc.
    bool IsCommercialFree(void) const { return commfree; }
    /// \brief Returns String representing device, useful for debugging
    virtual QString GetDevice(void) const { return ""; }

    // Sets
    virtual void SetChannelOrdering(const QString &chanorder)
        { channelorder = chanorder; }

    // Channel setting convenience method
    virtual bool SetChannelByDirection(ChannelChangeDirection);

    // Input toggling convenience methods
    virtual bool ToggleInputs(void);
    virtual bool SwitchToInput(const QString &input);
    virtual bool SwitchToInput(const QString &input, const QString &chan);

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void);

    // Picture attribute settings
    virtual void SetBrightness(void) {};
    virtual void SetColour(void) {};
    virtual void SetContrast(void) {};
    virtual void SetHue(void) {};
    virtual int  ChangeBrightness(bool up) { (void)up; return 0; };
    virtual int  ChangeColour(bool up)     { (void)up; return 0; };
    virtual int  ChangeContrast(bool up)   { (void)up; return 0; };
    virtual int  ChangeHue(bool up)        { (void)up; return 0; };

    // MPEG stuff
    /** \brief Returns cached MPEG PIDs for last tuned channel.
     *  \param pid_cache List of PIDs with their TableID
     *                   types is returned in pid_cache. */
    virtual void GetCachedPids(pid_cache_t&) const { ; };
    /// \brief Saves MPEG PIDs to cache to database
    /// \param pid_cache List of PIDs with their TableID types to be saved.
    virtual void SaveCachedPids(const pid_cache_t&) const { ; };
    /// \brief Returns program number in PAT, -1 if unknown.
    virtual int GetProgramNumber(void) const { return currentProgramNum; };
    /// \brief Returns major channel, -1 if unknown.
    virtual int GetMajorChannel(void) const { return currentATSCMajorChannel; };
    /// \brief Returns minor channel, -1 if unknown.
    virtual int GetMinorChannel(void) const { return currentATSCMinorChannel; };

  protected:
    virtual bool ChangeExternalChannel(const QString &newchan);
    virtual void SetCachedATSCInfo(const QString &chan);
    static void GetCachedPids(int chanid, pid_cache_t&);
    static void SaveCachedPids(int chanid, const pid_cache_t&);

    TVRec  *pParent;
    QString channelorder;
    QString curchannelname;
    int     capchannels;
    int     currentcapchannel;
    bool    commfree;

    int     currentATSCMajorChannel;
    int     currentATSCMinorChannel;
    int     currentProgramNum;

    QMap<int, QString> channelnames;
    QMap<int, QString> inputChannel;
    QMap<int, QString> inputTuneTo;
    QMap<int, QString> externalChanger;
    QMap<int, QString> sourceid;
};

#endif
