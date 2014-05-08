// -*- Mode: c++ -*-

// MythTV headers
#include "inputinfo.h"

InputInfo::InputInfo(
    const QString &_name,
    uint _sourceid, uint _inputid, uint _cardid, uint _mplexid, uint _chanid,
    uint _livetvorder) :
    name(_name),
    sourceid(_sourceid),
    inputid(_inputid),
    cardid(_cardid),
    mplexid(_mplexid),
    chanid(_chanid),
    livetvorder(_livetvorder)
{
    name.detach();
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

    name     = *it;
    name.detach();
    name     = (name == "<EMPTY>") ? QString::null : name;
    NEXT();

    sourceid = (*it).toUInt(); NEXT();
    inputid  = (*it).toUInt(); NEXT();
    cardid   = (*it).toUInt(); NEXT();
    mplexid  = (*it).toUInt(); NEXT();
    livetvorder = (*it).toUInt(); NEXT();

    displayName = *it;
    displayName.detach();
    displayName = (displayName == "<EMPTY>") ? QString::null : displayName;
    NEXT();

    recPriority = (*it).toInt(); NEXT();
    scheduleOrder = (*it).toUInt(); NEXT();
    quickTune = (*it).toUInt(); NEXT();
    chanid   = (*it).toUInt(); ++it;

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
    list.push_back(QString::number(livetvorder));
    list.push_back(displayName.isEmpty() ? "<EMPTY>" : displayName);
    list.push_back(QString::number(recPriority));
    list.push_back(QString::number(scheduleOrder));
    list.push_back(QString::number(quickTune));
    list.push_back(QString::number(chanid));
}

ChannelInputInfo::ChannelInputInfo(const ChannelInputInfo &other) :
    InputInfo(*this),
    startChanNum(other.startChanNum),
    tuneToChannel(other.tuneToChannel),
    externalChanger(other.externalChanger),
    channels(other.channels),
    groups(other.groups),
    inputNumV4L(other.inputNumV4L),
    videoModeV4L2(other.videoModeV4L2)
{
    startChanNum.detach();
    tuneToChannel.detach();
    externalChanger.detach();
}

ChannelInputInfo &ChannelInputInfo::operator=(const ChannelInputInfo &other)
{
    *((InputInfo*)this) = other;

    startChanNum    = other.startChanNum;
    tuneToChannel   = other.tuneToChannel;
    externalChanger = other.externalChanger;
    channels        = other.channels;
    groups          = other.groups;
    inputNumV4L     = other.inputNumV4L;
    videoModeV4L2   = other.videoModeV4L2;

    startChanNum.detach();
    tuneToChannel.detach();
    externalChanger.detach();

    return *this;
}

void ChannelInputInfo::Clear(void)
{
    ChannelInputInfo blank;
    *this = blank;
}

