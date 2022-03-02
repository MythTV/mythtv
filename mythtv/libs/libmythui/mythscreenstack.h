#ifndef MYTHSCREEN_STACK_H_
#define MYTHSCREEN_STACK_H_

#include <QVector>
#include <QObject>

#include "libmythbase/mythchrono.h"
#include "libmythui/mythuiexp.h"

class QString;

class MythScreenType;
class MythMainWindow;
class MythPainter;

class MUI_PUBLIC MythScreenStack : public QObject
{
  Q_OBJECT

  public:
    MythScreenStack(MythMainWindow *parent, const QString &name,
                    bool main = false);
    ~MythScreenStack() override;

    virtual void AddScreen(MythScreenType *screen, bool allowFade = true);
    virtual void PopScreen(MythScreenType *screen = nullptr, bool allowFade = true,
                           bool deleteScreen = true);

    virtual MythScreenType *GetTopScreen(void) const;

    void GetDrawOrder(QVector<MythScreenType *> &screens);
    void GetScreenList(QVector<MythScreenType *> &screens);
    void ScheduleInitIfNeeded(void);
    void AllowReInit(void) { m_doInit = true; }
    int TotalScreens() const;

    void DisableEffects(void) { m_doTransitions = false; }
    void EnableEffects(void);

    QString GetLocation(bool fullPath) const;

    static MythPainter *GetPainter(void);

  signals:
    void topScreenChanged(MythScreenType *screen);

  private slots:
    void doInit(void);

  protected:
    virtual void RecalculateDrawOrder(void);
    void DoNewFadeTransition();
    void CheckNewFadeTransition();
    void CheckDeletes(bool force = false);

    QVector<MythScreenType *> m_children;
    QVector<MythScreenType *> m_drawOrder;

    MythScreenType *m_topScreen {nullptr};

    bool m_doTransitions        {false};
    bool m_doInit               {false};
    bool m_initTimerStarted     {false};
    bool m_inNewTransition      {false};
    MythScreenType *m_newTop    {nullptr};

    QVector<MythScreenType *> m_toDelete;
};

#endif

