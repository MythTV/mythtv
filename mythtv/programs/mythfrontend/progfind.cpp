// Qt
#include <QDateTime>
#include <QCoreApplication>
#include <QStringList>
#include <QRegExp>
#include <QKeyEvent>
#include <QEvent>

// MythTV
#include "mythdb.h"
#include "mythdbcon.h"
#include "mythdirs.h"
#include "mythcorecontext.h"
#include "recordinginfo.h"
#include "tv.h"

// MythUI
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuihelper.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"

// mythfrontend
#include "guidegrid.h"
#include "customedit.h"
#include "progfind.h"

#define LOC      QString("ProgFinder: ")
#define LOC_ERR  QString("ProgFinder, Error: ")
#define LOC_WARN QString("ProgFinder, Warning: ")

void RunProgramFinder(TV *player, bool embedVideo, bool allowEPG)
{
    // Language specific progfinder, if needed
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgFinder *programFind = NULL;
    if (gCoreContext->GetLanguage() == "ja")
        programFind = new JaProgFinder(mainStack, allowEPG, player, embedVideo);
    else if (gCoreContext->GetLanguage() == "he")
        programFind = new HeProgFinder(mainStack, allowEPG, player, embedVideo);
    else if (gCoreContext->GetLanguage() == "ru")
        programFind = new RuProgFinder(mainStack, allowEPG, player, embedVideo);
    else // default
        programFind = new ProgFinder(mainStack, allowEPG, player, embedVideo);

    if (programFind->Create())
        mainStack->AddScreen(programFind, (player == NULL));
    else
        delete programFind;
}

ProgFinder::ProgFinder(MythScreenStack *parentStack, bool allowEPG,
                       TV *player, bool embedVideo)
          : ScheduleCommon(parentStack, "ProgFinder"),
    m_currentLetter(""),
    m_player(player),            m_embedVideo(embedVideo),
    m_allowEPG(allowEPG),        m_allowKeypress(true),
    m_alphabetList(NULL),        m_showList(NULL),
    m_timesList(NULL),           m_searchText(NULL),
    m_help1Text(NULL),           m_help2Text(NULL)
{
}

bool ProgFinder::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "programfind", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_alphabetList, "alphabet", &err);
    UIUtilE::Assign(this, m_showList, "shows", &err);
    UIUtilE::Assign(this, m_timesList, "times", &err);

    UIUtilW::Assign(this, m_help1Text, "help1text");
    UIUtilW::Assign(this, m_help2Text, "help2text");
    UIUtilW::Assign(this, m_searchText, "search");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'programfind'");
        return false;
    }

    m_alphabetList->SetLCDTitles(tr("Starts With"), "");
    m_showList->SetLCDTitles(tr("Programs"), "");
    m_timesList->SetLCDTitles(tr("Times"), "buttontext");

    BuildFocusList();
    LoadInBackground();

    if (m_player)
        m_player->StartEmbedding(QRect());

    return true;
}

void ProgFinder::Init(void)
{
    m_allowKeypress = true;

    initAlphabetList();

    gCoreContext->addListener(this);

    connect(m_timesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo()));
    connect(m_timesList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(select()));
    connect(m_timesList, SIGNAL(LosingFocus()), SLOT(timesListLosingFocus()));
    connect(m_timesList, SIGNAL(TakingFocus()), SLOT(timesListTakeFocus()));

    connect(m_alphabetList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(alphabetListItemSelected(MythUIButtonListItem*)));
    connect(m_alphabetList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(select()));

    connect(m_showList, SIGNAL(TakingFocus()), SLOT(showListTakeFocus()));
    connect(m_showList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(select()));

    m_alphabetList->MoveToNamedPosition("A");
}

ProgFinder::~ProgFinder()
{
    gCoreContext->removeListener(this);

    // if we have a player and we are returning to it we need
    // to tell it to stop embedding and return to fullscreen
    if (m_player && m_allowEPG)
    {
        QString message = QString("PROGFINDER_EXITING");
        qApp->postEvent(m_player, new MythEvent(message));
    }
}

void ProgFinder::alphabetListItemSelected(MythUIButtonListItem *item)
{
    if (!item || (m_currentLetter == item->GetText()))
        return;

    m_currentLetter = item->GetText();

    updateShowList();
    updateInfo();
}

void ProgFinder::timesListLosingFocus(void)
{
    m_timesList->Reset();
}

void ProgFinder::showListTakeFocus(void)
{
    updateInfo();
}

void ProgFinder::timesListTakeFocus(void)
{
    selectShowData("", 0);
    updateInfo();
}

bool ProgFinder::keyPressEvent(QKeyEvent *event)
{
    if (!m_allowKeypress)
        return true;

    m_allowKeypress = false;

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
    {
        m_allowKeypress = true;
        return true;
    }

    bool handled = false;

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "EDIT")
            edit();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS" || action == "INFO")
            details();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else if (action == "GUIDE" || action == "4")
            showGuide();
        else if (action == "ESCAPE")
        {
            // don't fade the screen if we are returning to the player
            if (m_player && m_allowEPG)
                GetScreenStack()->PopScreen(this, false);
            else
                GetScreenStack()->PopScreen(this, true);
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    m_allowKeypress = true;

    return handled;
}

void ProgFinder::ShowMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");

        if (!m_searchStr.isEmpty())
            menuPopup->AddButton(tr("Clear Search"));
        menuPopup->AddButton(tr("Edit Search"));
        if (GetFocusWidget() == m_timesList && m_timesList->GetCount() > 0)
        {
            menuPopup->AddButton(tr("Toggle Record"));
            menuPopup->AddButton(tr("Program Details"));
            menuPopup->AddButton(tr("Upcoming"));
            menuPopup->AddButton(tr("Custom Edit"));
            menuPopup->AddButton(tr("Program Guide"));
        }

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgFinder::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message == "SCHEDULE_CHANGE")
        {
            if (GetFocusWidget() == m_timesList)
            {
                ProgramInfo *curPick = m_showData[m_timesList->GetCurrentPos()];
                if (curPick)
                    selectShowData(curPick->GetTitle(),
                                   m_timesList->GetCurrentPos());
            }
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "menu")
        {
            if (resulttext == tr("Clear Search"))
            {
                m_searchStr.clear();
                if (m_searchText)
                    m_searchText->SetText(m_searchStr);
                updateShowList();
                SetFocusWidget(m_showList);
            }
            else if (resulttext == tr("Edit Search"))
            {
                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
                SearchInputDialog *textInput =
                        new SearchInputDialog(popupStack, m_searchStr);

                if (textInput->Create())
                {
                    textInput->SetReturnEvent(this, "searchtext");
                    popupStack->AddScreen(textInput);
                }
            }
            else if (resulttext == tr("Toggle Record"))
            {
                quickRecord();
            }
            else if (resulttext == tr("Program Details"))
            {
                details();
            }
            else if (resulttext == tr("Upcoming"))
            {
                upcoming();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                customEdit();
            }
            else if (resulttext == tr("Program Guide"))
            {
                showGuide();
            }
        }
        else if (resultid == "searchtext")
        {
            m_searchStr = resulttext;
            if (m_searchText)
                m_searchText->SetText(m_searchStr);
            updateShowList();
            SetFocusWidget(m_showList);
        }
        else
            ScheduleCommon::customEvent(event);
    }
}

void ProgFinder::updateInfo(void)
{
    if (m_help1Text)
        m_help1Text->Reset();
    if (m_help2Text)
        m_help2Text->Reset();

    if (GetFocusWidget() == m_alphabetList)
    {
        QString title;
        QString description;

        if (m_showList->GetCount() == 0)
        {
            if (m_help1Text)
                m_help1Text->SetText(tr("No Programs"));
            if (m_help2Text)
                m_help2Text->SetText(tr("There are no available programs under this search. "
                                        "Please select another search."));
        }
        else
        {
            if (m_help1Text)
                m_help1Text->SetText(tr("Select a letter..."));
            if (m_help2Text)
                m_help2Text->SetText(tr("Pick the first letter of the program name, "
                                        "then press SELECT or the right arrow."));
        }

        ResetMap(m_infoMap);
    }
    else if (GetFocusWidget() == m_showList)
    {
        if (m_help1Text)
            m_help1Text->SetText(tr("Select a program..."));
        if (m_help2Text)
            m_help2Text->SetText(tr("Select the title of the program you wish to find. "
                                    "When finished return with the left arrow key. "
                                    "Press SELECT to schedule a recording."));

        ResetMap(m_infoMap);
    }
    else if (GetFocusWidget() == m_timesList)
    {
        if (m_showData.size() == 0)
        {
            ResetMap(m_infoMap);
            if (m_help1Text)
                m_help1Text->SetText(tr("No Programs"));
            if (m_help2Text)
                m_help2Text->SetText(tr("There are no available programs under "
                                        "this search. Please select another "
                                        "search."));
        }
        else
        {
            InfoMap infoMap;
            m_showData[m_timesList->GetCurrentPos()]->ToMap(infoMap);
            SetTextFromMap(infoMap);
            m_infoMap = infoMap;
        }
    }
}

void ProgFinder::showGuide()
{
    if (m_allowEPG)
    {
        QString startchannel = gCoreContext->GetSetting("DefaultTVChannel");
        if (startchannel.isEmpty())
            startchannel = '3';
        uint startchanid = 0;
        QDateTime starttime;

        if (GetFocusWidget() == m_timesList)
        {
            ProgramInfo *pginfo = m_showData[m_timesList->GetCurrentPos()];
            startchannel = pginfo->GetChanNum();
            startchanid = pginfo->GetChanID();
            starttime = pginfo->GetScheduledStartTime();
        }
        GuideGrid::RunProgramGuide(startchanid, startchannel, starttime,
                                   m_player, m_embedVideo, false, -2);
    }
}

void ProgFinder::getInfo(bool toggle)
{
    if (GetFocusWidget() == m_timesList)
    {
        ProgramInfo *curPick = m_showData[m_timesList->GetCurrentPos()];

        if (curPick)
        {
            if (toggle)
                QuickRecord(curPick);
            else
                EditRecording(curPick);
        }
        else
            return;

        // TODO: When schedule editor is non-blocking, move
        selectShowData(curPick->GetTitle(), m_timesList->GetCurrentPos());
    }
}

void ProgFinder::edit()
{
    if (GetFocusWidget() == m_timesList)
    {
        ProgramInfo *curPick = m_showData[m_timesList->GetCurrentPos()];

        if (curPick)
        {
            EditScheduled(curPick);
            // TODO: When schedule editor is non-blocking, move
            selectShowData(curPick->GetTitle(), m_timesList->GetCurrentPos());
        }
    }
}

void ProgFinder::select()
{
    if (GetFocusWidget() == m_timesList)
        getInfo();
    else if (GetFocusWidget() == m_alphabetList && m_showList->GetCount())
        SetFocusWidget(m_showList);
    else if (GetFocusWidget() == m_showList)
        SetFocusWidget(m_timesList);
}

void ProgFinder::customEdit()
{
    if (GetFocusWidget() == m_timesList)
    {
        ProgramInfo *pginfo = m_showData[m_timesList->GetCurrentPos()];
        EditCustom(pginfo);
    }
}

void ProgFinder::upcoming()
{
    if (GetFocusWidget() == m_timesList)
    {
        ProgramInfo *pginfo = m_showData[m_timesList->GetCurrentPos()];
        ShowUpcoming(pginfo);
    }
}

void ProgFinder::details()
{
    if (GetFocusWidget() != m_timesList)
        return;

    ProgramInfo *curPick = m_showData[m_timesList->GetCurrentPos()];
    ShowDetails(curPick);
}

void ProgFinder::quickRecord()
{
    getInfo(true);
}

void ProgFinder::updateTimesList()
{
    InfoMap infoMap;

    m_timesList->Reset();

    if (m_showData.size() > 0)
    {
        QString itemText;
        QDateTime starttime;
        for (uint i = 0; i < m_showData.size(); ++i)
        {
            starttime = m_showData[i]->GetScheduledStartTime();
            itemText = MythDate::toString(starttime,
                                            MythDate::kDateTimeFull | MythDate::kSimplify);

            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_timesList, "");

            m_showData[i]->ToMap(infoMap);
            item->SetTextFromMap(infoMap);

            QString state = toUIState(m_showData[i]->GetRecordingStatus());
            item->SetText(itemText, "buttontext", state);
            item->DisplayState(state, "status");
        }
    }
}

void ProgFinder::getShowNames()
{
    m_showNames.clear();

    QString thequery;
    MSqlBindings bindings;

    MSqlQuery query(MSqlQuery::InitCon());
    whereClauseGetSearchData(thequery, bindings);

    query.prepare(thequery);
    query.bindValues(bindings);
    if (!query.exec())
    {
        MythDB::DBError("getShowNames", query);
        return;
    }

    QString data;
    while (query.next())
    {
        data = query.value(0).toString();

        if (formatSelectedData(data))
            m_showNames[data.toLower()] = data;
    }
}

void ProgFinder::updateShowList()
{
    m_showList->Reset();

    if (m_showNames.isEmpty())
        getShowNames();

    ShowName::Iterator it;
    for (it = m_showNames.begin(); it != m_showNames.end(); ++it)
    {
        QString tmpProgData = *it;
        restoreSelectedData(tmpProgData);
        new MythUIButtonListItem(m_showList, tmpProgData);
    }

    m_showNames.clear();
}

void ProgFinder::selectShowData(QString progTitle, int newCurShow)
{
    progTitle = m_showList->GetValue();

    QDateTime progStart = MythDate::current();

    MSqlBindings bindings;
    QString querystr = "WHERE program.title = :TITLE "
                       "  AND program.endtime > :ENDTIME "
                       "  AND channel.visible = 1 ";
    bindings[":TITLE"] = progTitle;
    bindings[":ENDTIME"] = progStart.addSecs(50 - progStart.time().second());

    LoadFromScheduler(m_schedList);
    LoadFromProgram(m_showData, querystr, bindings, m_schedList);

    updateTimesList();

    m_timesList->SetItemCurrent(newCurShow);
}

void ProgFinder::initAlphabetList(void)
{
    for (int charNum = 48; charNum < 91; ++charNum)
    {
        if (charNum == 58)
            charNum = 65;

        new MythUIButtonListItem(m_alphabetList, QString((char)charNum));
    }

    new MythUIButtonListItem(m_alphabetList, QString('@'));
}

void ProgFinder::whereClauseGetSearchData(QString &where, MSqlBindings &bindings)
{
    QDateTime progStart = MythDate::current();
    QString searchChar = m_alphabetList->GetValue();

    if (searchChar.isEmpty())
        searchChar = "A";

    if (searchChar.contains('@'))
    {
        where = "SELECT DISTINCT title FROM program "
                "LEFT JOIN channel ON program.chanid = channel.chanid "
                "WHERE channel.visible = 1 AND "
                "( title NOT REGEXP '^[A-Z0-9]' AND "
                "  title NOT REGEXP '^The [A-Z0-9]' AND "
                "  title NOT REGEXP '^A [A-Z0-9]' AND "
                "  title NOT REGEXP '^An [A-Z0-9]' AND "
                "  starttime > :STARTTIME ) ";
        if (!m_searchStr.isEmpty())
        {
            where += "AND title LIKE :SEARCH ";
            bindings[":SEARCH"] = '%' + m_searchStr + '%';
        }

        where += "ORDER BY title;";

        bindings[":STARTTIME"] =
            progStart.addSecs(50 - progStart.time().second());
    }
    else
    {
        QString one = searchChar + '%';
        QString two = QString("The ") + one;
        QString three = QString("A ") + one;
        QString four = QString("An ") + one;

        where = "SELECT DISTINCT title FROM program "
                "LEFT JOIN channel ON program.chanid = channel.chanid "
                "WHERE channel.visible = 1 "
                "AND ( title LIKE :ONE OR title LIKE :TWO "
                "      OR title LIKE :THREE "
                "      OR title LIKE :FOUR ) "
                "AND starttime > :STARTTIME ";
        if (!m_searchStr.isEmpty())
            where += "AND title LIKE :SEARCH ";

        where += "ORDER BY title;";

        bindings[":ONE"] = one;
        bindings[":TWO"] = two;
        bindings[":THREE"] = three;
        bindings[":FOUR"] = four;
        bindings[":STARTTIME"] =
            progStart.addSecs(50 - progStart.time().second());

        if (!m_searchStr.isEmpty())
            bindings[":SEARCH"] = '%' + m_searchStr + '%';
    }
}

bool ProgFinder::formatSelectedData(QString& data)
{
    bool retval = true;
    QString searchChar = m_alphabetList->GetValue();

    if (searchChar == "T")
    {
        if (!data.startsWith("The ") && !data.startsWith("A "))
        {
             // nothing, use as is
        }
        else if (data.startsWith("The T"))
            data = data.mid(4) + ", The";
        else if (data.startsWith("A T"))
            data = data.mid(2) + ", A";
        else
        {
            // don't add
            retval = false;
        }
    }
    else if (searchChar == "A")
    {
        if (!data.startsWith("The ") && !data.startsWith("A "))
        {
             // nothing, use as is
        }
        else if (data.startsWith("The A"))
            data = data.mid(4) + ", The";
        else if (data.startsWith("A A"))
             data = data.mid(2) + ", A";
        else if (data.startsWith("An A"))
             data = data.mid(3) + ", An";
        else
        {
            // don't add
            retval = false;
        }
    }
    else
    {
        if (data.startsWith("The "))
            data = data.mid(4) + ", The";
        else if (data.startsWith("A "))
            data = data.mid(2) + ", A";
        else if (data.startsWith("An "))
            data = data.mid(3) + ", An";
    }

    return retval;
}

bool ProgFinder::formatSelectedData(QString& data, int charNum)
{
    bool retval = true;

    if (charNum == 29 || charNum == 10)
    {
        if (data.startsWith("The T") && charNum == 29)
            data = data.mid(4) + ", The";
        else if (data.startsWith("The A") && charNum == 10)
            data = data.mid(4) + ", The";
        else if (data.startsWith("A T") && charNum == 29)
            data = data.mid(2) + ", A";
        else if (data.startsWith("A A") && charNum == 10)
            data = data.mid(2) + ", A";
        else if (data.startsWith("An A") && charNum == 10)
             data = data.mid(3) + ", An";
        else if (!data.startsWith("The ") && !data.startsWith("A "))
        {
            // use as is
        }
        else
        {
            // don't add
            retval = false;
        }
    }
    else
    {
        if (data.startsWith("The "))
            data = data.mid(4) + ", The";
        if (data.startsWith("A "))
            data = data.mid(2) + ", A";
        if (data.startsWith("An "))
            data = data.mid(3) + ", An";
    }

    return retval;
}

void ProgFinder::restoreSelectedData(QString &data)
{
    if (data.endsWith(", The"))
        data = "The " + data.left(data.length() - 5);
    if (data.endsWith(", A"))
        data = "A " + data.left(data.length() - 3);
}

///////////////////////////////////////////////////////////////////////////////
// Japanese specific program finder

// japanese HIRAGANA list and more
const QChar JaProgFinder::searchChars[] =
{
    // "あ", "か", "さ", "た",
    QChar(0x3042), QChar(0x304b), QChar(0x3055), QChar(0x305f),
    // "な", "は", "ま", "や",
    QChar(0x306a), QChar(0x306f), QChar(0x307e), QChar(0x3084),
    // "ら", "わ", "英", "数",
    QChar(0x3089), QChar(0x308f), QChar(0x82f1), QChar(0x6570),
    0,
};

JaProgFinder::JaProgFinder(MythScreenStack *parentStack, bool gg,
                           TV *player, bool embedVideo)
            : ProgFinder(parentStack, gg, player, embedVideo)
{
    for (numberOfSearchChars = 0; !searchChars[numberOfSearchChars].isNull();
         ++numberOfSearchChars)
         ;
}

void JaProgFinder::initAlphabetList()
{
    for (int charNum = 0; charNum < numberOfSearchChars; ++charNum)
    {
        new MythUIButtonListItem(m_alphabetList, QString(searchChars[charNum]));
    }
}

// search title_pronounce
// we hope japanese HIRAGANA and alphabets, numerics is inserted
// these character must required ZENKAKU
// because query work not fine, if mysql's default charset latin1
void JaProgFinder::whereClauseGetSearchData(QString &where, MSqlBindings &bindings)
{
    QDateTime progStart = MythDate::current();
    int charNum = m_alphabetList->GetCurrentPos();

    where = "SELECT DISTINCT title FROM program "
            "LEFT JOIN channel ON program.chanid = channel.chanid "
            "WHERE channel.visible = 1 ";

    switch (charNum) {
    case 0:
        where += "AND ( title_pronounce >= 'あ' AND title_pronounce <= 'お') ";
        break;
    case 1:
        where += "AND ( title_pronounce >= 'か' AND title_pronounce <= 'ご') ";
        break;
    case 2:
        where += "AND ( title_pronounce >= 'さ' AND title_pronounce <= 'そ') ";
        break;
    case 3:
        where += "AND ( title_pronounce >= 'た' AND title_pronounce <= 'ど') ";
        break;
    case 4:
        where += "AND ( title_pronounce >= 'な' AND title_pronounce <= 'の') ";
        break;
    case 5:
        where += "AND ( title_pronounce >= 'は' AND title_pronounce <= 'ぽ') ";
        break;
    case 6:
        where += "AND ( title_pronounce >= 'ま' AND title_pronounce <= 'も') ";
        break;
    case 7:
        where += "AND ( title_pronounce >= 'や' AND title_pronounce <= 'よ') ";
        break;
    case 8:
        where += "AND ( title_pronounce >= 'ら' AND title_pronounce <= 'ろ') ";
        break;
    case 9:
        where += "AND ( title_pronounce >= 'わ' AND title_pronounce <= 'ん') ";
        break;
    case 10:
        where += "AND ( title_pronounce >= 'Ａ' AND title_pronounce <= 'ｚ') ";
        break;
    case 11:
        where += "AND ( title_pronounce >= '０' AND title_pronounce <= '９') ";
        break;
    }

    where += "AND starttime > :STARTTIME ";

    if (!m_searchStr.isEmpty())
    {
        where += "AND title_pronounce LIKE :SEARCH ";
        bindings[":SEARCH"] = '%' + m_searchStr + '%';
    }

    where += "ORDER BY title_pronounce;";

    bindings[":STARTTIME"] = progStart.addSecs(50 - progStart.time().second());
}


bool JaProgFinder::formatSelectedData(QString& data)
{
    (void)data;
    return true;
}

bool JaProgFinder::formatSelectedData(QString& data, int charNum)
{
    (void)data;
    (void)charNum;
    return true;
}

void JaProgFinder::restoreSelectedData(QString& data)
{
    (void)data;
}

// Hebrew specific program finder

// Hebrew alphabet list and more
const QChar HeProgFinder::searchChars[] =
{
    // "א", "ב", "ג", "ד",
    QChar(0x5d0), QChar(0x5d1), QChar(0x5d2), QChar(0x5d3),
    // "ה", "ו", "ז", "ח",
    QChar(0x5d4), QChar(0x5d5), QChar(0x5d6), QChar(0x5d7),
    // "ט", "י", "כ", "ל",
    QChar(0x5d8), QChar(0x5d9), QChar(0x5db), QChar(0x5dc),
    // "מ", "נ", "ס", "ע",
    QChar(0x5de), QChar(0x5e0), QChar(0x5e1), QChar(0x5e2),
    // "פ", "צ", "ק", "ר",
    QChar(0x5e4), QChar(0x5e6), QChar(0x5e7), QChar(0x5e8),
    // "ש", "ת", "E", "#",
    QChar(0x5e9), QChar(0x5ea), QChar('E'), QChar('#'),
    QChar(0),
};

HeProgFinder::HeProgFinder(MythScreenStack *parentStack, bool gg,
                           TV *player, bool embedVideo)
            : ProgFinder(parentStack, gg, player, embedVideo)
{
    for (numberOfSearchChars = 0; !searchChars[numberOfSearchChars].isNull();
         ++numberOfSearchChars)
        ;
}

void HeProgFinder::initAlphabetList()
{
    for (int charNum = 0; charNum < numberOfSearchChars; ++charNum)
    {
        new MythUIButtonListItem(m_alphabetList, QString(searchChars[charNum]));
    }
}

// search by hebrew aleph-bet
// # for all numbers, E for all latin
void HeProgFinder::whereClauseGetSearchData(QString &where, MSqlBindings &bindings)
{
    QDateTime progStart = MythDate::current();
    QString searchChar = m_alphabetList->GetValue();

    if (searchChar.isEmpty())
        searchChar = searchChars[0];

    where = "SELECT DISTINCT title FROM program "
            "LEFT JOIN channel ON program.chanid = channel.chanid "
            "WHERE channel.visible = 1 ";

    if (searchChar.contains('E'))
    {
        where += "AND ( title REGEXP '^[A-Z]') ";
    }
    else if (searchChar.contains('#'))
    {
        where += "AND ( title REGEXP '^[0-9]') ";
    }
    else
    {
        QString one = searchChar + '%';
        bindings[":ONE"] = one;
        where += "AND ( title LIKE :ONE ) ";
    }

    where += "AND starttime > :STARTTIME ";

    if (!m_searchStr.isEmpty())
    {
        where += "AND title LIKE :SEARCH ";
        bindings[":SEARCH"] = '%' + m_searchStr + '%';
    }

    where += "ORDER BY title;";

    bindings[":STARTTIME"] = progStart.addSecs(50 - progStart.time().second());
}

bool HeProgFinder::formatSelectedData(QString& data)
{
    (void)data;
    return true;
}

bool HeProgFinder::formatSelectedData(QString& data, int charNum)
{
    (void)data;
    (void)charNum;
    return true;
}

void HeProgFinder::restoreSelectedData(QString& data)
{
    (void)data;
}

//////////////////////////////////////////////////////////////////////////////

// Cyrrilic specific program finder
// Cyrrilic alphabet list and more
const QChar RuProgFinder::searchChars[] =
{
    // "А", "Б", "В", "Г",
    QChar(0x410), QChar(0x411), QChar(0x412), QChar(0x413),
    // "Д", "Е", "Ё", "Ж",
    QChar(0x414), QChar(0x415), QChar(0x401), QChar(0x416),
    // "З", "И", "Й", "К",
    QChar(0x417), QChar(0x418), QChar(0x419), QChar(0x41a),
    // "Л", "М", "Н", "О",
    QChar(0x41b), QChar(0x41c), QChar(0x41d), QChar(0x41e),
    // "П", "Р", "С", "Т",
    QChar(0x41f), QChar(0x420), QChar(0x421), QChar(0x422),
    // "У", "Ф", "Х", "Ц",
    QChar(0x423), QChar(0x424), QChar(0x425), QChar(0x426),
    // "Ч", "Ш", "Щ", "Ъ",
    QChar(0x427), QChar(0x428), QChar(0x429), QChar(0x42a),
    // "Ы", "ь", "Э", "Ю",
    QChar(0x42b), QChar(0x44c), QChar(0x42d), QChar(0x42e),
    // "Я", "0", "1", "2",
    QChar(0x42f), QChar('0'),   QChar('1'),   QChar('2'),
    QChar('3'),   QChar('4'),   QChar('5'),   QChar('6'),
    QChar('7'),   QChar('8'),   QChar('9'),   QChar('@'),
    QChar('A'),   QChar('B'),   QChar('C'),   QChar('D'),
    QChar('E'),   QChar('F'),   QChar('G'),   QChar('H'),
    QChar('I'),   QChar('J'),   QChar('K'),   QChar('L'),
    QChar('M'),   QChar('N'),   QChar('O'),   QChar('P'),
    QChar('Q'),   QChar('R'),   QChar('S'),   QChar('T'),
    QChar('U'),   QChar('V'),   QChar('W'),   QChar('X'),
    QChar('Y'),   QChar('Z'), 0
};

RuProgFinder::RuProgFinder(MythScreenStack *parentStack, bool gg,
                           TV *player, bool embedVideo)
            : ProgFinder(parentStack, gg, player, embedVideo)
{
    for (numberOfSearchChars = 0; !searchChars[numberOfSearchChars].isNull();
         ++numberOfSearchChars)
        ;
}

void RuProgFinder::initAlphabetList()
{
    for (int charNum = 0; charNum < numberOfSearchChars; ++charNum)
    {
        new MythUIButtonListItem(m_alphabetList, searchChars[charNum]);
    }
}

// search by cyrillic and latin alphabet
// @ all programm
void RuProgFinder::whereClauseGetSearchData(QString &where, MSqlBindings
&bindings)
{
   QDateTime progStart = MythDate::current();
   QString searchChar = m_alphabetList->GetValue();

   if (searchChar.isEmpty())
       searchChar = searchChars[0];


  if (searchChar.contains('@'))
   {
       where = "SELECT DISTINCT title FROM program "
               "LEFT JOIN channel ON program.chanid = channel.chanid "
               "WHERE channel.visible = 1 AND "
               "( "
                  "title NOT REGEXP '^[A-Z0-9]' AND "
                  "title NOT REGEXP '^The [A-Z0-9]' AND "
                  "title NOT REGEXP '^A [A-Z0-9]' AND "
                  "title NOT REGEXP '^[0-9]' AND "
                  "starttime > :STARTTIME ) ";
       if (!m_searchStr.isEmpty())
       {
           where += "AND title LIKE :SEARCH ";
           bindings[":SEARCH"] = '%' + m_searchStr + '%';
       }

       where += "ORDER BY title;";

       bindings[":STARTTIME"] =
           progStart.addSecs(50 - progStart.time().second());
   }
   else
   {
       QString one = searchChar + '%';
       QString two = QString("The ") + one;
       QString three = QString("A ") + one;
       QString four = QString("An ") + one;
       QString five = QString("\"") + one;

       where = "SELECT DISTINCT title FROM program "
               "LEFT JOIN channel ON program.chanid = channel.chanid "
               "WHERE channel.visible = 1 "
               "AND ( title LIKE :ONE OR title LIKE :TWO "
               "      OR title LIKE :THREE "
               "      OR title LIKE :FOUR  "
               "      OR title LIKE :FIVE )"
               "AND starttime > :STARTTIME ";
       if (!m_searchStr.isEmpty())
           where += "AND title LIKE :SEARCH ";

       where += "ORDER BY title;";

       bindings[":ONE"] = one;
       bindings[":TWO"] = two;
       bindings[":THREE"] = three;
       bindings[":FOUR"] = four;
       bindings[":FIVE"] = five;
       bindings[":STARTTIME"] =
           progStart.addSecs(50 - progStart.time().second());

       if (!m_searchStr.isEmpty())
           bindings[":SEARCH"] = '%' + m_searchStr + '%';
   }
}

bool RuProgFinder::formatSelectedData(QString& data)
{
    (void)data;
    return true;
}

bool RuProgFinder::formatSelectedData(QString& data, int charNum)
{
    (void)data;
    (void)charNum;
    return true;
}

void RuProgFinder::restoreSelectedData(QString& data)
{
    (void)data;
}
//////////////////////////////////////////////////////////////////////////////

SearchInputDialog::SearchInputDialog(MythScreenStack *parent,
                                     const QString &defaultValue)
                 : MythTextInputDialog(parent, "", FilterNone,
                                       false, defaultValue)
{
}

bool SearchInputDialog::Create(void)
{
    if (!LoadWindowFromXML("schedule-ui.xml", "searchpopup", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_textEdit, "input", &err);
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'searchpopup'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(okButton, SIGNAL(Clicked()), SLOT(sendResult()));

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    m_textEdit->SetPassword(m_isPassword);
    connect(m_textEdit, SIGNAL(valueChanged()), SLOT(editChanged()));

    BuildFocusList();

    return true;
}

void SearchInputDialog::editChanged(void)
{
    QString inputString = m_textEdit->GetText();
    emit valueChanged(inputString);

    if (m_retObject)
    {
        //FIXME: add a new event type for this?
        DialogCompletionEvent *dce =
                new DialogCompletionEvent(m_id, 0, inputString, "");
        QCoreApplication::postEvent(m_retObject, dce);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
