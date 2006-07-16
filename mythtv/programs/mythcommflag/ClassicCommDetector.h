#ifndef _CLASSIC_COMMDETECTOR_H_
#define _CLASSIC_COMMDETECTOR_H_

#include "CommDetectorBase.h"
#include "libmythtv/frame.h"
#include "qdatetime.h"

class NuppelVideoPlayer;
class LogoDetectorBase;
class SceneChangeDetectorBase;

class ClassicCommDetector : public CommDetectorBase
{
    Q_OBJECT

    public:
        ClassicCommDetector(int commDetectMethod, bool showProgress,
                            bool fullSpeed, NuppelVideoPlayer* nvp,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in);
        virtual ~ClassicCommDetector();
        bool go();
        void getCommercialBreakList(QMap<long long, int> &comms);
        void recordingFinished(long long totalFileSize);
        void requestCommBreakMapUpdate(void);

        void logoDetectorBreathe();

        friend class ClassicLogoDetector;
    private:

        typedef struct frameinfo
        {
            int minBrightness;
            int maxBrightness;
            int avgBrightness;
            int sceneChangePercent;
            int aspect;
            int format;
            int flagMask;
        }
        FrameInfoEntry;

        typedef struct frameblock
        {
            long start;
            long end;
            long frames;
            double length;
            int bfCount;
            int logoCount;
            int ratingCount;
            int scCount;
            double scRate;
            int formatMatch;
            int aspectMatch;
            int score;
        }
        FrameBlock;

        void Init();
        void SetVideoParams(float aspect);
        void ProcessFrame(VideoFrame *frame, long long frame_number);
        void ClearAllMaps(void);
        void GetBlankCommMap(QMap<long long, int> &comms);
        void GetBlankCommBreakMap(QMap<long long, int> &comms);
        void GetSceneChangeMap(QMap<long long, int> &scenes,
                               long long start_frame);
        void BuildMasterCommList(void);
        void UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo,
                              int format, int aspect);
        void BuildAllMethodsCommList(void);
        void BuildBlankFrameCommList(void);
        void BuildSceneChangeCommList(void);
        void BuildLogoCommList();
        void MergeBlankCommList(void);
        bool FrameIsInBreakMap(long long f, QMap<long long, int> &breakMap);
        void DumpMap(QMap<long long, int> &map);
        void CondenseMarkMap(QMap<long long, int>&map, int spacing, int length);
        void ConvertShowMapToCommMap(QMap<long long, int>&map);
        void CleanupFrameInfo(void);
        void GetLogoCommBreakMap(QMap<long long, int> &map);

        int commDetectMethod;
        bool showProgress;
        bool fullSpeed;
        NuppelVideoPlayer *nvp;
        QDateTime startedAt, stopsAt;
        QDateTime recordingStartedAt, recordingStopsAt;
        bool stillRecording;
        QMap<long long,int> lastSentCommBreakMap;
        bool commBreakMapUpdateRequested;
        bool sendCommBreakMapUpdates;

        bool aggressiveDetection;
        int commDetectBorder;
        int commDetectBlankFrameMaxDiff;
        int commDetectDarkBrightness;
        int commDetectDimBrightness;
        int commDetectBoxBrightness;
        int commDetectDimAverage;
        int commDetectMaxCommBreakLength;
        int commDetectMinCommBreakLength;
        int commDetectMinShowLength;
        int commDetectMaxCommLength;
        bool commDetectBlankCanHaveLogo;

        bool verboseDebugging;

        long long lastFrameNumber;
        long long curFrameNumber;

        int width;
        int height;
        int horizSpacing;
        int vertSpacing;
        double fps;
        double fpm;
        bool blankFramesOnly;
        int blankFrameCount;
        int currentAspect;

        long long framesProcessed;
        long long preRoll;
        long long postRoll;

        int totalMinBrightness;

        bool detectBlankFrames;
        bool detectSceneChanges;
        bool detectStationLogo;

        bool logoInfoAvailable;
        LogoDetectorBase* logoDetector;

        bool skipAllBlanks;

        unsigned char *framePtr;

        QMap<long long, FrameInfoEntry> frameInfo;
        QMap<long long, int> blankFrameMap;
        QMap<long long, int> blankCommMap;
        QMap<long long, int> blankCommBreakMap;
        QMap<long long, int> sceneMap;
        QMap<long long, int> sceneCommBreakMap;
        QMap<long long, int> commBreakMap;
        QMap<long long, int> logoCommBreakMap;

        bool frameIsBlank;
        bool sceneHasChanged;
        bool stationLogoPresent;

        bool lastFrameWasBlank;
        bool lastFrameWasSceneChange;
        bool decoderFoundAspectChanges;

        SceneChangeDetectorBase* sceneChangeDetector;

public slots:
        void sceneChangeDetectorHasNewInformation(unsigned int framenum, bool isSceneChange,float debugValue);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
