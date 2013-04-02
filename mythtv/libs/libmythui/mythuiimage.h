#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include <QPair>

#include "mythuitype.h"
#include "mythuihelper.h"
#include "mythimage.h"

class MythUIImagePrivate;
class MythScreenType;
class ImageLoadThread;

/*!
 * \class ImageLoader
 * \note ImageProperties must only be used in the UI thread.
 */
class ImageProperties
{
  public:
    ImageProperties();
    ImageProperties(const ImageProperties& other);
   ~ImageProperties();

    ImageProperties &operator=(const ImageProperties &other);

    void SetMaskImage(MythImage *image);
    QRect GetMaskImageRect(void)
    {
        QRect rect;
        if (maskImage)
            rect = maskImage->rect();
        return rect;
    }
    QImage GetMaskImageSubset(const QRect &imageArea)
    {
        if (maskImage)
            return maskImage->copy(imageArea);

        QImage img(imageArea.size(), QImage::Format_ARGB32);
        img.fill(0xFFFFFFFF);
        return img;
    }

    QString filename;

    MythRect cropRect;
    QSize forceSize;

    bool preserveAspect;
    bool isGreyscale;
    bool isReflected;
    bool isMasked;
    bool isOriented;

    ReflectAxis reflectAxis;
    int reflectScale;
    int reflectLength;
    int reflectShear;
    int reflectSpacing;

    int orientation;

  private:
    void Init(void);
    void Copy(const ImageProperties &other);

    MythImage *maskImage;
};

typedef QPair<MythImage *, int> AnimationFrame;
typedef QVector<AnimationFrame> AnimationFrames;

/**
 * \class MythUIImage
 *
 * \brief Image widget, displays a single image or multiple images in sequence
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIImage : public MythUIType
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
    void SetImages(QVector<MythImage *> *images);

    void SetDelay(int delayms);
    void SetDelays(QVector<int> delays);

    void Reset(void);
    bool Load(bool allowLoadInBackground = true, bool forceStat = false);

    virtual void Pulse(void);

    virtual void LoadNow(void);

    void SetOrientation(int orientation);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    void Init(void);
    void Clear(void);

    void SetAnimationFrames(AnimationFrames frames);
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

    QString m_Filename;
    QString m_OrigFilename;

    QHash<int, MythImage *> m_Images;
    QHash<int, int>         m_Delays;
    QMutex                  m_ImagesLock;

    int m_Delay;
    int m_LowNum;
    int m_HighNum;

    unsigned int m_CurPos;
    QTime m_LastDisplay;

    bool m_NeedLoad;

    ImageProperties m_imageProperties;

    int m_runningThreads;

    MythUIImagePrivate *d;

    enum AnimationCycle {kCycleStart, kCycleReverse};
    AnimationCycle m_animationCycle;
    bool m_animationReverse;
    bool m_animatedImage;

    friend class MythUIImagePrivate;
    friend class MythThemeBase;
    friend class MythUIButtonListItem;
    friend class MythUIProgressBar;
    friend class MythUIEditBar;
    friend class MythUITextEdit;
    friend class ImageLoadThread;

  private:
    Q_DISABLE_COPY(MythUIImage)
};

#endif
