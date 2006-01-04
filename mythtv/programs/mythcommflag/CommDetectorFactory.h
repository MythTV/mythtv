#ifndef _COMMDETECTOR_FACTORY_H_
#define _COMMDETECTOR_FACTORY_H_

class CommDetectorBase;
class NuppelVideoPlayer;
class RemoteEncoder;
class QDateTime;

// This is used as a bitmask.
enum SkipTypes {
    COMM_DETECT_OFF         = 0x0000,
    COMM_DETECT_BLANKS      = 0x0001,
    COMM_DETECT_SCENE       = 0x0002,
    COMM_DETECT_BLANK_SCENE = 0x0003,
    COMM_DETECT_LOGO        = 0x0004,
    COMM_DETECT_ALL         = 0x00FF
};


class CommDetectorFactory
{
public:
	CommDetectorFactory() {}
	~CommDetectorFactory() {}

	CommDetectorBase* makeCommDetector(int commDetectMethod, bool showProgress,
                                       bool fullSpeed, NuppelVideoPlayer* nvp,
                                       const QDateTime& startedAt,
                                       const QDateTime& stopsAt,
                                       const QDateTime& recordingStartedAt,
                                       const QDateTime& recordingStopsAt);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
