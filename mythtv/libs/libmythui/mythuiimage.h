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
    ImageProperties() = default;
    ImageProperties(const ImageProperties& other);
   ~ImageProperties();

    ImageProperties &operator=(const ImageProperties &other);

    void SetMaskImage(MythImage *image);
    void SetMaskImageFilename(const QString &filename)
    {
        m_maskImageFilename=filename;
    }
    QString GetMaskImageFilename()
    {
        return m_maskImageFilename;
    }
    QRect GetMaskImageRect(void)
    {
        QRect rect;
        if (m_maskImage)
            rect = m_maskImage->rect();
        return rect;
    }
    QImage GetMaskImageSubset(QRect imageArea)
    {
        if (m_maskImage)
            return m_maskImage->copy(imageArea);

        QImage img(imageArea.size(), QImage::Format_ARGB32);
        img.fill(0xFFFFFFFF);
        return img;
    }

    QString m_filename;

    MythRect m_cropRect       {0,0,0,0};
    QSize    m_forceSize      {0,0};

    bool     m_preserveAspect {false};
    bool     m_isGreyscale    {false};
    bool     m_isReflected    {false};
    bool     m_isMasked       {false};
    bool     m_isOriented     {false};

    ReflectAxis m_reflectAxis {ReflectAxis::Vertical};
    int m_reflectScale        {100};
    int m_reflectLength       {100};
    int m_reflectShear        {0};
    int m_reflectSpacing      {0};

    int m_orientation         {1};

    bool m_isThemeImage       {false};

  private:
    void Init(void);
    void Copy(const ImageProperties &other);

    MythImage *m_maskImage {nullptr};
    QString m_maskImageFilename;
};

using AnimationFrame = QPair<MythImage *, std::chrono::milliseconds>;
using AnimationFrames = QVector<AnimationFrame>;

/**
 * \class MythUIImage
 *
 * \brief Image widget, displays a single image or multiple images in sequence
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIImage : public MythUIType
{
    Q_OBJECT

  public:
    MythUIImage(const QString &filepattern, int low, int high, std::chrono::milliseconds delay,
                MythUIType *parent, const QString &name);
    MythUIImage(const QString &filename, MythUIType *parent, const QString &name);
    MythUIImage(MythUIType *parent, const QString &name);
   ~MythUIImage() override;

    QString GetFilename(void) { return m_filename; }

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

    void SetDelay(std::chrono::milliseconds delayms);
    void SetDelays(const QVector<std::chrono::milliseconds>& delays);

    void Reset(void) override; // MythUIType
    bool Load(bool allowLoadInBackground = true, bool forceStat = false);

    void Pulse(void) override; // MythUIType

    void LoadNow(void) override; // MythUIType

    void SetOrientation(int orientation);

  signals:
    void LoadComplete();

  protected:
    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    void Clear(void);

    void SetAnimationFrames(const AnimationFrames& frames);
    void customEvent(QEvent *event) override; // MythUIType

    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void SetSize(int width, int height);
    void SetSize(QSize size) override; // MythUIType
    void ForceSize(QSize size);

    void SetCropRect(int x, int y, int width, int height);
    void SetCropRect(const MythRect &rect);

    void FindRandomImage(void);

    QString m_filename;
    QString m_origFilename;

    QHash<int, MythImage *> m_images;
    QHash<int, std::chrono::milliseconds> m_delays;
    QMutex                  m_imagesLock;

    std::chrono::milliseconds m_delay { -1ms };
    int m_lowNum  {0};
    int m_highNum {0};

    unsigned int    m_curPos             {0};
    QTime           m_lastDisplay        {QTime::currentTime()};

    bool            m_needLoad           {false};

    ImageProperties m_imageProperties;

    int             m_runningThreads     {0};

    bool            m_showingRandomImage {false};
    QString         m_imageDirectory;
    QStringList     m_imageList;
    int             m_imageListIndex     {0};

    MythUIImagePrivate *d                {nullptr}; // NOLINT(readability-identifier-naming)

    enum AnimationCycle : std::uint8_t {kCycleStart, kCycleReverse};
    AnimationCycle  m_animationCycle     {kCycleStart};
    bool            m_animationReverse   {false};
    bool            m_animatedImage      {false};

    friend class MythUIImagePrivate;
    friend class MythThemeBase;
    friend class MythUIButtonListItem;
    friend class MythUIProgressBar;
    friend class MythUIEditBar;
    friend class MythUITextEdit;
    friend class ImageLoadThread;
    friend class MythUIGuideGrid;

  private:
    Q_DISABLE_COPY(MythUIImage)
};

#endif
