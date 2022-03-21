#ifndef SCENECHANGEDETECTORBASE_H
#define SCENECHANGEDETECTORBASE_H

#include <QObject>
#include "libmythtv/mythframe.h"

class SceneChangeDetectorBase : public QObject
{
    Q_OBJECT

  public:
    SceneChangeDetectorBase(unsigned int w, unsigned int h) :
        m_width(w), m_height(h) {}

    virtual void processFrame(MythVideoFrame* frame) = 0;

  signals:
    void haveNewInformation(unsigned int framenum, bool scenechange,
                            float debugValue = 0.0);

  protected:
    ~SceneChangeDetectorBase() override = default;

  protected:
    unsigned int m_width, m_height;
};

#endif // SCENECHANGEDETECTORBASE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
