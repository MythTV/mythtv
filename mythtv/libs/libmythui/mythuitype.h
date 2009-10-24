#ifndef MYTHUI_TYPES_H_
#define MYTHUI_TYPES_H_

#include <QObject>
#include <QRegion>
#include <QMap>
#include <QList>
#include <QFont>

#include "xmlparsebase.h"
#include "mythrect.h"

class MythImage;
class MythPainter;
class MythGestureEvent;
class FontMap;
class MythFontProperties;

class QEvent;
class QKeyEvent;

class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;
class MythUICheckBox;
class MythUISpinBox;
class MythUITextEdit;
class MythUIProgressBar;
class MythUIWebBrowser;


/**
 * Base UI type.  Children are drawn/processed in order added
 */
class MPUBLIC MythUIType : public QObject, public XMLParseBase
{
    Q_OBJECT

  public:
    MythUIType(QObject *parent, const QString &name);
    virtual ~MythUIType();

    virtual void Reset(void);

    void AddChild(MythUIType *child);
    MythUIType *GetChild(const QString &name);
    MythUIType *GetChildAt(const QPoint &p, bool recursive=true,
                           bool focusable=true);
    QList<MythUIType *> *GetAllChildren(void);

    void DeleteChild(const QString &name);
    void DeleteChild(MythUIType *child);
    void DeleteAllChildren(void);

    // Check set dirty status
    bool NeedsRedraw(void) const;
    void ResetNeedsRedraw(void);
    void SetRedraw();

    void SetChildNeedsRedraw(MythUIType *child);

    // Check set if this can take focus
    bool CanTakeFocus(void);
    void SetCanTakeFocus(bool set = true);
    void SetFocusOrder(int);

    bool IsEnabled(void) const { return m_Enabled; }
    void SetEnabled(bool enable);

    bool MoveToTop(void);
    bool MoveChildToTop(MythUIType *child);

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255,
              QRect clipRegion = QRect());

    virtual void SetPosition(int x, int y);
    virtual void SetPosition(const MythPoint &pos);
    virtual MythPoint GetPosition(void) const;
    virtual void SetSize(const QSize &size);
    virtual void SetArea(const MythRect &rect);
    virtual MythRect GetArea(void) const;
    virtual void RecalculateArea(bool recurse = true);
    void ExpandArea(const MythRect &rect);

    virtual QRegion GetDirtyArea(void) const;

    bool IsVisible(bool recurse = false) const;
    void SetVisible(bool visible);

    void MoveTo(QPoint destXY, QPoint speedXY);
    //FIXME: make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void) const;

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void gestureEvent(MythUIType *origtype, MythGestureEvent *ge);

    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);

    void SetHelpText(const QString &text) { m_helptext = text; }
    QString GetHelpText(void) const { return m_helptext; }

    bool IsDeferredLoading(bool recurse = false);
    void SetDeferLoad(bool defer) { m_deferload = defer; }
    virtual void LoadNow(void);

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
    void Enabling();
    void Disabling();
    void FinishedMoving();
    void FinishedFading();

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRegion);

    void AddFocusableChildrenToList(QMap<int, MythUIType *> &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();

    int CalcAlpha(int alphamod);

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
    bool m_Enabled;

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

    QString m_helptext;

    bool m_deferload;

    friend class XMLParseBase;
};


#endif
