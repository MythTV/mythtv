#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QWaitCondition>

#include "mythuitype.h"
#include "mythimage.h"

class MythUIImagePrivate;
class MythScreenType;
class ImageLoadThread;

/**
 * \class MythUIImage
 *
 * \brief Image widget, displays a single image or multiple images in sequence
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIImage : public MythUIType
{
  public:
    MythUIImage(const QString &filepattern, int low, int high, int delayms,
                MythUIType *parent, const QString &name);
    MythUIImage(const QString &filename, MythUIType *parent, const QString &name);
    MythUIImage(MythUIType *parent, const QString &name);
   ~MythUIImage();

    QString GetFilename(void) { return m_Filename; }

    /** Must be followed by a call to Load() to load the image. */
    void SetFilename(const QString &filename);

    /** Must be followed by a call to Load() to load the image. */
    void SetFilepattern(const QString &filepattern, int low, int high);

    void SetImageCount(int low, int high);

    /** Should not be used unless absolutely necessary since it bypasses the
     *   image caching and threaded loaded mechanisms.
     */
    void SetImage(MythImage *img);

    /** Should not be used unless absolutely necessary since it bypasses the
     *   image caching and threaded loaded mechanisms.
     */
    void SetImages(QVector<MythImage *> &images);

    void SetDelay(int delayms);
    void SetDelays(QVector<int> delays);

    void Reset(void);
    bool Load(bool allowLoadInBackground = true, bool forceStat = false);

    bool IsGradient(void) const { return m_gradient; }

    virtual void Pulse(void);

    virtual void LoadNow(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    void Init(void);
    void Clear(void);
    MythImage* LoadImage(MythImageReader &imageReader, const QString &imFile,
                         QSize bForceSize, int cacheMode);
    bool LoadAnimatedImage(MythImageReader &imageReader, const QString &imFile,
                           QSize bForceSize, int mode);
    void customEvent(QEvent *event);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetSize(int width, int height);
    void SetSize(const QSize &size);
    void ForceSize(const QSize &size);

    void SetCropRect(int x, int y, int width, int height);
    void SetCropRect(const MythRect &rect);

    QString GenImageLabel(const QString &filename, int w, int h) const;
    QString GenImageLabel(int w, int h) const;

    QString m_Filename;
    QString m_OrigFilename;

    QHash<int, MythImage *> m_Images;
    QHash<int, int>         m_Delays;
    QMutex                  m_ImagesLock;

    static QHash<QString, MythUIImage *> m_loadingImages;
    static QMutex                        m_loadingImagesLock;
    static QWaitCondition                m_loadingImagesCond;

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
    int  m_reflectSpacing;

    MythImage *m_maskImage;
    bool m_isMasked;

    bool m_gradient;
    QColor m_gradientStart;
    QColor m_gradientEnd;
    uint m_gradientAlpha;
    FillDirection m_gradientDirection;

    bool m_preserveAspect;

    bool m_isGreyscale;

    MythUIImagePrivate *d;

    enum AnimationCycle {kCycleStart, kCycleReverse};
    AnimationCycle m_animationCycle;
    bool m_animationReverse;
    bool m_animatedImage;

    friend class MythThemeBase;
    friend class MythUIButtonListItem;
    friend class MythUIProgressBar;
    friend class MythUIEditBar;
    friend class MythUITextEdit;
    friend class ImageLoadThread;
};

#endif
