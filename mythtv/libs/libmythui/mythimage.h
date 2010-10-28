#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

#include <QImage>
#include <QMutex>
#include <QPixmap>
#include <QImageReader>

#include "mythpainter.h"

enum ReflectAxis {ReflectHorizontal, ReflectVertical};
enum FillDirection {FillLeftToRight, FillTopToBottom};

class QNetworkReply;
class MythUIHelper;

class MPUBLIC MythImageReader: public QImageReader
{
  public:
    MythImageReader(const QString &fileName);
   ~MythImageReader();

  private:
    QString        m_fileName;
    QNetworkReply *m_networkReply;
};

class MPUBLIC MythImage : public QImage
{
  public:
    MythImage(MythPainter *parent);

    MythPainter* GetParent(void)        { return m_Parent;   }
    void SetParent(MythPainter *parent) { m_Parent = parent; }
    void UpRef(void);
    bool DownRef(void);

    int RefCount(void);

    virtual void SetChanged(bool change = true) { m_Changed = change; }
    bool IsChanged() const { return m_Changed; }

    bool IsGradient() const { return m_isGradient; }
    bool IsReflected() const { return m_isReflected; }

    void SetToYUV(void) { m_isYUV = true; }
    void ConvertToYUV(void);

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    bool Load(MythImageReader &reader);
    bool Load(const QString &filename, bool scale = true);

    void Resize(const QSize &newSize, bool preserveAspect = false);
    void Reflect(ReflectAxis axis, int shear, int scale, int length,
                 int spacing = 0);
    void ToGreyscale();

    /**
     * @brief Create a gradient image.
     * @param size The size of the image.
     * @param begin The beginning colour.
     * @param end The ending colour.
     * @return A MythImage filled with a gradient.
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

    void SetIsInCache(bool bCached);

  protected:
    virtual ~MythImage();
    static void MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha,
                             bool drawBoundary=true,
                             FillDirection direction = FillTopToBottom);

    bool m_Changed;
    MythPainter *m_Parent;

    int    m_RefCount;
    QMutex m_RefCountLock;

    bool m_isGradient;
    QColor m_gradBegin;
    QColor m_gradEnd;
    int m_gradAlpha;
    FillDirection m_gradDirection;

    bool m_isReflected;
    bool m_isYUV;

    unsigned int m_imageId;

    QString m_FileName;

    static MythUIHelper *m_ui;
    bool m_cached;
};

#endif

