#ifndef _CLASSIC_COMMDETECTOR_H_
#define _CLASSIC_COMMDETECTOR_H_

#include "CommDetectorBase.h"
#include "libmythtv/frame.h"
#include "qdatetime.h"

class NuppelVideoPlayer;

class ClassicCommDetector : public CommDetectorBase
{
    public:
        ClassicCommDetector(int commDetectMethod, bool showProgress,
                            bool fullSpeed, NuppelVideoPlayer* nvp,
                            const QDateTime& recordingStartedAt,
                            const QDateTime& recordingStopsAt_in);
        virtual ~ClassicCommDetector();
        bool go();
        void getCommercialBreakList(QMap<long long, int> &comms);
        void recordingFinished(long long totalFileSize);
        void requestCommBreakMapUpdate(void);

    private:

        typedef struct edgemaskentry
        {
            int isedge;
            int horiz;
            int vert;
            int rdiag;
            int ldiag;
        }
        EdgeMaskEntry;

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
        void DumpLogo(bool fromCurrentFrame);
        void SetLogoMaskArea();
        void SetLogoMask(unsigned char *mask);
        void CondenseMarkMap(QMap<long long, int>&map, int spacing, int length);
        void ConvertShowMapToCommMap(QMap<long long, int>&map);
        bool CheckEdgeLogo(void);
        void SearchForLogo();
        void CleanupFrameInfo(void);
        void DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges, int edgeDiff);
        void GetLogoCommBreakMap(QMap<long long, int> &map);

        int commDetectMethod;
        bool showProgress;
        bool fullSpeed;
        NuppelVideoPlayer *nvp;
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
        int commDetectLogoSamplesNeeded;
        int commDetectLogoSampleSpacing;
        int commDetectLogoSecondsNeeded;
        bool commDetectBlankCanHaveLogo;
        double commDetectLogoGoodEdgeThreshold;
        double commDetectLogoBadEdgeThreshold;

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

        int totalMinBrightness;

        bool detectBlankFrames;
        bool detectSceneChanges;
        bool detectStationLogo;

        bool skipAllBlanks;

        EdgeMaskEntry *edgeMask;

        unsigned char *logoMaxValues;
        unsigned char *logoMinValues;
        unsigned char *logoFrame;
        unsigned char *logoMask;
        unsigned char *logoCheckMask;
        unsigned char *tmpBuf;
        bool logoInfoAvailable;
        int logoEdgeDiff;
        int logoFrameCount;
        int logoMinX;
        int logoMaxX;
        int logoMinY;
        int logoMaxY;

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

        int histogram[256];
        int lastHistogram[256];
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
