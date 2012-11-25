// MythTV
#include "mythuibuttonlist.h"
#include "proglist_helpers.h"
#include "mythcorecontext.h"
#include "mythuitextedit.h"
#include "scheduleeditor.h"
#include "recordingrule.h"
#include "mythuibutton.h"
#include "channelinfo.h"
#include "channelutil.h"
#include "proglist.h"
#include "mythdate.h"

PhrasePopup::PhrasePopup(MythScreenStack *parentStack,
                         ProgLister *parent,
                         RecSearchType searchType,
                         const QStringList &list,
                         const QString &currentValue) :
    MythScreenType(parentStack, "phrasepopup"),
    m_parent(parent), m_searchType(searchType),  m_list(list),
    m_titleText(NULL), m_phraseList(NULL), m_phraseEdit(NULL),
    m_okButton(NULL), m_deleteButton(NULL), m_recordButton(NULL)
{
    m_currentValue = currentValue;
}

bool PhrasePopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "phrasepopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleText, "title_text", &err);
    UIUtilE::Assign(this, m_phraseList, "phrase_list", &err);
    UIUtilE::Assign(this, m_phraseEdit, "phrase_edit", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_deleteButton, "delete_button", &err);
    UIUtilE::Assign(this, m_recordButton, "record_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'phrasepopup'");
        return false;
    }

    if (m_searchType == kPowerSearch)
    {
        m_titleText->SetText(tr("Select Search"));
        new MythUIButtonListItem(m_phraseList, tr("<New Search>"), NULL, false);
        m_okButton->SetText(tr("Edit"));
    }
    else
    {
        m_titleText->SetText(tr("Phrase"));
        new MythUIButtonListItem(m_phraseList, tr("<New Phrase>"), NULL, false);
    }

    for (int x = 0; x < m_list.size(); x++)
    {
        new MythUIButtonListItem(m_phraseList, m_list.at(x), NULL, false);
    }

    connect(m_phraseList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(phraseClicked(MythUIButtonListItem*)));
    connect(m_phraseList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(phraseSelected(MythUIButtonListItem*)));


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deleteClicked()));
    connect(m_recordButton, SIGNAL(Clicked()), this, SLOT(recordClicked()));

    connect(m_phraseEdit, SIGNAL(valueChanged()), this, SLOT(editChanged()));

    BuildFocusList();

    SetFocusWidget(m_phraseList);

    return true;
}

void PhrasePopup::editChanged(void)
{
    m_okButton->SetEnabled(!m_phraseEdit->GetText().trimmed().isEmpty());
    m_deleteButton->SetEnabled(
        (m_list.indexOf(m_phraseEdit->GetText().trimmed()) != -1));
    m_recordButton->SetEnabled(!m_phraseEdit->GetText().trimmed().isEmpty());
}

void PhrasePopup::phraseClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int pos = m_phraseList->GetCurrentPos();

    if (pos == 0)
        SetFocusWidget(m_phraseEdit);
    else
        okClicked();
}

void PhrasePopup::phraseSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_phraseList->GetCurrentPos() == 0)
        m_phraseEdit->Reset();
    else
        m_phraseEdit->SetText(item->GetText());

    m_okButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_deleteButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_recordButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
}

void PhrasePopup::okClicked(void)
{
    if (m_phraseEdit->GetText().trimmed().isEmpty())
        return;

    // check to see if we need to save the phrase
    m_parent->UpdateKeywordInDB(
        m_phraseEdit->GetText(), m_phraseList->GetValue());

//    emit haveResult(m_phraseList->GetCurrentPos());
    emit haveResult(m_phraseEdit->GetText());

    Close();
}

void PhrasePopup::deleteClicked(void)
{
    int view = m_phraseList->GetCurrentPos() - 1;

    if (view < 0)
        return;

    QString text = m_list[view];
    QString qphrase = text;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM keyword "
                  "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
    query.bindValue(":PHRASE", qphrase);
    query.bindValue(":TYPE", m_searchType);
    if (!query.exec())
        MythDB::DBError("PhrasePopup::deleteClicked", query);

    m_phraseList->RemoveItem(m_phraseList->GetItemCurrent());

    m_parent->m_viewList.removeAll(text);
    m_parent->m_viewTextList.removeAll(text);

    if (view < m_parent->m_curView)
        m_parent->m_curView--;
    else if (view == m_parent->m_curView)
        m_parent->m_curView = -1;

    if (m_parent->m_viewList.count() < 1)
        SetFocusWidget(m_phraseEdit);
    else
        SetFocusWidget(m_phraseList);
}

void PhrasePopup::recordClicked(void)
{
    QString text = m_phraseEdit->GetText();
    bool genreflag = false;

    QString what = text;

    if (text.trimmed().isEmpty())
        return;

    if (m_searchType == kNoSearch)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->PowerStringToSQL(text, what, bindings);

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    RecordingRule *record = new RecordingRule();

    if (genreflag)
    {
        QString fromgenre = QString("LEFT JOIN programgenres ON "
                                    "program.chanid = programgenres.chanid AND "
                                    "program.starttime = programgenres.starttime ");
        record->LoadBySearch(m_searchType, text, what, fromgenre);
    }
    else
    {
        record->LoadBySearch(m_searchType, text, what);
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        okClicked();
    }
    else
        delete schededit;
}

///////////////////////////////////////////////////////////////////////////////

PowerSearchPopup::PowerSearchPopup(MythScreenStack *parentStack,
                                   ProgLister *parent,
                                   RecSearchType searchType,
                                   const QStringList &list,
                                   const QString &currentValue)
    : MythScreenType(parentStack, "phrasepopup"),
      m_parent(parent), m_searchType(searchType), m_list(list),
      m_currentValue(currentValue),
      m_titleText(NULL), m_phraseList(NULL), m_phraseEdit(NULL),
      m_editButton(NULL), m_deleteButton(NULL), m_recordButton(NULL)
{
}

bool PowerSearchPopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "powersearchpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleText, "title_text", &err);
    UIUtilE::Assign(this, m_phraseList, "phrase_list", &err);
    UIUtilE::Assign(this, m_editButton, "edit_button", &err);
    UIUtilE::Assign(this, m_deleteButton, "delete_button", &err);
    UIUtilE::Assign(this, m_recordButton, "record_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'powersearchpopup'");
        return false;
    }

    m_titleText->SetText(tr("Select Search"));
    new MythUIButtonListItem(m_phraseList, tr("<New Search>"), NULL, false);

    for (int x = 0; x < m_list.size(); x++)
    {
        new MythUIButtonListItem(m_phraseList, m_list.at(x), NULL, false);
    }

    connect(m_phraseList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(phraseClicked(MythUIButtonListItem*)));
    connect(m_phraseList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(phraseSelected(MythUIButtonListItem*)));


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_editButton->SetText(tr("Edit"));
    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_editButton, SIGNAL(Clicked()), this, SLOT(editClicked()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deleteClicked()));
    connect(m_recordButton, SIGNAL(Clicked()), this, SLOT(recordClicked()));

    BuildFocusList();

    SetFocusWidget(m_phraseList);

    return true;
}

void PowerSearchPopup::phraseClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int pos = m_phraseList->GetCurrentPos();

    if (pos == 0)
        editClicked();
    else
    {
        emit haveResult(m_phraseList->GetValue());
        Close();
    }
}

void PowerSearchPopup::phraseSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_editButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_deleteButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_recordButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
}

void PowerSearchPopup::editClicked(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString currentItem = ":::::";

    if (m_phraseList->GetCurrentPos() != 0)
        currentItem = m_phraseList->GetValue();

    EditPowerSearchPopup *popup = new EditPowerSearchPopup(
        popupStack, m_parent, currentItem);

    if (!popup->Create())
    {
        delete popup;
        return;
    }

    popupStack->AddScreen(popup);

    Close();
}

void PowerSearchPopup::deleteClicked(void)
{
    int view = m_phraseList->GetCurrentPos() - 1;

    if (view < 0)
        return;

    QString text = m_list[view];
    QString qphrase = text;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM keyword "
                  "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
    query.bindValue(":PHRASE", qphrase);
    query.bindValue(":TYPE", m_searchType);
    if (!query.exec())
        MythDB::DBError("PowerSearchPopup::deleteClicked", query);

    m_phraseList->RemoveItem(m_phraseList->GetItemCurrent());

    m_parent->m_viewList.removeAll(text);
    m_parent->m_viewTextList.removeAll(text);

    if (view < m_parent->m_curView)
        m_parent->m_curView--;
    else if (view == m_parent->m_curView)
        m_parent->m_curView = -1;

    if (m_parent->m_viewList.count() < 1)
        SetFocusWidget(m_phraseEdit);
    else
        SetFocusWidget(m_phraseList);
}

void PowerSearchPopup::recordClicked(void)
{
    QString text = m_phraseList->GetValue();
    bool genreflag = false;

    QString what = text;

    if (text.trimmed().isEmpty())
        return;

    if (m_searchType == kNoSearch)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->PowerStringToSQL(text, what, bindings);

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    RecordingRule *record = new RecordingRule();

    if (genreflag)
    {
        QString fromgenre = QString(
            "LEFT JOIN programgenres ON "
            "program.chanid = programgenres.chanid AND "
            "program.starttime = programgenres.starttime ");
        record->LoadBySearch(m_searchType, text, what, fromgenre);
    }
    else
    {
        record->LoadBySearch(m_searchType, text, what);
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        emit haveResult(m_phraseList->GetValue());
        Close();
    }
    else
        delete schededit;
}

///////////////////////////////////////////////////////////////////////////////

EditPowerSearchPopup::EditPowerSearchPopup(MythScreenStack *parentStack,
                                           ProgLister *parent,
                                           const QString &currentValue)
    : MythScreenType(parentStack, "phrasepopup"),
        m_titleEdit(NULL), m_subtitleEdit(NULL), m_descEdit(NULL),
        m_categoryList(NULL), m_genreList(NULL), m_channelList(NULL),
        m_okButton(NULL)
{
    m_parent = parent;

    //sanity check currentvalue
    m_currentValue = currentValue;
    QStringList field = m_currentValue.split(':');
    if (field.count() != 6)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error. PowerSearch %1 has %2 fields")
                .arg(m_currentValue).arg(field.count()));
        m_currentValue = ":::::";
    }
}

bool EditPowerSearchPopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "editpowersearchpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleEdit, "title_edit", &err);
    UIUtilE::Assign(this, m_subtitleEdit, "subtitle_edit", &err);
    UIUtilE::Assign(this, m_descEdit, "desc_edit", &err);
    UIUtilE::Assign(this, m_categoryList, "category_list", &err);
    UIUtilE::Assign(this, m_genreList, "genre_list", &err);
    UIUtilE::Assign(this, m_channelList, "channel_list", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editpowersearchpopup'");
        return false;
    }

    QStringList field = m_currentValue.split(':');

    m_titleEdit->SetText(field[0]);
    m_subtitleEdit->SetText(field[1]);
    m_descEdit->SetText(field[2]);

    initLists();

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));

    BuildFocusList();

    SetFocusWidget(m_titleEdit);

    return true;
}

void EditPowerSearchPopup::okClicked(void)
{
    QString text;
    text =  m_titleEdit->GetText().replace(':','%').replace('*','%') + ':';
    text += m_subtitleEdit->GetText().replace(':','%').replace('*','%') + ':';
    text += m_descEdit->GetText().replace(':','%').replace('*','%') + ':';

    if (m_categoryList->GetCurrentPos() > 0)
        text += m_categories[m_categoryList->GetCurrentPos()];
    text += ':';
    if (m_genreList->GetCurrentPos() > 0)
        text += m_genres[m_genreList->GetCurrentPos()];
    text += ':';
    if (m_channelList->GetCurrentPos() > 0)
        text += m_channels[m_channelList->GetCurrentPos()];

    if (text == ":::::")
        return;

    m_parent->UpdateKeywordInDB(text, m_currentValue);
    m_parent->FillViewList(text);
    m_parent->SetViewFromList(text);

    Close();
}

void EditPowerSearchPopup::initLists(void)
{
    QStringList field = m_currentValue.split(':');

    // category type
    m_categories.clear();
    new MythUIButtonListItem(
        m_categoryList, tr("(Any Program Type)"), NULL, false);
    m_categories << "";
    new MythUIButtonListItem(m_categoryList, tr("Movies"), NULL, false);
    m_categories << "movie";
    new MythUIButtonListItem(m_categoryList, tr("Series"), NULL, false);
    m_categories << "series";
    new MythUIButtonListItem(m_categoryList, tr("Show"), NULL, false);
    m_categories << "tvshow";
    new MythUIButtonListItem(m_categoryList, tr("Sports"), NULL, false);
    m_categories << "sports";
    m_categoryList->SetItemCurrent(m_categories.indexOf(field[3]));

    // genre
    m_genres.clear();
    new MythUIButtonListItem(m_genreList, tr("(Any Genre)"), NULL, false);
    m_genres << "";

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT genre FROM programgenres GROUP BY genre;");

    if (query.exec())
    {
        while (query.next())
        {
            QString category = query.value(0).toString();
            if (category.isEmpty() || category.trimmed().isEmpty())
                continue;
            category = query.value(0).toString();
            new MythUIButtonListItem(m_genreList, category, NULL, false);
            m_genres << category;
            if (category == field[4])
                m_genreList->SetItemCurrent(m_genreList->GetCount() - 1);
        }
    }

    // channel
    QString channelOrdering = gCoreContext->GetSetting(
        "ChannelOrdering", "channum");

    m_channels.clear();
    new MythUIButtonListItem(m_channelList, tr("(Any Channel)"), NULL, false);
    m_channels << "";

    ChannelInfoList channels = ChannelUtil::GetChannels(0, true, "callsign");
    ChannelUtil::SortChannels(channels, channelOrdering, true);

    MythUIButtonListItem *item;
    for (uint i = 0; i < channels.size(); ++i)
    {
        QString chantext = channels[i].GetFormatted(ChannelInfo::kChannelShort);

        m_parent->m_viewList << QString::number(channels[i].chanid);
        m_parent->m_viewTextList << chantext;

        item = new MythUIButtonListItem(m_channelList, chantext, NULL, false);

        InfoMap chanmap;
        channels[i].ToMap(chanmap);
        item->SetTextFromMap(chanmap);

        m_channels << channels[i].callsign;
        if (channels[i].callsign == field[5])
            m_channelList->SetItemCurrent(m_channelList->GetCount() - 1);
    }
}
