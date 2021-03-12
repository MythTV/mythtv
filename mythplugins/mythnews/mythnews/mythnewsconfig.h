#ifndef MYTHNEWSCONFIG_H
#define MYTHNEWSCONFIG_H

// Qt headers
#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif

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
    ~MythNewsConfig() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    void loadData(void);
    void populateSites(void);

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    mutable QMutex      m_lock           {QMutex::Recursive};
#else
    mutable QRecursiveMutex  m_lock;
#endif
    MythNewsConfigPriv *m_priv           {nullptr};

    MythUIButtonList   *m_categoriesList {nullptr};
    MythUIButtonList   *m_siteList       {nullptr};

    MythUIText         *m_helpText       {nullptr};
    MythUIText         *m_contextText    {nullptr};
    int                 m_updateFreq     {30};

  private slots:
    void slotCategoryChanged(MythUIButtonListItem *item);
    void toggleItem(MythUIButtonListItem *item);
};

#endif /* MYTHNEWSCONFIG_H */
