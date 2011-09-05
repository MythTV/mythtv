#ifndef MYTHNEWSCONFIG_H
#define MYTHNEWSCONFIG_H

// Qt headers
#include <QMutex>

// MythTV headers
#include <mythscreentype.h>

class MythNewsConfigPriv;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;

class MythNewsConfig : public MythScreenType
{
    Q_OBJECT

  public:
    MythNewsConfig(MythScreenStack *parent,
                   const QString &name);
    ~MythNewsConfig();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    void loadData(void);
    void populateSites(void);

    mutable QMutex      m_lock;
    MythNewsConfigPriv *m_priv;

    MythUIButtonList   *m_categoriesList;
    MythUIButtonList   *m_siteList;

    MythUIText         *m_helpText;
    MythUIText         *m_contextText;
    int                 m_updateFreq;

  private slots:
    void slotCategoryChanged(MythUIButtonListItem *item);
    void toggleItem(MythUIButtonListItem *item);
};

#endif /* MYTHNEWSCONFIG_H */
