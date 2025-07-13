#ifndef MYTHUI_TYPES_H_
#define MYTHUI_TYPES_H_

#include <QObject>
#include <QRegion>
#include <QMap>
#include <QList>
#include <QPair>
#include <QFont>
#include <QColor>

#include "xmlparsebase.h"
#include "mythuianimation.h"
#include "mythrect.h"

class MythPainter; // TODO: Should be an include but we first need to sort out the video scanner UI dependency mess

class MythImage;
class MythGestureEvent;
class FontMap;
class MythFontProperties;

class QEvent;
class QKeyEvent;
class QInputMethodEvent;
class MythGestureEvent;
class MythMediaEvent;

class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;
class MythUICheckBox;
class MythUISpinBox;
class MythUITextEdit;
class MythUIProgressBar;

using FocusInfoType = QMultiMap<int, MythUIType *>;

// For non-class, static class, or lambda function callbacks.
using MythUICallbackNMF = std::function<void(void)>;
// For class member function callbacks.
using MythUICallbackMF  = void (QObject::*)(void);
using MythUICallbackMFc = void (QObject::*)(void) const;

Q_DECLARE_METATYPE(MythUICallbackNMF)
Q_DECLARE_METATYPE(MythUICallbackMF)
Q_DECLARE_METATYPE(MythUICallbackMFc)

// Templates for determining if an argument is a "Pointer to a
// Member Function"
template<typename Func> struct FunctionPointerTest
{ enum : std::uint8_t {MemberFunction = false, MemberConstFunction = false}; };
template<class Obj, typename Ret, typename... Args> struct FunctionPointerTest<Ret (Obj::*) (Args...)>
{ enum : std::uint8_t {MemberFunction = true, MemberConstFunction = false}; };
template<class Obj, typename Ret, typename... Args> struct FunctionPointerTest<Ret (Obj::*) (Args...) const>
{ enum {MemberFunction = false, MemberConstFunction = true}; };

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
    MythUIType *GetChildAt(QPoint p, bool recursive=true,
                           bool focusable=true) const;
    QList<MythUIType *> *GetAllChildren(void);
    QList<MythUIType *> GetAllDescendants(void);

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
    void SetFocusOrder(int order);
    void SetFocusedName(const QString & widgetname);
    QString GetFocusedName(void) const { return m_focusedName; }

    bool IsEnabled(void) const { return m_enabled; }
    void SetEnabled(bool enable);

    bool MoveToTop(void);
    bool MoveChildToTop(MythUIType *child);

    void ActivateAnimations(MythUIAnimation::Trigger trigger);
    QList<MythUIAnimation*>* GetAnimations(void) { return &m_animations; }

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255,
              QRect clipRect = QRect());

    /// Convenience method, calls SetPosition(const MythPoint&)
    /// Override that instead to change functionality.
    void SetPosition(int x, int y);
    void SetPosition(QPoint point);
    virtual void SetPosition(const MythPoint &point);
    virtual MythPoint GetPosition(void) const;
    virtual void SetSize(QSize size);
    virtual void SetMinSize(const MythPoint &size);
    virtual QSize GetMinSize(void) const;
    virtual void SetArea(const MythRect &rect);
    virtual void AdjustMinArea(int delta_x, int delta_y,
                               int delta_w, int delta_h);
    virtual void VanishSibling(void);
    virtual void SetMinAreaParent(MythRect actual_area, MythRect allowed_area,
                                  MythUIType *child);
    virtual void SetMinArea(const MythRect & rect);
    virtual MythRect GetArea(void) const;
    virtual MythRect GetFullArea(void) const;
    virtual void RecalculateArea(bool recurse = true);
    void ExpandArea(QRect rect);

    virtual QRegion GetDirtyArea(void) const;

    bool IsVisible(bool recurse = false) const;
    virtual void SetVisible(bool visible);

    void MoveTo(QPoint destXY, QPoint speedXY);
    //FIXME: make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void) const;

    // This class is not based on QWidget, so this is a new function
    // and not an override of QWidget::keyPressEvent.
    virtual bool keyPressEvent(QKeyEvent *event);
    virtual bool inputMethodEvent(QInputMethodEvent *event);
    virtual bool gestureEvent(MythGestureEvent *event);
    virtual void mediaEvent(MythMediaEvent *event);

    MythFontProperties *GetFont(const QString &text) const;
    bool AddFont(const QString &text, MythFontProperties *fontProp);

    void SetHelpText(const QString &text) { m_helptext = text; }
    QString GetHelpText(void) const { return m_helptext; }

    void SetXMLLocation(const QString &filename, int where)
    { m_xmlLocation = QString("%1:%2").arg(filename).arg(where); }
    QString GetXMLLocation(void) const { return m_xmlLocation; }

    void SetXMLName(const QString &name) { m_xmlName = name; }
    QString GetXMLName(void) const { return m_xmlName; }

    bool IsDeferredLoading(bool recurse = false) const;
    void SetDeferLoad(bool defer) { m_deferload = defer; }
    virtual void LoadNow(void);

    bool ContainsPoint(QPoint point) const;

    virtual MythPainter *GetPainter(void);
    void SetPainter(MythPainter *painter) { m_painter = painter; }

    void SetCentre(UIEffects::Centre centre);
    void SetZoom(float zoom);
    void SetHorizontalZoom(float zoom);
    void SetVerticalZoom(float zoom);
    void SetAngle(float angle);
    void SetDependIsDefault(bool isDefault);
    void SetReverseDependence(MythUIType *dependee, bool reverse);
    void SetDependsMap(QMap<QString, QString> dependsMap);
    QMap<QString, QString> GetDependsMap() const { return m_dependsMap; }

  protected:
    ~MythUIType() override;
    void customEvent(QEvent *event) override; // QObject

  public slots:
    void LoseFocus(void);
    bool TakeFocus(void);
    void Activate(void);
    void Hide(void);
    void Show(void);
    void Refresh(void);
    void UpdateDependState(bool isDefault);
    void UpdateDependState(MythUIType *dependee, bool isDefault);

  signals:
    void RequestUpdate(void);
    void RequestRegionUpdate(const QRect &);
    void TakingFocus(void);
    void LosingFocus(void);
    void VisibilityChanged(bool Visible);
    void Showing(void);
    void Hiding(void);
    void Enabling(void);
    void Disabling(void);
    void FinishedMoving(void);
    void FinishedFading(void);
    void DependChanged(bool isDefault);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    void AddFocusableChildrenToList(FocusInfoType &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();

    int CalcAlpha(int alphamod) const;

    static int NormX(int width);
    static int NormY(int height);

    void ConnectDependants(bool recurse = false);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    QList<MythUIType *> m_childrenList;
    QMap<QString, QString> m_dependsMap;
    // the number of dependencies is assumed to be small (1 or 2 elements on average)
    // so we use a QList as we want the element ordered in the order they were defined
    // and speed isn't going to be a factor
    QList< QPair<MythUIType *, bool> >m_dependsValue;
    QList<int> m_dependOperator;

    bool         m_visible         {true};
    bool         m_hasFocus        {false};
    bool         m_canHaveFocus    {false};
    bool         m_enabled         {true};
    bool         m_enableInitiator {false};
    bool         m_initiator       {false};
    bool         m_vanish          {false};
    bool         m_vanished        {false};
    bool         m_isDependDefault {false};
    QMap<MythUIType *, bool> m_reverseDepend;

    QString      m_focusedName;
    int          m_focusOrder      {0};

    MythRect     m_area            {0,0,0,0};
    MythRect     m_minArea         {0,0,0,0};
    MythPoint    m_minSize;

    QRegion      m_dirtyRegion     {0,0,0,0};
    bool         m_needsRedraw     {false};

    UIEffects    m_effects;

    int          m_alphaChangeMode {0}; // 0 - none, 1 - once, 2 - cycle
    int          m_alphaChange     {0};
    int          m_alphaMin        {0};
    int          m_alphaMax        {255};

    bool         m_moving          {false};
    QPoint       m_xyDestination   {0,0};
    QPoint       m_xySpeed         {0,0};

    FontMap     *m_fonts           {nullptr};

    MythUIType  *m_parent          {nullptr};
    MythPainter *m_painter         {nullptr};

    QList<MythUIAnimation*> m_animations;
    QString      m_helptext;

    QString      m_xmlName;
    QString      m_xmlLocation;

    bool         m_deferload       {false};

    QColor       m_borderColor     {Qt::black};

    friend class MythScreenType;
    friend class XMLParseBase;
};


#endif
