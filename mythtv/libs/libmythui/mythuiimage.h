#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <QDateTime>

#include "mythuitype.h"
#include "mythimage.h"

class MythScreenType;

class MythUIImage : public MythUIType
{
  public:
    MythUIImage(const QString &filepattern, int low, int high, int delayms,
                MythUIType *parent, const QString &name);
    MythUIImage(const QString &filename, MythUIType *parent, const QString &name);
    MythUIImage(MythUIType *parent, const QString &name);
   ~MythUIImage();

    // doesn't load
    void SetFilename(const QString &filename);
    void SetFilepattern(const QString &filepattern, int low, int high);

    void SetImageCount(int low, int high);
    void SetDelay(int delayms);
    void SetImage(MythImage *img);
    void SetImages(QVector<MythImage *> &images);

    void SetSize(int width, int height);
    void SetSize(const QSize &size);
    void ForceSize(const QSize &size);
    void SetCropRect(int x, int y, int width, int height);
    void SetCropRect(const MythRect &rect);

    void Reset(void);
    bool Load(void);

    bool IsGradient(void) { return m_gradient; }

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

    QVector<MythImage *> m_Images;

    MythRect m_cropRect;
    QSize m_ForceSize;

    int m_Delay;
    int m_LowNum;
    int m_HighNum;

    unsigned int m_CurPos;
    QTime m_LastDisplay;

    bool m_NeedLoad;

    bool m_isReflected;
    ReflectAxis m_reflectAxis;
    int  m_reflectShear;
    int  m_reflectScale;
    int  m_reflectLength;

    bool m_gradient;
    QColor m_gradientStart;
    QColor m_gradientEnd;
    uint m_gradientAlpha;
    FillDirection m_gradientDirection;

    bool m_preserveAspect;
};

#endif
