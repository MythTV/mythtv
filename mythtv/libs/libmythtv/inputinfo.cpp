// -*- Mode: c++ -*-

// Qt headers
#include <qdeepcopy.h>

// MythTV headers
#include "inputinfo.h"

InputInfo::InputInfo(
    const QString &_name,
    uint _sourceid, uint _inputid, uint _cardid, uint _mplexid) :
    name(QDeepCopy<QString>(_name)),
    sourceid(_sourceid),
    inputid(_inputid),
    cardid(_cardid),
    mplexid(_mplexid)
{
}

InputInfo::InputInfo(const InputInfo &other) :
    name(QDeepCopy<QString>(other.name)),
    sourceid(other.sourceid),
    inputid(other.inputid),
    cardid(other.cardid),
    mplexid(other.mplexid)
{
}

InputInfo &InputInfo::operator=(const InputInfo &other)
{
    name     = QDeepCopy<QString>(other.name);
    sourceid = other.sourceid;
    inputid  = other.inputid;
    cardid   = other.cardid;
    mplexid  = other.mplexid;
    return *this;
}

void InputInfo::Clear(void)
{
    InputInfo blank;
    *this = blank;
}

#define NEXT() do { ++it; if (it == end) return false; } while (0)
bool InputInfo::FromStringList(QStringList::const_iterator &it,
                               QStringList::const_iterator end)
{
    if (it == end)
        return false;

    name     = QDeepCopy<QString>(*it);
    name     = (name == "<EMPTY>") ? QString::null : name;
    NEXT();

    sourceid = (*it).toUInt(); NEXT();
    inputid  = (*it).toUInt(); NEXT();
    cardid   = (*it).toUInt(); NEXT();
    mplexid  = (*it).toUInt(); ++it;

    return true;
}
#undef NEXT

void InputInfo::ToStringList(QStringList &list) const
{
    list.push_back(name.isEmpty() ? "<EMPTY>" : name);
    list.push_back(QString::number(sourceid));
    list.push_back(QString::number(inputid));
    list.push_back(QString::number(cardid));
    list.push_back(QString::number(mplexid));
}

TunedInputInfo::TunedInputInfo(
    const QString &_name,
    uint _sourceid, uint _inputid, uint _cardid, uint _mplexid, uint _chanid) :
    InputInfo(_name, _sourceid, _inputid, _cardid, _mplexid), chanid(_chanid)
{
}

TunedInputInfo::TunedInputInfo(const TunedInputInfo &other) :
    InputInfo(other), chanid(other.chanid)
{
}

TunedInputInfo &TunedInputInfo::operator=(const TunedInputInfo &other)
{
    *((InputInfo*)this) = other;
    chanid = other.chanid;
    return *this;
}

void TunedInputInfo::Clear(void)
{
    TunedInputInfo blank;
    *this = blank;
}

bool TunedInputInfo::FromStringList(QStringList::const_iterator &it,
                                    QStringList::const_iterator end)
{
    if (!InputInfo::FromStringList(it, end) || (it == end))
        return false;

    chanid = (*it).toUInt();
    ++it;
    return true;
}

void TunedInputInfo::ToStringList(QStringList &list) const
{
    InputInfo::ToStringList(list);
    list.push_back(QString::number(chanid));
}

ChannelInputInfo::ChannelInputInfo(const ChannelInputInfo &other) :
    InputInfo(*this),
    startChanNum(QDeepCopy<QString>(other.startChanNum)),
    tuneToChannel(QDeepCopy<QString>(other.tuneToChannel)),
    externalChanger(QDeepCopy<QString>(other.externalChanger)),
    channels(other.channels),
    inputNumV4L(other.inputNumV4L),
    videoModeV4L1(other.videoModeV4L1),
    videoModeV4L2(other.videoModeV4L2)
{
}

ChannelInputInfo &ChannelInputInfo::operator=(const ChannelInputInfo &other)
{
    *((InputInfo*)this) = other;

    startChanNum    = QDeepCopy<QString>(other.startChanNum);
    tuneToChannel   = QDeepCopy<QString>(other.tuneToChannel);
    externalChanger = QDeepCopy<QString>(other.externalChanger);
    channels        = other.channels;
    inputNumV4L     = other.inputNumV4L;
    videoModeV4L1   = other.videoModeV4L1;
    videoModeV4L2   = other.videoModeV4L2;

    return *this;
}

void ChannelInputInfo::Clear(void)
{
    ChannelInputInfo blank;
    *this = blank;
}

