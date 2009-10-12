#ifndef CHANNELEDITOR_H
#define CHANNELEDITOR_H

#include "mythscreentype.h"

#include "settings.h"

class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIProgressDialog;

class ChannelEditor : public MythScreenType
{
    Q_OBJECT
  public:
    ChannelEditor(MythScreenStack *parent);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

  protected slots:
    void menu();
    void del();
    void edit(MythUIButtonListItem *item);
    void scan(void);
    void transportEditor(void);
    void channelIconImport(void);
    void deleteChannels(void);
    void setSortMode(MythUIButtonListItem *item);
    void setSourceID(MythUIButtonListItem *item);
    void setHideMode(bool hide);
    void fillList();

  private:
    enum sourceFilter {
        FILTER_ALL = -1,
        FILTER_UNASSIGNED = 0
    };

    int m_sourceFilter;
    QString m_sourceFilterName;
    QString m_currentSortMode;
    bool m_currentHideMode;

    MythUIButtonList *m_channelList;
    MythUIButtonList *m_sourceList;
};

class ChannelID;

class ChannelWizard : public QObject, public ConfigurationWizard
{
    Q_OBJECT
  public:
    ChannelWizard(int id, int default_sourceid);
    QString getCardtype();
    bool cardTypesInclude(const QString& cardtype);
    int countCardtypes();

  private:
    ChannelID *cid;
};

#endif
