#ifndef CHANNELRECPRIORITY_H_
#define CHANNELRECPRIORITY_H_

#include "mythscreentype.h"

#include "programinfo.h"

class ChannelInfo;

class MythUIText;
class MythUIImage;
class MythUIStateType;
class MythUIButtonList;
class MythUIButtonListItem;

/**
 * \class ChannelRecPriority
 *
 * \brief Screen for managing channel priorities in recording scheduling
 *        decisions
 */
class ChannelRecPriority : public MythScreenType
{
    Q_OBJECT
  public:
    ChannelRecPriority(MythScreenStack *parent);
    ~ChannelRecPriority();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

    enum SortType
    {
        byChannel,
        byRecPriority,
    };

  protected slots:
    void updateInfo(MythUIButtonListItem *);

  private:
    void FillList(void);
    void SortList(void);
    void updateList(void);
    void ShowMenu(void);
    void upcoming(void);
    void changeRecPriority(int howMuch);
    void applyChannelRecPriorityChange(QString, const QString&);

    void saveRecPriority(void);

    QMap<QString, ChannelInfo> m_channelData;
    QMap<QString, ChannelInfo*> m_sortedChannel;
    QMap<QString, QString> m_origRecPriorityData;

    MythUIButtonList *m_channelList;

    MythUIText *m_chanstringText;
    MythUIText *m_channameText;
    MythUIText *m_channumText;
    MythUIText *m_callsignText;
    MythUIText *m_sourcenameText;
    MythUIText *m_sourceidText;
    MythUIText *m_priorityText;

    MythUIImage *m_iconImage;

    SortType m_sortType;

    ChannelInfo *m_currentItem;
};

#endif
