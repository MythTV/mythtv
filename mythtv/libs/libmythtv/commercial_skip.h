#ifndef __COMMERCIAL_SKIP_H__
#define __COMMERCIAL_SKIP_H__

#include <qmap.h>

class CommDetect
{
  public:
    CommDetect(int w, int h, double fps);
    ~CommDetect();

    void ReInit(int w, int h, double fps);

    bool FrameIsBlank(unsigned char *buf);
    bool SceneChanged(unsigned char *buf);

    int GetAvgBrightness(unsigned char *buf);

    void BuildCommListFromBlanks(QMap<long long, int> &blanks,
                                 QMap<long long, int> &commMap);
    void MergeCommList(QMap<long long, int> &commMap,
                       QMap<long long, int> &commBreakMap);
    void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame);

  private:
    int width;
    int height;
    double frame_rate;

    double sceneChangeTolerance;

    bool lastFrameWasBlank;
    bool lastFrameWasSceneChange;

    unsigned char *lastFrame;
    int lastHistogram[256];
};

#endif
