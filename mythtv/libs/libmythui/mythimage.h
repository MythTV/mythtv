#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

#include "mythpainter.h"

#include <QImage>
#include <QPixmap>

enum ReflectAxis {ReflectHorizontal, ReflectVertical};
enum FillDirection {FillLeftToRight, FillTopToBottom};

class MythImage : public QImage
{
  public:
    MythImage(MythPainter *parent);

    void UpRef(void);
    bool DownRef(void);

    virtual void SetChanged(bool change = true) { m_Changed = change; }
    bool IsChanged() const { return m_Changed; }

    bool IsGradient() const { return m_isGradient; }
    bool IsReflected() const { return m_isReflected; }

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    // *NOTE* *DELETES* img!
    static MythImage *FromQImage(QImage **img);

    bool LoadNoScale(const QString &filename);
    bool Load(const QString &filename);

    void Resize(const QSize &newSize, bool preserveAspect = false);
    void Reflect(ReflectAxis axis, int shear, int scale, int length);

    /**
     * @brief Create a gradient image.
     * @param size The size of the image.
     * @param begin The beginning colour.
     * @param end The ending colour.
     * @return A MythImage filled with a gradient.
     */
    static MythImage *Gradient(const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha,
                               FillDirection direction = FillTopToBottom);

    void SetID(unsigned int id) { m_imageId = id; }
    unsigned int GetID(void) const { return m_imageId; }

    void SetFileName(QString fname) { m_FileName = fname; }
    QString GetFileName(void) const { return m_FileName; }

  protected:
    static void MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha,
                             bool drawBoundary=true,
                             FillDirection direction = FillTopToBottom);

    virtual ~MythImage();

    bool m_Changed;
    MythPainter *m_Parent;

    int m_RefCount;

    bool m_isGradient;
    QColor m_gradBegin;
    QColor m_gradEnd;
    int m_gradAlpha;
    FillDirection m_gradDirection;

    bool m_isReflected;

    unsigned int m_imageId;

    QString m_FileName;
};

#endif

