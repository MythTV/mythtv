// -*- Mode: c++ -*-
#ifndef _INPUTINFO_H_
#define _INPUTINFO_H_

// Qt headers
#include <QStringList>
#include <QMap>

// MythTV headers
#include "channelinfo.h" // for ChannelInfoList

class MTV_PUBLIC InputInfo
{
  public:
    InputInfo() : name(QString::null),
                  sourceid(0), inputid(0), cardid(0), mplexid(0),
                  chanid(0), recPriority(0), scheduleOrder(0),
                  livetvorder(0), quickTune(false) {}
    InputInfo(const QString &name,
              uint sourceid, uint inputid, uint cardid, uint mplexid, 
              uint chanid, uint livetvorder);

    InputInfo(const InputInfo &other) :
        name(other.name),
        sourceid(other.sourceid),
        inputid(other.inputid),
        cardid(other.cardid),
        mplexid(other.mplexid),
        chanid(other.chanid),
        displayName(other.displayName),
        recPriority(other.recPriority),
        scheduleOrder(other.scheduleOrder),
        livetvorder(other.livetvorder),
        quickTune(other.quickTune)
    {
        name.detach();
    }

    InputInfo &operator=(const InputInfo &other)
    {
        name     = other.name;
        name.detach();
        sourceid = other.sourceid;
        inputid  = other.inputid;
        cardid   = other.cardid;
        mplexid  = other.mplexid;
        chanid   = other.chanid;
        displayName = other.displayName;
        recPriority = other.recPriority;
        scheduleOrder = other.scheduleOrder;
        livetvorder = other.livetvorder;
        quickTune = other.quickTune;
        return *this;
    }

    bool operator == (uint _inputid) const
        { return inputid == _inputid; }

    bool operator == (const QString &_name) const
        { return name == _name; }

    virtual ~InputInfo() {}

    virtual bool FromStringList(QStringList::const_iterator &it,
                                QStringList::const_iterator  end);
    virtual void ToStringList(QStringList &list) const;

    virtual void Clear(void);
    virtual bool IsEmpty(void) const { return name.isEmpty(); }

  public:
    QString name;     ///< input name
    uint    sourceid; ///< associated channel listings source
    uint    inputid;  ///< unique key in DB for this input
    uint    cardid;   ///< card id associated with input
    uint    mplexid;  ///< mplexid restriction if applicable
    uint    chanid;   ///< chanid restriction if applicable
    QString displayName;
    int     recPriority;
    uint    scheduleOrder;
    uint    livetvorder; ///< order for live TV use
    bool    quickTune;
};

class MTV_PUBLIC ChannelInputInfo : public InputInfo
{
  public:
    ChannelInputInfo() :
        startChanNum(QString::null),    tuneToChannel(QString::null),
        externalChanger(QString::null),
        inputNumV4L(-1), videoModeV4L2(0) {}
    ChannelInputInfo(QString _name,            QString _startChanNum,
                     QString _tuneToChannel,   QString _externalChanger,
                     uint    _sourceid,        uint    _cardid,
                     uint    _inputid,         uint    _mplexid,
                     uint    _livetvorder,
                     const ChannelInfoList &_channels) :
        InputInfo(_name, _sourceid, _inputid, _cardid, _mplexid, 0,
                  _livetvorder),
        startChanNum(_startChanNum),
        tuneToChannel(_tuneToChannel),  externalChanger(_externalChanger),
        channels(_channels),
        inputNumV4L(-1),
        videoModeV4L2(0) {}
    ChannelInputInfo(const ChannelInputInfo &other);
    ChannelInputInfo &operator=(const ChannelInputInfo &other);
    virtual ~ChannelInputInfo() {}

    virtual void Clear(void);

  public:
    QString      startChanNum;    ///< channel to start on
    QString      tuneToChannel;   ///< for using a cable box & S-Video
    QString      externalChanger; ///< for using a cable box...
    ChannelInfoList   channels;
    vector<uint> groups;
    int          inputNumV4L;
    int          videoModeV4L2;
};
typedef QMap<uint, ChannelInputInfo*> InputMap;

#endif // _INPUTINFO_H_
