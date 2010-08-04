#ifndef _COMMDETECTOR_FACTORY_H_
#define _COMMDETECTOR_FACTORY_H_

#include "programinfo.h"

class CommDetectorBase;
class MythPlayer;
class RemoteEncoder;
class QDateTime;

class CommDetectorFactory
{
  public:
    CommDetectorFactory() {}
    ~CommDetectorFactory() {}

    CommDetectorBase* makeCommDetector(
        SkipType commDetectMethod,
        bool showProgress,
        bool fullSpeed, MythPlayer* player,
        int chanid,
        const QDateTime& startedAt,
        const QDateTime& stopsAt,
        const QDateTime& recordingStartedAt,
        const QDateTime& recordingStopsAt,
        bool useDB);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
