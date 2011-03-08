#ifndef MYTHPAINTER_H_
#define MYTHPAINTER_H_

#include <QMap>
#include <QString>
#include <QWidget>
#include <QPaintDevice>
#include <QMutex>

class QRect;
class QRegion;
class QPoint;
class QColor;

//  #include "mythfontproperties.h"

#include "compat.h"

#include "mythuiexp.h"

class MythFontProperties;
class MythImage;

class MUI_PUBLIC MythPainter
{
  public:
    MythPainter()
      : m_Parent(0), m_CacheSize(0), m_ItemCacheSize(256),
        m_showBorders(false), m_showNames(false) { }
    virtual ~MythPainter();

    virtual QString GetName(void) = 0;
    virtual bool SupportsAnimation(void) = 0;
    virtual bool SupportsAlpha(void) = 0;
    virtual bool SupportsClipping(void) = 0;
    virtual void FreeResources(void) { }
    virtual void Begin(QPaintDevice *parent) { m_Parent = parent; }
    virtual void End() { m_Parent = NULL; }

    virtual void SetClipRect(const QRect &clipRect);
    virtual void SetClipRegion(const QRegion &clipRegion);
    virtual void Clear(QPaintDevice *device, const QRegion &region);

    QPaintDevice *GetParent(void) { return m_Parent; }

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha) = 0;

    void DrawImage(int x, int y, MythImage *im, int alpha);
    void DrawImage(const QPoint &topLeft, MythImage *im, int alph);

    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect);
    virtual void DrawRect(const QRect &area, const QBrush &fillBrush,
                          const QPen &linePen, int alpha);
    virtual void DrawRoundRect(const QRect &area, int cornerRadius,
                               const QBrush &fillBrush, const QPen &linePen,
                               int alpha);
    virtual void DrawEllipse(const QRect &area, const QBrush &fillBrush,
                             const QPen &linePen, int alpha);

    MythImage *GetFormatImage();
    void DeleteFormatImage(MythImage *im);

    void SetDebugMode(bool showBorders, bool showNames)
    {
        m_showBorders = showBorders;
        m_showNames = showNames;
    }

    bool ShowBorders(void) { return m_showBorders; }
    bool ShowTypeNames(void) { return m_showNames; }

  protected:
    void DrawTextPriv(MythImage *im, const QString &msg, int flags,
                      const QRect &r, const MythFontProperties &font);
    void DrawRectPriv(MythImage *im, const QRect &area, int radius, int ellipse,
                      const QBrush &fillBrush, const QPen &linePen);

    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);
    MythImage *GetImageFromRect(const QRect &area, int radius, int ellipse,
                                const QBrush &fillBrush,
                                const QPen &linePen);

    virtual MythImage* GetFormatImagePriv(void) = 0;
    virtual void DeleteFormatImagePriv(MythImage *im) = 0;
    virtual void ExpireImages(uint max = 0);

    void CheckFormatImage(MythImage *im);
    void IncreaseCacheSize(QSize size);
    void DecreaseCacheSize(QSize size);

    QPaintDevice     *m_Parent;
    int               m_CacheSize;
    static int        m_MaxCacheSize;
    int               m_ItemCacheSize;
    QList<MythImage*> m_allocatedImages;
    QMutex            m_allocationLock;

    QMap<QString, MythImage *> m_StringToImageMap;
    std::list<QString>         m_StringExpireList;

    bool m_showBorders;
    bool m_showNames;
};

#endif
