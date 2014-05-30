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
#include "mythframe.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"

class MythPlayer;
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

class ClassicCommDetector : public CommDetectorBase
{
    Q_OBJECT

    public:
        ClassicCommDetector(SkipType commDetectMethod, bool showProgress,
                            bool fullSpeed, MythPlayer* player,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in);
        virtual void deleteLater(void);

        bool go();
        void GetCommercialBreakList(frm_dir_map_t &comms);
        void recordingFinished(long long totalFileSize);
        void requestCommBreakMapUpdate(void);

        void PrintFullMap(
            ostream &out, const frm_dir_map_t *comm_breaks,
            bool verbose) const;

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
        void GetBlankCommMap(frm_dir_map_t &comms);
        void GetBlankCommBreakMap(frm_dir_map_t &comms);
        void GetSceneChangeMap(frm_dir_map_t &scenes,
                               int64_t start_frame);
        frm_dir_map_t Combine2Maps(
            const frm_dir_map_t &a, const frm_dir_map_t &b) const;
        void UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo,
                              int format, int aspect);
        void BuildAllMethodsCommList(void);
        void BuildBlankFrameCommList(void);
        void BuildSceneChangeCommList(void);
        void BuildLogoCommList();
        void MergeBlankCommList(void);
        bool FrameIsInBreakMap(uint64_t f, const frm_dir_map_t &breakMap) const;
        void DumpMap(frm_dir_map_t &map);
        void CondenseMarkMap(show_map_t &map, int spacing, int length);
        void ConvertShowMapToCommMap(
            frm_dir_map_t &out, const show_map_t &in);
        void CleanupFrameInfo(void);
        void GetLogoCommBreakMap(show_map_t &map);

        enum SkipTypes commDetectMethod;
        frm_dir_map_t lastSentCommBreakMap;
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

        unsigned char *framePtr;

        frm_dir_map_t blankFrameMap;
        frm_dir_map_t blankCommMap;
        frm_dir_map_t blankCommBreakMap;
        frm_dir_map_t sceneMap;
        frm_dir_map_t sceneCommBreakMap;
        frm_dir_map_t commBreakMap;
        frm_dir_map_t logoCommBreakMap;

        bool frameIsBlank;
        bool sceneHasChanged;
        bool stationLogoPresent;

        bool lastFrameWasBlank;
        bool lastFrameWasSceneChange;
        bool decoderFoundAspectChanges;

        SceneChangeDetectorBase* sceneChangeDetector;

protected:
        MythPlayer *player;
        QDateTime startedAt, stopsAt;
        QDateTime recordingStartedAt, recordingStopsAt;
        bool aggressiveDetection;
        bool stillRecording;
        bool fullSpeed;
        bool showProgress;
        double fps;
        uint64_t framesProcessed;
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
