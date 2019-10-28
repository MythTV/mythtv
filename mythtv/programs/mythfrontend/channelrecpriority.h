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
    explicit ChannelRecPriority(MythScreenStack *parent);
    ~ChannelRecPriority();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

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
    void ShowMenu(void) override; // MythScreenType
    void upcoming(void);
    void changeRecPriority(int howMuch);
    static void applyChannelRecPriorityChange(const QString&, const QString&);

    void saveRecPriority(void);

    QMap<QString, ChannelInfo> m_channelData;
    QMap<QString, ChannelInfo*> m_sortedChannel;
    QMap<QString, QString> m_origRecPriorityData;

    MythUIButtonList *m_channelList {nullptr};

    MythUIText *m_chanstringText    {nullptr};
    MythUIText *m_channameText      {nullptr};
    MythUIText *m_channumText       {nullptr};
    MythUIText *m_callsignText      {nullptr};
    MythUIText *m_sourcenameText    {nullptr};
    MythUIText *m_sourceidText      {nullptr};
    MythUIText *m_priorityText      {nullptr};

    MythUIImage *m_iconImage        {nullptr};

    SortType m_sortType             {byChannel};

    ChannelInfo *m_currentItem      {nullptr};
};

#endif
