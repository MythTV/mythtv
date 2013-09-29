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
    MythImageReader(const QString &fileName);
   ~MythImageReader();

  private:
    QString        m_fileName;
    QNetworkReply *m_networkReply;
};

class MUI_PUBLIC MythImage : public QImage, public ReferenceCounter
{
  public:
    /// Creates a reference counted image, call DecrRef() to delete.
    MythImage(MythPainter *parent, const char *name = "MythImage");

    MythPainter* GetParent(void)        { return m_Parent;   }
    void SetParent(MythPainter *parent) { m_Parent = parent; }

    virtual int IncrRef(void);
    virtual int DecrRef(void);

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
     * @param size The size of the image.
     * @param begin The beginning colour.
     * @param end The ending colour.
     * @return A reference counted image, call DecrRef() to delete.
     */
    static MythImage *Gradient(MythPainter *painter,
                               const QSize & size, const QColor &beg,
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
        return (m_cached) ? byteCount() : 0;
    }

  protected:
    virtual ~MythImage();
    static void MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha,
                             bool drawBoundary=true,
                             FillDirection direction = FillTopToBottom);

    bool m_Changed;
    MythPainter *m_Parent;

    bool m_isGradient;
    QColor m_gradBegin;
    QColor m_gradEnd;
    int m_gradAlpha;
    FillDirection m_gradDirection;

    bool m_isOriented;
    bool m_isReflected;
    bool m_isYUV;

    unsigned int m_imageId;

    QString m_FileName;

    bool m_cached;

    static MythUIHelper *s_ui;
};

#endif

