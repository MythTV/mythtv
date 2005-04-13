#ifndef __COMMERCIAL_SKIP_H__
#define __COMMERCIAL_SKIP_H__

#include <qmap.h>

#include "NuppelVideoPlayer.h"
#include "mythcontext.h"
#include "remoteencoder.h"

enum commMapValues {
    MARK_START = 0,
    MARK_END,
    MARK_PRESENT
};

// This is used as a bitmask.
enum SkipTypes {
    COMM_DETECT_OFF         = 0x00,
    COMM_DETECT_BLANKS      = 0x01,
    COMM_DETECT_SCENE       = 0x02,
    COMM_DETECT_BLANK_SCENE = 0x03,
    COMM_DETECT_LOGO        = 0x04,
    COMM_DETECT_ALL         = 0xFF
};

enum frameMaskValues {
    COMM_FRAME_BLANK         = 0x0001,
    COMM_FRAME_SCENE_CHANGE  = 0x0002,
    COMM_FRAME_LOGO_PRESENT  = 0x0004,
    COMM_FRAME_ASPECT_CHANGE = 0x0008,
    COMM_FRAME_RATING_SYMBOL = 0x0010
};

enum frameAspects {
    COMM_ASPECT_NORMAL = 0,
    COMM_ASPECT_WIDE
};

enum frameFormats {
    COMM_FORMAT_NORMAL = 0,
    COMM_FORMAT_LETTERBOX,
    COMM_FORMAT_PILLARBOX,
    COMM_FORMAT_MAX
};

// Set max comm break length to 00:08:05 minutes & max comm length to 00:02:05
#define MIN_COMM_BREAK_LENGTH   60
#define MAX_COMM_BREAK_LENGTH  395
#define MAX_COMM_LENGTH        125
#define MIN_SHOW_LENGTH         65
#define LOGO_SAMPLES_NEEDED    240
#define LOGO_SAMPLE_SPACING      2
#define LOGO_SECONDS_NEEDED    (LOGO_SAMPLES_NEEDED * LOGO_SAMPLE_SPACING)

extern "C" {
#include "frame.h"
}

class CommDetect : public QObject
{
    Q_OBJECT
  public:
    CommDetect(int w, int h, double frame_rate, int method);
    ~CommDetect(void);

    void customEvent(QCustomEvent *e);

    void Init(int w, int h, double frame_rate, int method);
    void SetVideoParams(float aspect);
    void SetCommDetectMethod(int method) { commDetectMethod = method; };
    void SetWatchingRecording(bool watching, RemoteEncoder *rec,
                              NuppelVideoPlayer *nvp);

    void ProcessNextFrame(VideoFrame *frame, long long frame_number = -1);
    void SetVerboseDebugging(bool beVerbose = true)
                             { verboseDebugging = beVerbose; };

    bool FrameIsBlank(void);
    bool SceneHasChanged(void);

    void SetAggressiveDetection(bool onOff)  { aggressiveDetection = onOff; }
    void SetCommSkipAllBlanks(bool onOff)  { skipAllBlanks = onOff; }

    void ClearAllMaps(void);
    void GetCommBreakMap(QMap<long long, int> &marks);
    void SetBlankFrameMap(QMap<long long, int> &blanks);
    void GetBlankFrameMap(QMap<long long, int> &blanks,
                          long long start_frame = -1);
    void GetBlankCommMap(QMap<long long, int> &comms);
    void GetBlankCommBreakMap(QMap<long long, int> &breaks);
    void GetSceneChangeMap(QMap<long long, int> &scenes,
                           long long start_frame = -1);
    void GetLogoCommBreakMap(QMap<long long, int> &map);

    void BuildMasterCommList(void);
    void BuildAllMethodsCommList(void);
    void BuildBlankFrameCommList(void);
    void BuildSceneChangeCommList(void);
    void BuildLogoCommList(void);

    void MergeBlankCommList(void);
    void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame);

    bool FrameIsInBreakMap(long long f, QMap<long long, int> &breakMap);

    void DumpMap(QMap<long long, int> &map);
    void DumpLogo(bool fromCurrentFrame = false);

    void SetMinMaxPixels(VideoFrame *frame);

    void SearchForLogo(NuppelVideoPlayer *nvp, bool fullSpeed);
    void SetLogoMaskArea();
    void GetLogoMask(unsigned char *mask);
    void SetLogoMask(unsigned char *mask);


  private:
    typedef struct edgemaskentry {
        int isedge;
        int horiz;
        int vert;
        int rdiag;
        int ldiag;
    } EdgeMaskEntry;

    typedef struct frameinfo {
        int minBrightness;
        int maxBrightness;
        int avgBrightness;
        int sceneChangePercent;
        int aspect;
        int format;
        int flagMask;
    } FrameInfoEntry;

    typedef struct frameblock {
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
    } FrameBlock;

    bool CheckFrameIsBlank(void);
    bool CheckSceneHasChanged(void);
    bool CheckStationLogo(void);
    bool CheckEdgeLogo(void);
    bool CheckRatingSymbol(void);
    bool CheckFrameIsInCommMap(long long frameNumber, QMap<long long, int>);

    void CondenseMarkMap(QMap<long long, int>&map, int spacing, int length);
    void ConvertShowMapToCommMap(QMap<long long, int>&map);

    void CleanupFrameInfo(void);
    void DumpFrameInfo(void);
    void UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo, int format,
                          int aspect);
    void DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges, int edgeDiff);

    bool aggressiveDetection;

    int commDetectMethod;
    bool verboseDebugging;

    long long curFrameNumber;

    int width;
    int height;
    int horizSpacing;
    int vertSpacing;
    int border;
    int blankFrameMaxDiff;
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

    NuppelVideoPlayer *m_parent;
    bool watchingRecording;
    RemoteEncoder *recorder;

    EdgeMaskEntry *edgeMask;

    unsigned char *logoMaxValues;
    unsigned char *logoMinValues;
    unsigned char *logoFrame;
    unsigned char *logoMask;
    unsigned char *logoCheckMask;
    unsigned char *tmpBuf;
    bool logoInfoAvailable;
    int logoEdgeDiff;
    double logoGoodEdgeThreshold;
    double logoBadEdgeThreshold;
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

    int sceneChangePercent;

    bool lastFrameWasBlank;
    bool lastFrameWasSceneChange;
    bool decoderFoundAspectChanges;

    int histogram[256];
    int lastHistogram[256];
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
