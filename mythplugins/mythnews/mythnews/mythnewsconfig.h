#ifndef MYTHNEWSCONFIG_H
#define MYTHNEWSCONFIG_H

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>

// MythNews headers
#include "newsengine.h"

class MythNewsConfigPriv;

class MythNewsConfig : public MythScreenType
{
    Q_OBJECT

public:

    MythNewsConfig(MythScreenStack *parent, const char *name = 0);
    ~MythNewsConfig();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

private:
    void loadData();
    void populateSites();

    MythNewsConfigPriv *m_priv;

    MythUIButtonList      *m_categoriesList;
    MythUIButtonList      *m_siteList;

    MythUIText *m_helpText;
    MythUIText *m_contextText;

    //MythNewsSpinBox    *m_SpinBox;

    int                 m_updateFreq;

private slots:
    void slotCategoryChanged(MythUIButtonListItem* item);
    void toggleItem(MythUIButtonListItem* item);
};

#endif /* MYTHNEWSCONFIG_H */
