// -*- Mode: c++ -*-

#include "mythevent.h"
#include <qdeepcopy.h>

MythEvent::MythEvent(int t) :
    QCustomEvent(t), message(QString::null), extradata("empty")
{
}

MythEvent::MythEvent(const QString &lmessage) :
    QCustomEvent(MythEventMessage)
{
    message   = QDeepCopy<QString>(lmessage);
    extradata = "empty";
}

MythEvent::MythEvent(const QString &lmessage, const QStringList &lextradata) :
    QCustomEvent(MythEventMessage)
{
    message   = QDeepCopy<QString>(lmessage);
    extradata = QDeepCopy<QStringList>(lextradata);
}

MythEvent::~MythEvent()
{
}

const QString &MythEvent::ExtraData(int idx) const
{
    if (((uint)idx) < extradata.size())
        return extradata[idx];
    return QString::null;
} 
