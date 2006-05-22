#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qvaluevector.h>

#include "mythuitype.h"
#include "mythimage.h"

class MythScreenType;

class MythUIImage : public MythUIType
{
  public:
    MythUIImage(const QString &filepattern, int low, int high, int delayms,
                MythUIType *parent, const char *name);
    MythUIImage(const QString &filename, MythUIType *parent, const char *name);
    MythUIImage(MythUIType *parent, const char *name);
   ~MythUIImage();

    // doesn't load
    void SetFilename(const QString &filename);
    void SetFilepattern(const QString &filepattern, int low, int high);

    void SetDelay(int delayms);

    // load's original
    void ResetFilename();

    void SetImage(MythImage *img);
    void SetImages(QValueVector<MythImage *> &images);

    void SetSize(int width, int height);
    void SetSkip(int x, int y);

    void Reset(void);
    void Load(void);

    virtual void Pulse(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                          int alphaMod, QRect clipRect);

    void Init(void);
    void Clear(void);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    QString m_Filename;
    QString m_OrigFilename;

    QValueVector<MythImage *> m_Images;

    QPoint m_Skip;
    QSize  m_ForceSize;

    int m_Delay;
    int m_LowNum;
    int m_HighNum;

    unsigned int m_CurPos;
    QTime m_LastDisplay;

    bool m_NeedLoad;
};

#endif
