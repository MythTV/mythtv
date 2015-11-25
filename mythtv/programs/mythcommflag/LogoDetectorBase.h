#ifndef _LOGODETECTORBASE_H_
#define _LOGODETECTORBASE_H_

#include <QObject>

class MythPlayer;
typedef struct VideoFrame_ VideoFrame;

class LogoDetectorBase : public QObject
{
    Q_OBJECT

  public:
    LogoDetectorBase(unsigned int w,unsigned int h) :
        foundLogo(false), width(w),height(h) {};

    virtual bool searchForLogo(MythPlayer* player) = 0;
    virtual bool doesThisFrameContainTheFoundLogo(VideoFrame* frame) = 0;
    virtual bool pixelInsideLogo(unsigned int x, unsigned int y) = 0;
    virtual unsigned int getRequiredAvailableBufferForSearch() = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool haslogo,
                            float debugValue = 0.0);

  protected:
    virtual ~LogoDetectorBase() {}

  protected:
    bool foundLogo;
    unsigned int width, height;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

