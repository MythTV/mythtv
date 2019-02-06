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
                const QString &currentValue)
        : MythScreenType(parentStack, "phrasepopup"),
          m_parent(parent), m_searchType(searchType),  m_list(list),
          m_currentValue(currentValue) {}

    bool Create() override; // MythScreenType

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
    ProgLister      *m_parent        {nullptr};
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText    {nullptr};
    MythUIButtonList *m_phraseList   {nullptr};
    MythUITextEdit   *m_phraseEdit   {nullptr};
    MythUIButton     *m_okButton     {nullptr};
    MythUIButton     *m_deleteButton {nullptr};
    MythUIButton     *m_recordButton {nullptr};
};

class PowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    PowerSearchPopup(MythScreenStack *parentStack,
                ProgLister *parent,
                RecSearchType searchType,
                const QStringList &list,
                const QString &currentValue)
        : MythScreenType(parentStack, "phrasepopup"),
          m_parent(parent), m_searchType(searchType), m_list(list),
          m_currentValue(currentValue) {}

    bool Create() override; // MythScreenType

  signals:
    void haveResult(QString item);

  private slots:
    void editClicked(void);
    void deleteClicked(void);
    void recordClicked(void);
    void phraseClicked(MythUIButtonListItem *item);
    void phraseSelected(MythUIButtonListItem *item);

  private:
    ProgLister      *m_parent        {nullptr};
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText    {nullptr};
    MythUIButtonList *m_phraseList   {nullptr};
    MythUITextEdit   *m_phraseEdit   {nullptr};
    MythUIButton     *m_editButton   {nullptr};
    MythUIButton     *m_deleteButton {nullptr};
    MythUIButton     *m_recordButton {nullptr};
};

class EditPowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    EditPowerSearchPopup(MythScreenStack *parentStack, ProgLister *parent,
                         const QString &currentValue);

    bool Create() override; // MythScreenType

  private slots:
    void okClicked(void);

  private:
    void initLists(void);

    ProgLister      *m_parent        {nullptr};
    QStringList      m_categories;
    QStringList      m_genres;
    QStringList      m_channels;

    QString          m_currentValue;

    MythUITextEdit   *m_titleEdit    {nullptr};
    MythUITextEdit   *m_subtitleEdit {nullptr};
    MythUITextEdit   *m_descEdit     {nullptr};
    MythUIButtonList *m_categoryList {nullptr};
    MythUIButtonList *m_genreList    {nullptr};
    MythUIButtonList *m_channelList  {nullptr};

    MythUIButton     *m_okButton     {nullptr};
};

#endif // _PROGLIST_HELPERS_H_
