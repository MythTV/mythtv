#ifndef _PROGLIST_HELPERS_H_
#define _PROGLIST_HELPERS_H_

// Qt headers
#include <QDateTime>

// MythTV headers
#include "mythscreentype.h"
#include "recordingtypes.h"

class MythUIText;
class MythUIButtonList;
class ProgLister;

class PhrasePopup : public MythScreenType
{
    Q_OBJECT

  public:
    PhrasePopup(MythScreenStack *parentStack,
                ProgLister *parent,
                RecSearchType searchType,
                const QStringList &list,
                const QString &currentValue);

    bool Create();

  signals:
    void haveResult(QString item);

  private slots:
    void okClicked(void);
    void deleteClicked(void);
    void recordClicked(void);
    void phraseClicked(MythUIButtonListItem *item);
    void phraseSelected(MythUIButtonListItem *item);
    void editChanged(void);

  private:
    ProgLister      *m_parent;
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText;
    MythUIButtonList *m_phraseList;
    MythUITextEdit   *m_phraseEdit;
    MythUIButton     *m_okButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_recordButton;
};

class TimePopup : public MythScreenType
{
    Q_OBJECT

  public:
    TimePopup(MythScreenStack *parentStack, ProgLister *parent);

    bool Create();

  signals:
    void haveResult(QDateTime time);

  private slots:
    void okClicked(void);

  private:
    ProgLister      *m_parent;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIButtonList *m_dateList;
    MythUIButtonList *m_timeList;
    MythUIButton     *m_okButton;
};

class PowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    PowerSearchPopup(MythScreenStack *parentStack,
                ProgLister *parent,
                RecSearchType searchType,
                const QStringList &list,
                const QString &currentValue);

    bool Create();

  signals:
    void haveResult(QString item);

  private slots:
    void editClicked(void);
    void deleteClicked(void);
    void recordClicked(void);
    void phraseClicked(MythUIButtonListItem *item);
    void phraseSelected(MythUIButtonListItem *item);

  private:
    ProgLister      *m_parent;
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText;
    MythUIButtonList *m_phraseList;
    MythUITextEdit   *m_phraseEdit;
    MythUIButton     *m_editButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_recordButton;
};

class EditPowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    EditPowerSearchPopup(MythScreenStack *parentStack, ProgLister *parent,
                         const QString &currentValue);

    bool Create();

  private slots:
    void okClicked(void);

  private:
    void initLists(void);

    ProgLister      *m_parent;
    QStringList      m_categories;
    QStringList      m_genres;
    QStringList      m_channels;

    QString          m_currentValue;

    MythUITextEdit   *m_titleEdit;
    MythUITextEdit   *m_subtitleEdit;
    MythUITextEdit   *m_descEdit;
    MythUIButtonList *m_categoryList;
    MythUIButtonList *m_genreList;
    MythUIButtonList *m_channelList;

    MythUIButton     *m_okButton;
};

#endif // _PROGLIST_HELPERS_H_
