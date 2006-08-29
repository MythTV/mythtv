#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// C++ headers
#include <vector>

// Qt headers
#include <qmap.h>
#include <qstring.h>
#include <qsqldatabase.h>

// MythTV headers
#include "channelutil.h"
#include "frequencies.h"
#include "tv.h"

class TVRec;

typedef std::pair<uint,uint> pid_cache_item_t;
typedef std::vector<pid_cache_item_t> pid_cache_t;

class InputBase
{
  public:
    InputBase() :
        name(QString::null),            startChanNum(QString::null),
        tuneToChannel(QString::null),   externalChanger(QString::null),
        sourceid(0),                    cardid(0),
        inputNumV4L(-1),
        videoModeV4L1(0),               videoModeV4L2(0) {}

    InputBase(QString _name,            QString _startChanNum,
              QString _tuneToChannel,   QString _externalChanger,
              uint    _sourceid,        uint    _cardid,
              const DBChanList &_channels) :
        name(_name),                    startChanNum(_startChanNum),
        tuneToChannel(_tuneToChannel),  externalChanger(_externalChanger),
        sourceid(_sourceid),            cardid(_cardid),
        channels(_channels),
        inputNumV4L(-1),
        videoModeV4L1(0),               videoModeV4L2(0) {}

    virtual ~InputBase() {}

  public:
    QString name;            // input name
    QString startChanNum;    // channel to start on
    QString tuneToChannel;   // for using a cable box & S-Video/Composite
    QString externalChanger; // for using a cable box...
    uint    sourceid;        // associated channel listings source
    uint    cardid;          // input card id
    DBChanList channels;
    int     inputNumV4L;
    int     videoModeV4L1;
    int     videoModeV4L2;
};
typedef QMap<uint, InputBase*> InputMap;

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
    virtual bool SetChannelByString(const QString &chan) = 0;
    /// \brief Reports whether channel is already open
    virtual bool IsOpen(void) const = 0;

    // Methods that one might want to specialize
    /// \brief Sets file descriptor.
    virtual void SetFd(int fd) { (void)fd; };
    /// \brief Returns file descriptor, -1 if it does not exist.
    virtual int GetFd(void) const { return -1; };

    // Gets
    virtual uint GetNextChannel(uint chanid, int direction) const;
    virtual uint GetNextChannel(const QString &channum, int direction) const;
    virtual int GetInputByName(const QString &input) const;
    virtual QString GetInputByNum(int capchannel) const;
    virtual QString GetCurrentName(void) const
        { return curchannelname; }
    virtual int GetChanID(void) const;
    virtual int GetCurrentInputNum(void) const
        { return currentInputID; }
    virtual QString GetCurrentInput(void) const
        { return inputs[GetCurrentInputNum()]->name; }
    virtual int GetNextInputNum(void) const;
    virtual QString GetNextInput(void) const
        { return inputs[GetNextInputNum()]->name; }
    virtual QString GetNextInputStartChan(void)
        { return inputs[GetNextInputNum()]->startChanNum; }
    virtual uint GetCurrentSourceID(void) const
        { return inputs[GetCurrentInputNum()]->sourceid; }
    virtual uint GetInputCardID(int inputNum) const;
    virtual DBChanList GetChannels(int inputNum) const;
    virtual DBChanList GetChannels(const QString &inputname) const;
    virtual QStringList GetConnectedInputs(void) const;
    /// \brief Returns true iff commercial detection is not required
    //         on current channel, for BBC, CBC, etc.
    bool IsCommercialFree(void) const { return commfree; }
    /// \brief Returns String representing device, useful for debugging
    virtual QString GetDevice(void) const { return ""; }

    // Sets
    virtual void Renumber(uint srcid, const QString &oldChanNum,
                          const QString &newChanNum);

    // Channel setting convenience method
    virtual bool SetChannelByDirection(ChannelChangeDirection);

    // Input toggling convenience methods
    virtual bool SwitchToInput(const QString &input);
    virtual bool SwitchToInput(const QString &input, const QString &chan);

    virtual bool InitializeInputs(void);

    // Misc. Commands
    virtual bool Retune(void) { return false; }

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void)
        { StoreInputChannels(inputs); }

    // Picture attribute settings
    virtual bool InitPictureAttributes(void) { return false; }
    virtual int  GetPictureAttribute(const QString) const { return 0; }
    virtual int  ChangePictureAttribute(int,const QString,bool) { return 0; }

    bool CheckChannel(const QString &channum, QString& inputName) const;

    // MPEG stuff
    /** \brief Returns cached MPEG PIDs for last tuned channel.
     *  \param pid_cache List of PIDs with their TableID
     *                   types is returned in pid_cache. */
    virtual void GetCachedPids(pid_cache_t &pid_cache) const
        { (void) pid_cache; }
    /// \brief Saves MPEG PIDs to cache to database
    /// \param pid_cache List of PIDs with their TableID types to be saved.
    virtual void SaveCachedPids(const pid_cache_t &pid_cache) const
        { (void) pid_cache; }
    /// \brief Returns program number in PAT, -1 if unknown.
    virtual int GetProgramNumber(void) const
        { return currentProgramNum; };
    /// \brief Returns major channel, 0 if unknown.
    virtual uint GetMajorChannel(void) const
        { return currentATSCMajorChannel; };
    /// \brief Returns minor channel, 0 if unknown.
    virtual uint GetMinorChannel(void) const
        { return currentATSCMinorChannel; };
    /// \brief Returns DVB original_network_id, 0 if unknown.
    virtual uint GetOriginalNetworkID(void) const
        { return currentOriginalNetworkID; };
    /// \brief Returns DVB transport_stream_id, 0 if unknown.
    virtual uint GetTransportID(void) const
        { return currentTransportID; };

    // \brief Set cardid for scanning
    void SetCardID(uint _cardid) { cardid = _cardid; }

  protected:
    /// \brief Switches to another input on hardware, 
    ///        and sets the channel is setstarting is true.
    virtual bool SwitchToInput(int newcapchannel, bool setstarting) = 0;

    virtual int GetCardID(void) const;
    virtual bool ChangeExternalChannel(const QString &newchan);
    virtual void SetCachedATSCInfo(const QString &chan);
    static void GetCachedPids(int chanid, pid_cache_t&);
    static void SaveCachedPids(int chanid, const pid_cache_t&);
    static void StoreInputChannels(const InputMap&);

    TVRec   *pParent;
    QString  curchannelname;
    int      currentInputID;
    bool     commfree;
    uint     cardid;
    InputMap inputs;
    DBChanList allchannels; ///< channels across all inputs

    int     currentProgramNum;
    uint    currentATSCMajorChannel;
    uint    currentATSCMinorChannel;
    uint    currentTransportID;
    uint    currentOriginalNetworkID;
};

#endif

