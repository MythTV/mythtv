#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

// C++ headers
#include <utility>

// QT headers
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QPixmap>

#include "libmythbase/referencecounter.h"
#include "libmythui/mythpainter.h"

enum class ReflectAxis : std::uint8_t {Horizontal, Vertical};
enum class FillDirection : std::uint8_t {LeftToRight, TopToBottom};
enum class BoundaryWanted : std::uint8_t {No, Yes};

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
    explicit MythImage(MythPainter *parent, const char *name = "MythImage");

    MythPainter* GetParent(void)        { return m_parent;   }
    void SetParent(MythPainter *parent) { m_parent = parent; }

    int IncrRef(void) override; // ReferenceCounter
    int DecrRef(void) override; // ReferenceCounter

    virtual void SetChanged(bool change = true) { m_changed = change; }
    bool IsChanged() const { return m_changed; }

    bool IsGradient() const { return m_isGradient; }
    bool IsReflected() const { return m_isReflected; }
    bool IsOriented() const { return m_isOriented; }

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    bool Load(MythImageReader *reader);
    bool Load(const QString &filename);

    void Orientation(int orientation);
    void Resize(QSize newSize, bool preserveAspect = false);
    void Reflect(ReflectAxis axis, int shear, int scale, int length,
                 int spacing = 0);
    void ToGreyscale();

    int64_t GetSize(void)
    { return static_cast<int64_t>(bytesPerLine()) * height(); }

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
                               QSize size, const QColor &begin,
                               const QColor &end, uint alpha,
                               FillDirection direction = FillDirection::TopToBottom);

    void SetID(unsigned int id) { m_imageId = id; }
    unsigned int GetID(void) const { return m_imageId; }

    void SetFileName(QString fname) { m_fileName = std::move(fname); }
    QString GetFileName(void) const { return m_fileName; }

    void setIsReflected(bool reflected) { m_isReflected = reflected; }
    void setIsOriented(bool oriented) { m_isOriented = oriented; }

    void SetIsInCache(bool bCached);

    uint GetCacheSize(void) const
    {
        return (m_cached) ? sizeInBytes() : 0;
    }

  protected:
    ~MythImage() override;
    static void MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha,
                             BoundaryWanted drawBoundary = BoundaryWanted::Yes,
                             FillDirection direction = FillDirection::TopToBottom);

    bool           m_changed       {false};
    MythPainter   *m_parent        {nullptr};

    bool           m_isGradient    {false};
    QColor         m_gradBegin     {0x00, 0x00, 0x00};
    QColor         m_gradEnd       {0xFF, 0xFF, 0xFF};
    int            m_gradAlpha     {255};
    FillDirection  m_gradDirection {FillDirection::TopToBottom};

    bool           m_isOriented    {false};
    bool           m_isReflected   {false};

    unsigned int   m_imageId       {0};

    QString        m_fileName;

    bool           m_cached        {false};

    static MythUIHelper *s_ui;
};

#endif

