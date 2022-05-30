#ifndef LOGODETECTORBASE_H
#define LOGODETECTORBASE_H

#include <QObject>
#include "libmythtv/mythframe.h"

class MythCommFlagPlayer;

class LogoDetectorBase : public QObject
{
    Q_OBJECT

  public:
    LogoDetectorBase(unsigned int w,unsigned int h) :
        m_width(w),m_height(h) {}

    virtual bool searchForLogo(MythCommFlagPlayer* player) = 0;
    virtual bool doesThisFrameContainTheFoundLogo(MythVideoFrame* frame) = 0;
    virtual bool pixelInsideLogo(unsigned int x, unsigned int y) = 0;
    virtual unsigned int getRequiredAvailableBufferForSearch() = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool haslogo,
                            float debugValue = 0.0);

  protected:
    ~LogoDetectorBase() override = default;

  protected:
    bool m_foundLogo {false};
    size_t m_width;
    size_t m_height;
};

#endif // LOGODETECTORBASE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
