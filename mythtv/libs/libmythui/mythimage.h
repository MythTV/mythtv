#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

#include <QImage>
#include <QMutex>
#include <QPixmap>
#include <QImageReader>

#include "referencecounter.h"
#include "mythpainter.h"

enum ReflectAxis {ReflectHorizontal, ReflectVertical};
enum FillDirection {FillLeftToRight, FillTopToBottom};

class QNetworkReply;
class MythUIHelper;

class MUI_PUBLIC MythImageReader: public QImageReader
{
  public:
    explicit MythImageReader(QString fileName);
   ~MythImageReader();

  private:
    QString        m_fileName;
    QNetworkReply *m_networkReply {nullptr};
};

class MUI_PUBLIC MythImage : public QImage, public ReferenceCounter
{
  public:
    static QImage ApplyExifOrientation(QImage &image, int orientation);

    /// Creates a reference counted image, call DecrRef() to delete.
    MythImage(MythPainter *parent, const char *name = "MythImage");

    MythPainter* GetParent(void)        { return m_Parent;   }
    void SetParent(MythPainter *parent) { m_Parent = parent; }

    int IncrRef(void) override; // ReferenceCounter
    int DecrRef(void) override; // ReferenceCounter

    virtual void SetChanged(bool change = true) { m_Changed = change; }
    bool IsChanged() const { return m_Changed; }

    bool IsGradient() const { return m_isGradient; }
    bool IsReflected() const { return m_isReflected; }
    bool IsOriented() const { return m_isOriented; }

    void SetToYUV(void) { m_isYUV = true; }
    void ConvertToYUV(void);

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    bool Load(MythImageReader *reader);
    bool Load(const QString &filename);

    void Orientation(int orientation);
    void Resize(const QSize &newSize, bool preserveAspect = false);
    void Reflect(ReflectAxis axis, int shear, int scale, int length,
                 int spacing = 0);
    void ToGreyscale();

    /**
     * @brief Create a gradient image.
     * @param painter The painter on which the gradient should be draw.
     * @param size The size of the image.
     * @param begin The beginning colour.
     * @param end The ending colour.
     * @param alpha The opacity of the gradient.
     * @param direction Whether the gradient should be left-to-right or top-to-bottom.
     * @return A reference counted image, call DecrRef() to delete.
     */
    static MythImage *Gradient(MythPainter *painter,
                               const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha,
                               FillDirection direction = FillTopToBottom);

    void SetID(unsigned int id) { m_imageId = id; }
    unsigned int GetID(void) const { return m_imageId; }

    void SetFileName(QString fname) { m_FileName = fname; }
    QString GetFileName(void) const { return m_FileName; }

    void setIsReflected(bool reflected) { m_isReflected = reflected; }
    void setIsOriented(bool oriented) { m_isOriented = oriented; }

    void SetIsInCache(bool bCached);

    uint GetCacheSize(void) const
    {
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
        return (m_cached) ? byteCount() : 0;
#else
        return (m_cached) ? sizeInBytes() : 0;
#endif
    }

  protected:
    virtual ~MythImage();
    static void MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha,
                             bool drawBoundary=true,
                             FillDirection direction = FillTopToBottom);

    bool           m_Changed       {false};
    MythPainter   *m_Parent        {nullptr};

    bool           m_isGradient    {false};
    QColor         m_gradBegin     {"#000000"};
    QColor         m_gradEnd       {"#FFFFFF"};
    int            m_gradAlpha     {255};
    FillDirection  m_gradDirection {FillTopToBottom};

    bool           m_isOriented    {false};
    bool           m_isReflected   {false};
    bool           m_isYUV         {false};

    unsigned int   m_imageId       {0};

    QString        m_FileName;

    bool           m_cached        {false};

    static MythUIHelper *s_ui;
};

#endif

