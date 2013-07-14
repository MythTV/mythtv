#ifndef MYTHUI_TYPES_H_
#define MYTHUI_TYPES_H_

#include <QObject>
#include <QRegion>
#include <QMap>
#include <QHash>
#include <QList>
#include <QFont>
#include <QColor>

#include "xmlparsebase.h"
#include "mythuianimation.h"
#include "mythrect.h"
#include "mythgesture.h"
#include "mythmedia.h"

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

#define QT_PAINTER      "qt"
#define OPENGL_PAINTER  "opengl"
#define AUTO_PAINTER    "auto"
#define D3D9_PAINTER    "d3d9"

typedef QHash<QString,QString> InfoMap;

/**
 * \defgroup MythUI MythTV User Interface Library
 *
 * MythUI has multiple components
 * - Widgets and Theme handlers
 * - Painters & Renderers
 * - Input handling
 *
 * \defgroup MythUI_Widgets MythUI Widget and theme handling
 * \ingroup MythUI
 *
 * \defgroup MythUI_Painters MythUI Painters and Renderers
 * \ingroup MythUI
 *
 * \defgroup MythUI_Input MythUI Input handling
 * \ingroup MythUI
 */

/**
 * \brief The base class on which all widgets and screens are based.
 *
 * N.B. Children are drawn/processed in order added
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIType : public QObject, public XMLParseBase
{
    Q_OBJECT

  public:
    MythUIType(QObject *parent, const QString &name);

    virtual void Reset(void);

    void AddChild(MythUIType *child);
    MythUIType *GetChild(const QString &name) const;
    MythUIType *GetChildAt(const QPoint &p, bool recursive=true,
                           bool focusable=true) const;
    QList<MythUIType *> *GetAllChildren(void);

    void DeleteChild(const QString &name);
    void DeleteChild(MythUIType *child);
    void DeleteAllChildren(void);

    // Check set dirty status
    bool NeedsRedraw(void) const;
    void ResetNeedsRedraw(void);
    void SetRedraw(void);

    void SetChildNeedsRedraw(MythUIType *child);

    // Check set if this can take focus
    bool CanTakeFocus(void) const;
    void SetCanTakeFocus(bool set = true);
    void SetFocusOrder(int);

    bool IsEnabled(void) const { return m_Enabled; }
    void SetEnabled(bool enable);

    bool MoveToTop(void);
    bool MoveChildToTop(MythUIType *child);

    void ActivateAnimations(MythUIAnimation::Trigger trigger);
    QList<MythUIAnimation*>* GetAnimations(void) { return &m_animations; }

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255,
              QRect clipRegion = QRect());

    /// Convenience method, calls SetPosition(const MythPoint&)
    /// Override that instead to change functionality.
    void SetPosition(int x, int y);
    virtual void SetPosition(const MythPoint &pos);
    virtual MythPoint GetPosition(void) const;
    virtual void SetSize(const QSize &size);
    virtual void SetMinSize(const MythPoint &size);
    virtual QSize GetMinSize(void) const;
    virtual void SetArea(const MythRect &rect);
    virtual void AdjustMinArea(int delta_x, int delta_y,
                               int delta_w, int delta_h);
    virtual void VanishSibling(void);
    virtual void SetMinAreaParent(MythRect actual_area, MythRect full_area,
                                  MythUIType *child);
    virtual void SetMinArea(const MythRect & rect);
    virtual MythRect GetArea(void) const;
    virtual MythRect GetFullArea(void) const;
    virtual void RecalculateArea(bool recurse = true);
    void ExpandArea(const MythRect &rect);

    virtual QRegion GetDirtyArea(void) const;

    bool IsVisible(bool recurse = false) const;
    virtual void SetVisible(bool visible);

    void MoveTo(QPoint destXY, QPoint speedXY);
    //FIXME: make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void) const;

    virtual bool keyPressEvent(QKeyEvent *);
    virtual bool gestureEvent(MythGestureEvent *);
    virtual void mediaEvent(MythMediaEvent *);

    MythFontProperties *GetFont(const QString &text) const;
    bool AddFont(const QString &text, MythFontProperties *fontProp);

    void SetHelpText(const QString &text) { m_helptext = text; }
    QString GetHelpText(void) const { return m_helptext; }

    void SetXMLLocation(const QString &filename, int where)
    { m_xmlLocation = QString("%1:%2").arg(filename).arg(where); }
    QString GetXMLLocation(void) const { return m_xmlLocation; }

    bool IsDeferredLoading(bool recurse = false) const;
    void SetDeferLoad(bool defer) { m_deferload = defer; }
    virtual void LoadNow(void);

    bool ContainsPoint(const QPoint &point) const;

    virtual MythPainter *GetPainter(void);
    void SetPainter(MythPainter *painter) { m_Painter = painter; }

    void SetCentre(UIEffects::Centre centre);
    void SetZoom(float zoom);
    void SetHorizontalZoom(float zoom);
    void SetVerticalZoom(float zoom);
    void SetAngle(float angle);
    void SetDependIsDefault(bool isDefault);
    void SetReverseDependence(bool reverse);
    void SetDependsMap(QMap<QString, QString> dependsMap);
    QMap<QString, QString> GetDependsMap() const { return m_dependsMap; }

  protected:
    virtual ~MythUIType();
    virtual void customEvent(QEvent *);

  public slots:
    void LoseFocus();
    bool TakeFocus();
    void Activate();
    void Hide(void);
    void Show(void);
    void Refresh(void);
    void UpdateDependState(bool isDefault);

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
    void DependChanged(bool isDefault);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRegion);

    void AddFocusableChildrenToList(QMap<int, MythUIType *> &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();

    int CalcAlpha(int alphamod);

    int NormX(const int width);
    int NormY(const int height);

    void ConnectDependants(bool recurse = false);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    QList<MythUIType *> m_ChildrenList;
    QMap<QString, QString> m_dependsMap;

    bool m_Visible;
    bool m_HasFocus;
    bool m_CanHaveFocus;
    bool m_Enabled;
    bool m_EnableInitiator;
    bool m_Initiator;
    bool m_Vanish;
    bool m_Vanished;
    bool m_IsDependDefault;
    bool m_ReverseDepend;

    int m_focusOrder;

    MythRect m_Area;
    MythRect m_MinArea;
    MythPoint m_MinSize;
//    QSize m_NormalSize;

    QRegion m_DirtyRegion;
    bool m_NeedsRedraw;

    UIEffects m_Effects;

    int m_AlphaChangeMode; // 0 - none, 1 - once, 2 - cycle
    int m_AlphaChange;
    int m_AlphaMin;
    int m_AlphaMax;

    bool m_Moving;
    QPoint m_XYDestination;
    QPoint m_XYSpeed;

    FontMap *m_Fonts;

    MythUIType *m_Parent;
    MythPainter *m_Painter;

    QList<MythUIAnimation*> m_animations;
    QString m_helptext;
    QString m_xmlLocation;

    bool m_deferload;

    QColor m_BorderColor;

    friend class XMLParseBase;
};


#endif
