#ifndef __COMMERCIAL_SKIP_H__
#define __COMMERCIAL_SKIP_H__

#include <qmap.h>

#include "NuppelVideoPlayer.h"
#include "mythcontext.h"

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


extern "C" {
#include "frame.h"
}

class CommDetect
{
  public:
    CommDetect(int w, int h, double frame_rate, int method);
    ~CommDetect(void);

    void Init(int w, int h, double frame_rate, int method);

    void ProcessNextFrame(VideoFrame *frame, long long frame_number = -1);

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

    void BuildMasterCommList(void);
    void BuildAllMethodsCommList(void);
    void BuildBlankFrameCommList(void);
    void BuildSceneChangeCommList(void);
    void BuildLogoCommList(void);

    void MergeBlankCommList(void);
    void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame);

    bool FrameIsInCommBreak(long long f, QMap<long long, int> &breakMap);

    void DumpMap(QMap<long long, int> &map);
    void DumpLogo(bool fromCurrentFrame = false);

    void SetMinMaxPixels(VideoFrame *frame);

    void SearchForLogo(NuppelVideoPlayer *nvp, bool fullSpeed, bool verbose);
    void SetLogoMaskArea();
    void GetLogoMask(unsigned char *mask);
    void SetLogoMask(unsigned char *mask);


  private:
    bool CheckFrameIsBlank(void);
    bool CheckSceneHasChanged(void);
    bool CheckStationLogo(void);

    int GetAvgBrightness(void);

    void CondenseMarkMap(QMap<long long, int>&map, int spacing, int length);
    void ConvertShowMapToCommMap(QMap<long long, int>&map);

    bool aggressiveDetection;

    int commDetectMethod;

    int width;
    int height;
    int border;
    double fps;
    double fpm;
    bool blankFramesOnly;

    long long framesProcessed;

    bool detectBlankFrames;
    bool detectSceneChanges;
    bool detectStationLogo;

    bool skipAllBlanks;

    unsigned char *logoMaxValues;
    unsigned char *logoMinValues;
    unsigned char *logoFrame;
    unsigned char *logoMask;
    unsigned char *logoCheckMask;
    bool logoInfoAvailable;
    int logoFrameCount;
    int logoMinX;
    int logoMaxX;
    int logoMinY;
    int logoMaxY;

    unsigned char *frame_ptr;

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

    int histogram[256];
    int lastHistogram[256];
};

#endif
