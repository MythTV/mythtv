#ifndef MYTHUI_TYPES_H_
#define MYTHUI_TYPES_H_

#include <QObject>
#include <QImage>
#include <QList>
#include <QFont>
#include <QEvent>
#include <QKeyEvent>

#include "xmlparsebase.h"
#include "mythrect.h"

class MythImage;
class MythPainter;
class MythGestureEvent;
class FontMap;
class MythFontProperties;

/**
 * Base UI type.  Children are drawn/processed in order added
 */
class MythUIType : public QObject, public XMLParseBase
{
    Q_OBJECT

  public:
    MythUIType(QObject *parent, const QString &name);
    virtual ~MythUIType();

    void AddChild(MythUIType *child);
    MythUIType *GetChild(const QString &name);
    MythUIType *GetChildAt(const QPoint &p);
    QList<MythUIType *> *GetAllChildren(void);

    void DeleteChild(const QString &name);
    void DeleteChild(MythUIType *child);
    void DeleteAllChildren(void);

    // Check set dirty status
    bool NeedsRedraw(void);
    void SetRedraw();

    void SetChildNeedsRedraw(MythUIType *child);

    // Check set if this can take focus
    bool CanTakeFocus(void);
    void SetCanTakeFocus(bool set = true);
    void SetFocusOrder(int);

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255,
              QRect clipRegion = QRect());

    virtual void SetPosition(int x, int y);
    virtual void SetPosition(const QPoint &pos);
    virtual QPoint GetPosition(void) const;
    virtual void SetSize(const QSize &size);
    virtual void SetArea(const MythRect &rect);
    virtual MythRect GetArea(void) const;
    void ExpandArea(const MythRect &rect);
    virtual void Rescale(const float hscale, const float vscale);

    virtual QRegion GetDirtyArea(void) const;

    QString cutDown(const QString &data, QFont *font,
                    bool multiline = false, int overload_width = -1,
                    int overload_height = -1);

    bool IsVisible(void);
    void SetVisible(bool visible);

    void MoveTo(QPoint destXY, QPoint speedXY);
    //FIXME: make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void);

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void gestureEvent(MythUIType *origtype, MythGestureEvent *ge);

    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);

  protected:
    virtual void customEvent(QEvent *);

  public slots:
    void LoseFocus();
    bool TakeFocus();
    void Activate();
    void Hide(void);
    void Show(void);
    void Refresh(void);

  signals:
    void RequestUpdate();
    void RequestUpdate(const QRect &);
    void RequestRegionUpdate(const QRect &);
    void TakingFocus();
    void LosingFocus();
    void Showing();
    void Hiding();
    void FinishedMoving();
    void FinishedFading();

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRegion);

    void AddFocusableChildrenToList(QMap<int, MythUIType *> &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();

    int CalcAlpha(int alphamod);

    QFont CreateQFont(const QString &face, int pointSize = 12,
                      int weight = QFont::Normal, bool italic = FALSE);
    QPoint NormPoint(const QPoint &point);
    int NormX(const int width);
    int NormY(const int height);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    QList<MythUIType *> m_ChildrenList;

    bool m_Visible;
    bool m_HasFocus;
    bool m_CanHaveFocus;

    int m_focusOrder;

    MythRect m_Area;

    QRegion m_DirtyRegion;
    bool m_NeedsRedraw;

    int m_Alpha;
    int m_AlphaChangeMode; // 0 - none, 1 - once, 2 - cycle
    int m_AlphaChange;
    int m_AlphaMin;
    int m_AlphaMax;

    bool m_Moving;
    QPoint m_XYDestination;
    QPoint m_XYSpeed;

    FontMap *m_Fonts;

    MythUIType *m_Parent;

  friend class XMLParseBase;
};


#endif
