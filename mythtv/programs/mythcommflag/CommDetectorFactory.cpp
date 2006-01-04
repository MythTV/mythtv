#include "CommDetectorFactory.h"
#include "ClassicCommDetector.h"

class NuppelVideoPlayer;
class RemoteEncoder;

CommDetectorBase*
    CommDetectorFactory::makeCommDetector(int commDetectMethod,
                                          bool showProgress, bool fullSpeed,
                                          NuppelVideoPlayer* nvp,
                                          const QDateTime& startedAt,
                                          const QDateTime& stopsAt,
                                          const QDateTime& recordingStartedAt,
                                          const QDateTime& recordingStopsAt)
{
	switch (commDetectMethod)
	{
		//Future different CommDetect implementations will be created here.
        default:
			return new ClassicCommDetector(commDetectMethod, showProgress,
                                           fullSpeed, nvp, startedAt,
                                           stopsAt, recordingStartedAt,
                                           recordingStopsAt);
	}
	
	return 0;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
