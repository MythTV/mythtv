
#ifndef MYTHFLIXCONFIG_H
#define MYTHFLIXCONFIG_H

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>

class MythFlixConfigPriv;
class NewsSiteItem;

class MythFlixConfig : public MythScreenType
{
    Q_OBJECT

  public:

    MythFlixConfig(MythScreenStack *parent, const char *name = 0);
    ~MythFlixConfig();

    bool Create(void);

  private:
    void loadData();
    void populateSites();

    bool findInDB(const QString& name);
    bool insertInDB(NewsSiteItem* site);
    bool removeFromDB(NewsSiteItem* site);

    MythFlixConfigPriv *m_priv;

    MythUIButtonList      *m_genresList;
    MythUIButtonList      *m_siteList;

private slots:
    void slotCategoryChanged(MythUIButtonListItem* item);
    void toggleItem(MythUIButtonListItem* item);
};

#endif /* MYTHFLIXCONFIG_H */
