#ifndef MYTHSCREEN_STACK_H_
#define MYTHSCREEN_STACK_H_

#include <QVector>
#include <QObject>

#include "mythuiexp.h"

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
    virtual ~MythScreenStack();

    virtual void AddScreen(MythScreenType *screen, bool allowFade = true);
    virtual void PopScreen(MythScreenType *screen = NULL, bool allowFade = true,
                           bool deleteScreen = true);

    virtual MythScreenType *GetTopScreen(void) const;

    void GetDrawOrder(QVector<MythScreenType *> &screens);
    void GetScreenList(QVector<MythScreenType *> &screens);
    void ScheduleInitIfNeeded(void);
    void AllowReInit(void) { m_DoInit = true; }
    int TotalScreens() const;

    void DisableEffects(void) { m_DoTransitions = false; }
    void EnableEffects(void);

    QString GetLocation(bool fullPath) const;

    MythPainter *GetPainter(void);

  signals:
    void topScreenChanged(MythScreenType *screen);

  private slots:
    void doInit(void);

  protected:
    virtual void RecalculateDrawOrder(void);
    void DoNewFadeTransition();
    void CheckNewFadeTransition();
    void CheckDeletes(bool force = false);

    QVector<MythScreenType *> m_Children;
    QVector<MythScreenType *> m_DrawOrder;

    MythScreenType *m_topScreen;

    bool m_DoTransitions;
    bool m_DoInit;
    bool m_InitTimerStarted;
    bool m_InNewTransition;
    MythScreenType *m_newTop;

    QVector<MythScreenType *> m_ToDelete;
};

#endif

