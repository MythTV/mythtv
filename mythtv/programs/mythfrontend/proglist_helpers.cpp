// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/recordingrule.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

// MythFrontend
#include "proglist.h"
#include "proglist_helpers.h"
#include "scheduleeditor.h"

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
        new MythUIButtonListItem(m_phraseList, tr("<New Search>"), nullptr, false);
        m_okButton->SetText(tr("Edit"));
    }
    else
    {
        m_titleText->SetText(tr("Phrase"));
        new MythUIButtonListItem(m_phraseList, tr("<New Phrase>"), nullptr, false);
    }

    for (const QString& item : std::as_const(m_list))
    {
        new MythUIButtonListItem(m_phraseList, item, nullptr, false);
    }

    connect(m_phraseList, &MythUIButtonList::itemClicked,
            this, &PhrasePopup::phraseClicked);
    connect(m_phraseList, &MythUIButtonList::itemSelected,
            this, &PhrasePopup::phraseSelected);


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_okButton, &MythUIButton::Clicked, this, &PhrasePopup::okClicked);
    connect(m_deleteButton, &MythUIButton::Clicked, this, &PhrasePopup::deleteClicked);
    connect(m_recordButton, &MythUIButton::Clicked, this, &PhrasePopup::recordClicked);

    connect(m_phraseEdit, &MythUITextEdit::valueChanged, this, &PhrasePopup::editChanged);

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
    const QString& qphrase = text;

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
    QString what = text;
    QString fromgenre;

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
        if (ProgLister::PowerStringToSQL(text, what, bindings))
        {
            fromgenre = QString("LEFT JOIN programgenres ON "
                                "program.chanid = programgenres.chanid AND "
                                "program.starttime = programgenres.starttime ");
        }

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    auto *record = new RecordingRule();

    record->LoadBySearch(m_searchType, text, what, fromgenre);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        okClicked();
    }
    else
    {
        delete schededit;
    }
}

///////////////////////////////////////////////////////////////////////////////

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
    new MythUIButtonListItem(m_phraseList, tr("<New Search>"), nullptr, false);

    for (const QString &item : std::as_const(m_list))
    {
        new MythUIButtonListItem(m_phraseList, item, nullptr, false);
    }

    connect(m_phraseList, &MythUIButtonList::itemClicked,
            this, &PowerSearchPopup::phraseClicked);
    connect(m_phraseList, &MythUIButtonList::itemSelected,
            this, &PowerSearchPopup::phraseSelected);


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_editButton->SetText(tr("Edit"));
    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_editButton, &MythUIButton::Clicked, this, &PowerSearchPopup::editClicked);
    connect(m_deleteButton, &MythUIButton::Clicked, this, &PowerSearchPopup::deleteClicked);
    connect(m_recordButton, &MythUIButton::Clicked, this, &PowerSearchPopup::recordClicked);

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

    auto *popup = new EditPowerSearchPopup(popupStack, m_parent, currentItem);

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
    const QString& qphrase = text;

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
    QString what = text;
    QString fromgenre;

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
        if (ProgLister::PowerStringToSQL(text, what, bindings))
        {
            fromgenre = QString(
                "LEFT JOIN programgenres ON "
                "program.chanid = programgenres.chanid AND "
                "program.starttime = programgenres.starttime ");
        }

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    auto *record = new RecordingRule();

    record->LoadBySearch(m_searchType, text, what, fromgenre);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        emit haveResult(m_phraseList->GetValue());
        Close();
    }
    else
    {
        delete schededit;
    }
}

///////////////////////////////////////////////////////////////////////////////

EditPowerSearchPopup::EditPowerSearchPopup(MythScreenStack *parentStack,
                                           ProgLister *parent,
                                           QString &currentValue)
    : MythScreenType(parentStack, "phrasepopup"),
      m_parent(parent),
      m_currentValue(std::move(currentValue))
{
    //sanity check currentvalue
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

    connect(m_okButton, &MythUIButton::Clicked, this, &EditPowerSearchPopup::okClicked);

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
        m_categoryList, tr("(Any Program Type)"), nullptr, false);
    m_categories << "";
    new MythUIButtonListItem(m_categoryList, tr("Movies"), nullptr, false);
    m_categories << "movie";
    new MythUIButtonListItem(m_categoryList, tr("Series"), nullptr, false);
    m_categories << "series";
    new MythUIButtonListItem(m_categoryList, tr("Show"), nullptr, false);
    m_categories << "libmythtv/tvshow";
    new MythUIButtonListItem(m_categoryList, tr("Sports"), nullptr, false);
    m_categories << "sports";
    m_categoryList->SetItemCurrent(m_categories.indexOf(field[3]));

    // genre
    m_genres.clear();
    new MythUIButtonListItem(m_genreList, tr("(Any Genre)"), nullptr, false);
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
            new MythUIButtonListItem(m_genreList, category, nullptr, false);
            m_genres << category;
            if (category == field[4])
                m_genreList->SetItemCurrent(m_genreList->GetCount() - 1);
        }
    }

    // channel
    QString channelOrdering = gCoreContext->GetSetting(
        "ChannelOrdering", "channum");

    m_channels.clear();
    new MythUIButtonListItem(m_channelList, tr("(Any Channel)"), nullptr, false);
    m_channels << "";

    ChannelInfoList channels = ChannelUtil::GetChannels(0, true, "callsign");
    ChannelUtil::SortChannels(channels, channelOrdering, true);

    for (auto & channel : channels)
    {
        QString chantext = channel.GetFormatted(ChannelInfo::kChannelShort);

        m_parent->m_viewList << QString::number(channel.m_chanId);
        m_parent->m_viewTextList << chantext;

        auto *item = new MythUIButtonListItem(m_channelList, chantext,
                                              nullptr, false);

        InfoMap chanmap;
        channel.ToMap(chanmap);
        item->SetTextFromMap(chanmap);

        m_channels << channel.m_callSign;
        if (channel.m_callSign == field[5])
            m_channelList->SetItemCurrent(m_channelList->GetCount() - 1);
    }
}
