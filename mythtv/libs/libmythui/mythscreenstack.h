#ifndef MYTHSCREEN_STACK_H_
#define MYTHSCREEN_STACK_H_

#include <QVector>
#include <QObject>

#include "mythexp.h"

class QString;

class MythScreenType;
class MythMainWindow;

class MPUBLIC MythScreenStack : public QObject
{
  Q_OBJECT

  public:
    MythScreenStack(MythMainWindow *parent, const QString &name,
                    bool main = false);
    virtual ~MythScreenStack();

    void AddScreen(MythScreenType *screen, bool allowFade = true);
    void PopScreen(bool allowFade = true, bool deleteScreen = true);
    void PopScreen(MythScreenType *screen, bool allowFade = true,
                   bool deleteScreen = true);

    MythScreenType *GetTopScreen(void) const;

    void GetDrawOrder(QVector<MythScreenType *> &screens);
    void ScheduleInitIfNeeded(void);
    void AllowReInit(void) { m_DoInit = true; }
    int TotalScreens() const;

    void DisableEffects(void) { m_DoTransitions = false; }
    void EnableEffects(void);

    QString GetLocation(bool fullPath) const;

  private slots:
    void doInit(void);

  protected:
    void RecalculateDrawOrder(void);
    void DoNewFadeTransition();
    void CheckNewFadeTransition();
    void CheckDeletes();

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

