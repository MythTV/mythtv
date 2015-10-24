#ifndef _SCENECHANGEDETECTORBASE_H_
#define _SCENECHANGEDETECTORBASE_H_

#include <QObject>

typedef struct VideoFrame_ VideoFrame;

class SceneChangeDetectorBase : public QObject
{
    Q_OBJECT

  public:
    SceneChangeDetectorBase(unsigned int w, unsigned int h) :
        width(w), height(h) {}

    virtual void processFrame(VideoFrame* frame) = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool scenechange,
                            float debugValue = 0.0);

  protected:
    virtual ~SceneChangeDetectorBase() {}

  protected:
    unsigned int width, height;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

