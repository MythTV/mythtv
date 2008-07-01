#ifndef MYTHSCREEN_STACK_H_
#define MYTHSCREEN_STACK_H_

#include <QVector>
#include <QObject>

class MythScreenType;
class MythMainWindow;

class MythScreenStack : public QObject
{
  public:
    MythScreenStack(MythMainWindow *parent, const QString &name,
                    bool main = false);
    virtual ~MythScreenStack();

    void AddScreen(MythScreenType *screen, bool allowFade = true);
    void PopScreen(bool allowFade = true, bool deleteScreen = true);

    MythScreenType *GetTopScreen(void);

    void GetDrawOrder(QVector<MythScreenType *> &screens);
    int TotalScreens();

    void DisableEffects(void) { m_DoTransitions = false; }

  protected:
    void RecalculateDrawOrder(void);
    void DoNewFadeTransition();
    void CheckNewFadeTransition();
    void CheckDeletes();

    QVector<MythScreenType *> m_Children;
    QVector<MythScreenType *> m_DrawOrder;

    MythScreenType *topScreen;

    bool m_DoTransitions;
    bool m_InNewTransition;
    MythScreenType *newTop;

    QVector<MythScreenType *> m_ToDelete;
};
  
#endif
  
