#ifndef MYTHSCREEN_STACK_H_
#define MYTHSCREEN_STACK_H_

#include <qvaluevector.h>
#include <qobject.h>

class MythScreenType;
class MythMainWindow;

class MythScreenStack : public QObject
{
  public:
    MythScreenStack(MythMainWindow *parent, const char *name,
                    bool main = false);
    virtual ~MythScreenStack();

    void AddScreen(MythScreenType *screen, bool allowFade = true);
    void PopScreen(void);

    MythScreenType *GetTopScreen(void);

    void GetDrawOrder(QValueVector<MythScreenType *> &screens);
    int TotalScreens();

    void DisableEffects(void) { m_DoTransitions = false; }

  protected:
    void RecalculateDrawOrder(void);
    void DoNewFadeTransition();
    void CheckNewFadeTransition();
    void CheckDeletes();

    QValueVector<MythScreenType *> m_Children;
    QValueVector<MythScreenType *> m_DrawOrder;

    MythScreenType *topScreen;

    bool m_DoTransitions;
    bool m_InNewTransition;
    MythScreenType *newTop;

    QValueVector<MythScreenType *> m_ToDelete;
};
  
#endif
  
