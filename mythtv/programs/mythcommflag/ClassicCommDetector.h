#ifndef _CLASSIC_COMMDETECTOR_H_
#define _CLASSIC_COMMDETECTOR_H_

// POSIX headers
#include <stdint.h>

// Qt headers
#include <QObject>
#include <QMap>
#include <QDateTime>

// MythTV headers
#include "programinfo.h"
#include "frame.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"

class NuppelVideoPlayer;
class LogoDetectorBase;
class SceneChangeDetectorBase;

enum frameMaskValues {
    COMM_FRAME_SKIPPED       = 0x0001,
    COMM_FRAME_BLANK         = 0x0002,
    COMM_FRAME_SCENE_CHANGE  = 0x0004,
    COMM_FRAME_LOGO_PRESENT  = 0x0008,
    COMM_FRAME_ASPECT_CHANGE = 0x0010,
    COMM_FRAME_RATING_SYMBOL = 0x0020
};

class FrameInfoEntry
{
  public:
    int minBrightness;
    int maxBrightness;
    int avgBrightness;
    int sceneChangePercent;
    int aspect;
    int format;
    int flagMask;
    static QString GetHeader(void);
    QString toString(uint64_t frame, bool verbose) const;
};

typedef QMap<long long, int> comm_map_t;

class ClassicCommDetector : public CommDetectorBase
{
    Q_OBJECT

    public:
        ClassicCommDetector(SkipType commDetectMethod, bool showProgress,
                            bool fullSpeed, NuppelVideoPlayer* nvp,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in);
        virtual void deleteLater(void);

        bool go();
        void getCommercialBreakList(comm_map_t &comms);
        void recordingFinished(long long totalFileSize);
        void requestCommBreakMapUpdate(void);

        void PrintFullMap(
            ostream &out, const comm_break_t *comm_breaks, bool verbose) const;

        void logoDetectorBreathe();

        friend class ClassicLogoDetector;

    protected:
        virtual ~ClassicCommDetector() {}

    private:
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

        void ClearAllMaps(void);
        void GetBlankCommMap(comm_map_t &comms);
        void GetBlankCommBreakMap(comm_map_t &comms);
        void GetSceneChangeMap(comm_map_t &scenes,
                               long long start_frame);
        comm_map_t Combine2Maps(comm_map_t &a, comm_map_t &b) const;
        void UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo,
                              int format, int aspect);
        void BuildAllMethodsCommList(void);
        void BuildBlankFrameCommList(void);
        void BuildSceneChangeCommList(void);
        void BuildLogoCommList();
        void MergeBlankCommList(void);
        bool FrameIsInBreakMap(long long f, const comm_map_t &breakMap) const;
        void DumpMap(comm_map_t &map);
        void CondenseMarkMap(comm_map_t&map, int spacing, int length);
        void ConvertShowMapToCommMap(comm_map_t&map);
        void CleanupFrameInfo(void);
        void GetLogoCommBreakMap(comm_map_t &map);

        enum SkipTypes commDetectMethod;
        QMap<long long,int> lastSentCommBreakMap;
        bool commBreakMapUpdateRequested;
        bool sendCommBreakMapUpdates;

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
        double fpm;
        bool blankFramesOnly;
        int blankFrameCount;
        int currentAspect;


        int totalMinBrightness;

        bool detectBlankFrames;
        bool detectSceneChanges;
        bool detectStationLogo;

        bool logoInfoAvailable;
        LogoDetectorBase* logoDetector;

        bool skipAllBlanks;

        unsigned char *framePtr;

        comm_map_t blankFrameMap;
        comm_map_t blankCommMap;
        comm_map_t blankCommBreakMap;
        comm_map_t sceneMap;
        comm_map_t sceneCommBreakMap;
        comm_map_t commBreakMap;
        comm_map_t logoCommBreakMap;

        bool frameIsBlank;
        bool sceneHasChanged;
        bool stationLogoPresent;

        bool lastFrameWasBlank;
        bool lastFrameWasSceneChange;
        bool decoderFoundAspectChanges;

        SceneChangeDetectorBase* sceneChangeDetector;

protected:
        NuppelVideoPlayer *nvp;
        QDateTime startedAt, stopsAt;
        QDateTime recordingStartedAt, recordingStopsAt;
        bool aggressiveDetection;
        bool stillRecording;
        bool fullSpeed;
        bool showProgress;
        double fps;
        long long framesProcessed;
        long long preRoll;
        long long postRoll;


        void Init();
        void SetVideoParams(float aspect);
        void ProcessFrame(VideoFrame *frame, long long frame_number);
        QMap<long long, FrameInfoEntry> frameInfo;

public slots:
        void sceneChangeDetectorHasNewInformation(unsigned int framenum, bool isSceneChange,float debugValue);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
