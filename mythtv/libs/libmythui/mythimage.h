#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

#include "mythpainter.h"

#include <qimage.h>
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
    bool IsChanged() { return m_Changed; }

    bool IsGradient() { return m_isGradient; }

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    // *NOTE* *DELETES* img!
    static MythImage *FromQImage(QImage **img);

    bool Load(const QString &filename);

    void Resize(const QSize &newSize);
    void Reflect(ReflectAxis axis, int shear, int scale, int length);

    /**
     * @brief Create a gradient image.
     * @param size The size of the image.
     * @param begin The beginning colour.
     * @param end The ending colour.
     * @return A MythImage filled with a gradient.
     */
    static MythImage *Gradient(const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha);

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
};

#endif

