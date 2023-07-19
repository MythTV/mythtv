#ifndef MYTHPAINTER_H_
#define MYTHPAINTER_H_

#include <QMap>
#include <QString>
#include <QTextLayout>
#include <QWidget>
#include <QPaintDevice>
#include <QMutex>
#include <QSet>

class QRect;
class QRegion;
class QPoint;
class QColor;

#include "mythuiexp.h"

#include <list>
#include <memory>

#ifdef _MSC_VER
#  include <cstdint>    // int64_t
#endif

class MythFontProperties;
class MythImage;
class UIEffects;

using LayoutVector = QVector<QTextLayout *>;
using FormatVector = QVector<QTextLayout::FormatRange>;
using ProcSource = std::shared_ptr<QByteArray>;

class MUI_PUBLIC MythPainter : public QObject
{
    Q_OBJECT

  public:
    MythPainter();
    /** MythPainter destructor.
     *
     *  The MythPainter destructor does not cleanup, it is unsafe
     *  to do cleanup in the MythPainter destructor because
     *  DeleteImagePriv() is a pure virtual in this class. Instead
     *  children should call MythPainter::Teardown() for cleanup.
     */
    ~MythPainter() override = default;

    virtual QString GetName(void) = 0;
    virtual bool SupportsAnimation(void) = 0;
    virtual bool SupportsAlpha(void) = 0;
    virtual bool SupportsClipping(void) = 0;
    virtual void FreeResources(void) { }
    virtual void Begin(QPaintDevice* /*Parent*/) { }
    virtual void End() { }

    virtual void SetClipRect(QRect clipRect);
    virtual void SetClipRegion(const QRegion &clipRegion);
    virtual void Clear(QPaintDevice *device, const QRegion &region);

    virtual void DrawImage(QRect dest, MythImage *im, QRect src, int alpha) = 0;

    void DrawImage(int x, int y, MythImage *im, int alpha);
    void DrawImage(QPoint topLeft, MythImage *im, int alph);
    virtual void DrawProcedural(QRect /*Area*/, int /*Alpha*/,
                                const ProcSource& /*VertexSource*/,
                                const ProcSource& /*FragmentSource*/,
                                const QString& /*SourceHash*/) { }

    virtual void DrawText(QRect r, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          QRect boundRect);
    virtual void DrawTextLayout(QRect canvasRect,
                                const LayoutVector & layouts,
                                const FormatVector & formats,
                                const MythFontProperties &font, int alpha,
                                QRect destRect);
    virtual void DrawRect(QRect area, const QBrush &fillBrush,
                          const QPen &linePen, int alpha);
    virtual void DrawRoundRect(QRect area, int cornerRadius,
                               const QBrush &fillBrush, const QPen &linePen,
                               int alpha);
    virtual void DrawEllipse(QRect area, const QBrush &fillBrush,
                             const QPen &linePen, int alpha);

    virtual void PushTransformation([[maybe_unused]] const UIEffects &zoom,
                                    [[maybe_unused]] QPointF center = QPointF()) {};
    virtual void PopTransformation(void) { }

    /// Returns a blank reference counted image in the format required
    /// for the Draw functions for this painter.
    /// \note The reference count is set for one use, call DecrRef() to delete.
    MythImage *GetFormatImage();
    void DeleteFormatImage(MythImage *im);

    void SetDebugMode(bool showBorders, bool showNames)
    {
        m_showBorders = showBorders;
        m_showNames = showNames;
    }

    bool ShowBorders(void) const { return m_showBorders; }
    bool ShowTypeNames(void) const { return m_showNames; }

    void SetMaximumCacheSizes(int hardware, int software);

  protected:
    static void DrawTextPriv(MythImage *im, const QString &msg, int flags,
                             QRect r, const MythFontProperties &font);
    static void DrawRectPriv(MythImage *im, QRect area, int radius, int ellipse,
                             const QBrush &fillBrush, const QPen &linePen);

    MythImage *GetImageFromString(const QString &msg, int flags, QRect r,
                                  const MythFontProperties &font);
    MythImage *GetImageFromTextLayout(const LayoutVector & layouts,
                                      const FormatVector & formats,
                                      const MythFontProperties &font,
                                      QRect &canvas, QRect &dest);
    MythImage *GetImageFromRect(QRect area, int radius, int ellipse,
                                const QBrush &fillBrush,
                                const QPen &linePen);

    /// Creates a reference counted image, call DecrRef() to delete.
    virtual MythImage* GetFormatImagePriv(void) = 0;
    virtual void DeleteFormatImagePriv(MythImage *im) = 0;
    void ExpireImages(int64_t max = 0);

    // This needs to be called by classes inheriting from MythPainter
    // in the destructor.
    virtual void Teardown(void);

    void CheckFormatImage(MythImage *im);

    float m_frameTime { 0 };

    int m_hardwareCacheSize     { 0 };
    int m_maxHardwareCacheSize  { 0 };

  private:
    int64_t m_softwareCacheSize {0};
    int64_t m_maxSoftwareCacheSize {48LL * 1024 * 1024};

    QMutex           m_allocationLock;
    QSet<MythImage*> m_allocatedImages;

    QMap<QString, MythImage *> m_stringToImageMap;
    std::list<QString>         m_stringExpireList;

    bool m_showBorders          {false};
    bool m_showNames            {false};
};

#endif
