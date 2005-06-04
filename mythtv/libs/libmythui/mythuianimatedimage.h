#ifndef MYTHUI_ANIIMAGE_H_
#define MYTHUI_ANIIMAGE_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qvaluevector.h>

#include "mythuitype.h"
#include "mythimage.h"

// filepattern is something like: anim%1.png, with the %1 being replaced by 
// numbers from low to high inclusive.

class MythUIAnimatedImage : public MythUIType
{
  public:
    MythUIAnimatedImage(const QString &filepattern, int low, int high, 
                        int delayms, MythUIType *parent, const char *name);
    MythUIAnimatedImage(MythUIType *parent, const char *name);
   ~MythUIAnimatedImage();

    // doesn't load
    void SetFilepattern(const QString &filepattern, int low, int high);
    void SetDelay(int delayms);

    void SetSize(int width, int height);
    void SetSkip(int x, int y);

    void Load(void);

    virtual void Pulse(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod);

    void Init(void);

    QString m_Filepattern;

    QValueVector<MythImage *> m_Images;

    int m_Delay;
    int m_LowNum;
    int m_HighNum;
 
    int m_ForceW;
    int m_ForceH;

    unsigned int m_CurPos;
    QTime m_LastDisplay;
};

#endif
