#ifndef PROGLIST_HELPERS_H
#define PROGLIST_HELPERS_H

#include <utility>

// Qt headers
#include <QDateTime>

// MythTV headers
#include "libmythbase/recordingtypes.h"
#include "libmythui/mythscreentype.h"

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
                QStringList list,
                QString currentValue)
        : MythScreenType(parentStack, "phrasepopup"),
          m_parent(parent), m_searchType(searchType),
          m_list(std::move(list)),
          m_currentValue(std::move(currentValue)) {}

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
                QStringList list,
                QString currentValue)
        : MythScreenType(parentStack, "phrasepopup"),
          m_parent(parent), m_searchType(searchType),
          m_list(std::move(list)),
          m_currentValue(std::move(currentValue)) {}

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
                         QString &currentValue);

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

#endif // PROGLIST_HELPERS_H
