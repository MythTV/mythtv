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
                  sourceid(0), inputid(0), mplexid(0),
                  chanid(0), recPriority(0), scheduleOrder(0),
                  livetvorder(0), quickTune(false) {}
    InputInfo(const QString &_name,
              uint _sourceid, uint _inputid, uint _mplexid,
              uint _chanid, uint _livetvorder) :
        name(_name),
        sourceid(_sourceid),
        inputid(_inputid),
        mplexid(_mplexid),
        chanid(_chanid),
        recPriority(0),
        scheduleOrder(0),
        livetvorder(_livetvorder),
        quickTune(false) {}

    InputInfo(const InputInfo &other) :
        name(other.name),
        sourceid(other.sourceid),
        inputid(other.inputid),
        mplexid(other.mplexid),
        chanid(other.chanid),
        displayName(other.displayName),
        recPriority(other.recPriority),
        scheduleOrder(other.scheduleOrder),
        livetvorder(other.livetvorder),
        quickTune(other.quickTune) {}

    InputInfo &operator=(const InputInfo &other)
    {
        name     = other.name;
        sourceid = other.sourceid;
        inputid  = other.inputid;
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
    uint    mplexid;  ///< mplexid restriction if applicable
    uint    chanid;   ///< chanid restriction if applicable
    QString displayName;
    int     recPriority;
    uint    scheduleOrder;
    uint    livetvorder; ///< order for live TV use
    bool    quickTune;
};

#endif // _INPUTINFO_H_
