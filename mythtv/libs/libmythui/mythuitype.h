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
    MythUIType *GetChildAt(const QPoint &p);
    QValueVector<MythUIType *> *GetAllChildren(void);

    void DeleteAllChildren(void);

    // Check set dirty status
    bool NeedsRedraw(void);
    void SetRedraw();

    void SetChildNeedsRedraw(MythUIType *child);

    // Check set if this can take focus
    bool CanTakeFocus(void);
    void SetCanTakeFocus(bool set = true);

    // Called each draw pulse.  Will redraw automatically if dirty afterwards
    virtual void Pulse(void);

    void Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod = 255,
              QRect clipRegion = QRect());

    virtual void SetPosition(int x, int y);
    virtual void SetPosition(const QPoint &pos);
    virtual void SetArea(const QRect &rect);
    virtual QRect GetArea(void) const;

    virtual QRegion GetDirtyArea(void) const;

    QString cutDown(const QString &data, QFont *font,
                    bool multiline = false, int overload_width = -1,
                    int overload_height = -1);

    bool IsVisible(void);
    void SetVisible(bool visible);

    void MoveTo(QPoint destXY, QPoint speedXY);
    // make mode enum
    void AdjustAlpha(int mode, int alphachange, int minalpha = 0,
                     int maxalpha = 255);
    void SetAlpha(int newalpha);
    int GetAlpha(void);

    virtual bool keyPressEvent(QKeyEvent *);

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
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRegion);

    void AddFocusableChildrenToList(QPtrList<MythUIType> &focusList);
    void HandleAlphaPulse();
    void HandleMovementPulse();

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

    MythUIType *m_Parent;
};

#endif
