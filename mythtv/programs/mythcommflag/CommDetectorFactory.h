#ifndef _COMMDETECTOR_FACTORY_H_
#define _COMMDETECTOR_FACTORY_H_

#include "CommDetector.h"

class CommDetectorBase;
class NuppelVideoPlayer;
class RemoteEncoder;
class QDateTime;

class CommDetectorFactory
{
public:
	CommDetectorFactory() {}
	~CommDetectorFactory() {}

	CommDetectorBase* makeCommDetector(enum SkipTypes commDetectMethod,
                                       bool showProgress,
                                       bool fullSpeed, NuppelVideoPlayer* nvp,
                                       int chanid,
                                       const QDateTime& startedAt,
                                       const QDateTime& stopsAt,
                                       const QDateTime& recordingStartedAt,
                                       const QDateTime& recordingStopsAt);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
