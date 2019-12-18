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
    InputInfo() = default;
    InputInfo(const QString &_name,
              uint _sourceid, uint _inputid, uint _mplexid,
              uint _chanid, uint _livetvorder) :
        m_name(_name),
        m_sourceId(_sourceid),
        m_inputId(_inputid),
        m_mplexId(_mplexid),
        m_chanId(_chanid),
        m_liveTvOrder(_livetvorder) {}

    virtual ~InputInfo() = default;

    InputInfo(const InputInfo &other) = default;
    InputInfo &operator=(const InputInfo &other) = default;

    bool operator == (uint inputid) const
        { return m_inputId == inputid; }

    bool operator == (const QString &name) const
        { return m_name == name; }

    virtual bool FromStringList(QStringList::const_iterator &it,
                                const QStringList::const_iterator& end);
    virtual void ToStringList(QStringList &list) const;

    virtual void Clear(void);
    virtual bool IsEmpty(void) const { return m_name.isEmpty(); }

  public:
    QString m_name;              ///< input name
    uint    m_sourceId      {0}; ///< associated channel listings source
    uint    m_inputId       {0}; ///< unique key in DB for this input
    uint    m_mplexId       {0}; ///< mplexid restriction if applicable
    uint    m_chanId        {0}; ///< chanid restriction if applicable
    QString m_displayName;
    int     m_recPriority   {0};
    uint    m_scheduleOrder {0};
    uint    m_liveTvOrder   {0}; ///< order for live TV use
    bool    m_quickTune     {false};
};

#endif // _INPUTINFO_H_
