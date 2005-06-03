#ifndef MYTHUI_TYPES_H_
#define MYTHUI_TYPES_H_

#include <qobject.h>
#include <qimage.h>
#include <qobjectlist.h>
#include <qptrlist.h>
#include <qvaluevector.h>
#include <qfont.h>

class MythImage;
class MythPainter;

/**
 * Base UI type.  Children are drawn/processed in order added
 */
class MythUIType : public QObject
{
    Q_OBJECT

  public:
    MythUIType(QObject *parent, const char *name);
    virtual ~MythUIType();

    void AddChild(MythUIType *child);
    MythUIType *GetChild(const char *name, const char *inherits = 0);
    QValueVector<MythUIType *> *GetAllChildren(void);

    // Check set dirty status
    bool NeedsRedraw(void);
    void SetRedraw(bool set = true);

    // Check set if this can take focus
    bool CanTakeFocus(void);
    void SetCanTakeFocus(bool set = true);

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    virtual void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255);

    virtual void SetPosition(QPoint pos);
    virtual void SetArea(QRect rect);
    virtual QRect GetArea(void);

    QString cutDown(const QString &data, QFont *font,
                    bool multiline = false, int overload_width = -1,
                    int overload_height = -1);

    bool IsVisible(void);

    void MoveTo(QPoint destXY, QPoint speedXY);
    // make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void);

    virtual bool keyPressEvent(QKeyEvent *);
    void setDebug(bool y_or_n){ m_debug_mode = y_or_n; }
    void setDebugColor(QColor c);

  protected:
    virtual void customEvent(QCustomEvent *);

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
    void AddFocusableChildrenToList(QPtrList<MythUIType> &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();
    void makeDebugImages();

    int CalcAlpha(int alphamod);

    QFont CreateFont(const QString &face, int pointSize = 12,
                     int weight = QFont::Normal, bool italic = FALSE);
    QRect NormRect(const QRect &rect);
    QPoint NormPoint(const QPoint &point);
    int NormX(const int width);
    int NormY(const int height);

    QValueVector<MythUIType *> m_ChildrenList;

    bool m_Visible;
    bool m_HasFocus;
    bool m_CanHaveFocus;

    QRect m_Area;

    bool m_NeedsRedraw;

    int m_Alpha;
    int m_AlphaChangeMode; // 0 - none, 1 - once, 2 - cycle
    int m_AlphaChange;
    int m_AlphaMin;
    int m_AlphaMax;

    bool m_Moving; 
    QPoint m_XYDestination;
    QPoint m_XYSpeed;

    MythUIType *m_Parent;

    bool        m_debug_mode;
    MythImage  *m_debug_hor_line;
    MythImage  *m_debug_ver_line;
    QColor      m_debug_color;
};

#endif
