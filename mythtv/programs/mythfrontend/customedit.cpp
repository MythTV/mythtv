
#include "customedit.h"

// QT
#include <QSqlError>

// libmythdb
#include "mythdb.h"

// libmyth
#include "mythcorecontext.h"

// libmythui
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythuibutton.h"
#include "mythdialogbox.h"

// libmythtv
#include "recordingrule.h"

// mythfrontend
#include "scheduleeditor.h"
#include "proglist.h"

CustomEdit::CustomEdit(MythScreenStack *parent, ProgramInfo *pginfo)
              : MythScreenType(parent, "CustomEdit")
{
    if (pginfo)
        m_pginfo = new ProgramInfo(*pginfo);
    else
        m_pginfo = new ProgramInfo();

    prevItem = 0;
    maxex = 0;
    seSuffix = QString(" (%1)").arg(tr("stored search"));
    exSuffix = QString(" (%1)").arg(tr("stored example"));

    gCoreContext->addListener(this);
}

CustomEdit::~CustomEdit(void)
{
    delete m_pginfo;

    gCoreContext->removeListener(this);
}

bool CustomEdit::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "customedit", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_ruleList,   "rules", &err);
    UIUtilE::Assign(this, m_clauseList,  "clauses", &err);

    UIUtilE::Assign(this, m_titleEdit,       "title", &err);
    UIUtilE::Assign(this, m_subtitleEdit,    "subtitle", &err); 
    UIUtilE::Assign(this, m_descriptionEdit, "description", &err); 
    UIUtilE::Assign(this, m_clauseText,      "clausetext", &err);
    UIUtilE::Assign(this, m_testButton,      "test", &err);
    UIUtilE::Assign(this, m_recordButton,    "record", &err);
    UIUtilE::Assign(this, m_storeButton,     "store", &err);
    UIUtilE::Assign(this, m_cancelButton,    "cancel", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'customedit'");
        return false;
    }

    connect(m_ruleList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(ruleChanged(MythUIButtonListItem *)));
    connect(m_titleEdit, SIGNAL(valueChanged(void)), this,
                SLOT(textChanged(void)));
    m_titleEdit->SetMaxLength(128);
    m_subtitleEdit->SetMaxLength(128);
    connect(m_descriptionEdit, SIGNAL(valueChanged(void)), this,
                SLOT(textChanged(void)));
    m_descriptionEdit->SetMaxLength(0);

    connect(m_clauseList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(clauseChanged(MythUIButtonListItem *)));
    connect(m_clauseList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(clauseClicked(MythUIButtonListItem *)));

    connect(m_testButton, SIGNAL(Clicked()), SLOT(testClicked()));
    connect(m_recordButton, SIGNAL(Clicked()), SLOT(recordClicked()));
    connect(m_storeButton, SIGNAL(Clicked()), SLOT(storeClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    loadData();
    BuildFocusList();

    return true;
}


void CustomEdit::loadData(void)
{
    QString baseTitle = m_pginfo->GetTitle();
    baseTitle.remove(QRegExp(" \\(.*\\)$"));

    CustomRuleInfo rule;
    rule.recordid = '0';

    new MythUIButtonListItem(m_ruleList, tr("<New rule>"),
                             qVariantFromValue(rule));

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, title, subtitle, description "
                   "FROM record WHERE search = :SEARCH ORDER BY title;");
    result.bindValue(":SEARCH", kPowerSearch);

    if (result.exec())
    {
        while (result.next())
        {
            QString trimTitle = result.value(1).toString();
            trimTitle.remove(QRegExp(" \\(.*\\)$"));

            rule.recordid = result.value(0).toString();
            rule.title = trimTitle;
            rule.subtitle = result.value(2).toString();
            rule.description = result.value(3).toString();

            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_ruleList, rule.title,
                                         qVariantFromValue(rule));

            if (trimTitle == baseTitle ||
                result.value(0).toUInt() == m_pginfo->GetRecordingRuleID())
                m_ruleList->SetItemCurrent(item);
        }
    }
    else
        MythDB::DBError("Get power search rules query", result);

    loadClauses();

    if (m_ruleList->GetCurrentPos() == 0 && !m_pginfo->GetTitle().isEmpty())
    {
        m_titleEdit->SetText(baseTitle);
        QString quoteTitle = baseTitle;
        quoteTitle.replace("\'","\'\'");
        m_descriptionEdit->SetText("program.title = '" + quoteTitle + "' ");
    }
    textChanged();
}

void CustomEdit::loadClauses()
{
    QString baseTitle = m_pginfo->GetTitle();
    baseTitle.remove(QRegExp(" \\(.*\\)$"));

    QString quoteTitle = baseTitle;
    quoteTitle.replace("\'","\'\'");

    CustomRuleInfo rule;
  
    rule.title = tr("Match an exact title");
    if (!m_pginfo->GetTitle().isEmpty())
        rule.description = QString("program.title = '%1' ").arg(quoteTitle);
    else 
        rule.description = "program.title = 'Nova' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                              qVariantFromValue(rule));

    if (!m_pginfo->GetSeriesID().isEmpty()) 
    {
        rule.title = tr("Match this series");
        rule.subtitle.clear();
        rule.description = QString("program.seriesid = '%1' ")
            .arg(m_pginfo->GetSeriesID());
        new MythUIButtonListItem(m_clauseList, rule.title,
                                 qVariantFromValue(rule));
    }

    rule.title = tr("Match words in the title");
    rule.subtitle.clear();
    if (!m_pginfo->GetTitle().isEmpty())
        rule.description = QString("program.title LIKE '\%%1\%' ")
                                   .arg(quoteTitle);
    else
        rule.description = "program.title LIKE 'CSI: %' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Match words in the subtitle");
    rule.subtitle.clear();
    if (!m_pginfo->GetSubtitle().isEmpty())
    {
        QString subt = m_pginfo->GetSubtitle();
        subt.replace("\'","\'\'");
        rule.description = QString("program.subtitle LIKE '\%%1\%' ")
                                   .arg(subt);
    }
    else
        rule.description = "program.subtitle LIKE '%Las Vegas%' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    if (!m_pginfo->GetProgramID().isEmpty())
    {
        rule.title = tr("Match this episode");
        rule.subtitle.clear();
        rule.description = QString("program.programid = '%1' ")
            .arg(m_pginfo->GetProgramID()); 
    }
    else if (!m_pginfo->GetSubtitle().isEmpty())
    {
        rule.title = tr("Match this episode");
        rule.subtitle.clear();
        rule.description = QString("program.subtitle = '%1' \n"
                                   "AND program.description = '%2' ")
            .arg(m_pginfo->GetSubtitle().replace("\'","\'\'"))
            .arg(m_pginfo->GetDescription().replace("\'","\'\'"));
    }
    else
    {
        rule.title = tr("Match an exact episode");
        rule.subtitle.clear();
        rule.description = QString("program.title = 'Seinfeld' \n"
                                   "AND program.subtitle = 'The Soup' ");
    }
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Match in any descriptive field");
    rule.subtitle.clear();
    rule.description = QString("(program.title LIKE '%Japan%' \n"
                               "     OR program.subtitle LIKE '%Japan%' \n"
                               "     OR program.description LIKE '%Japan%') ");
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("New episodes only");
    rule.subtitle.clear();
    rule.description =  "program.previouslyshown = 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Exclude unidentified episodes");
    rule.subtitle.clear();
    rule.description =  "program.generic = 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("First showing of each episode");
    rule.subtitle.clear();
    rule.description =  "program.first > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Last showing of each episode");
    rule.subtitle.clear();
    rule.description =  "program.last > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Anytime on a specific day of the week");
    rule.subtitle.clear();
    rule.description = QString("DAYNAME(program.starttime) = '%1' ")
        .arg(m_pginfo->GetScheduledStartTime().toString("dddd"));
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only on weekdays (Monday through Friday)");
    rule.subtitle.clear();
    rule.description = "WEEKDAY(program.starttime) < 5 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only on weekends");
    rule.subtitle.clear();
    rule.description = "WEEKDAY(program.starttime) >= 5 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only in primetime");
    rule.subtitle.clear();
    rule.description = QString("HOUR(program.starttime) >= 19 \n"
                               "AND HOUR(program.starttime) < 23 ");
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Not in primetime");
    rule.subtitle.clear();
    rule.description = QString("(HOUR(program.starttime) < 19 \n"
                               "      OR HOUR(program.starttime) >= 23) ");
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only on a specific station");
    rule.subtitle.clear();
    if (!m_pginfo->GetChannelSchedulingID().isEmpty())
        rule.description = QString("channel.callsign = '%1' ")
            .arg(m_pginfo->GetChannelSchedulingID());
    else
        rule.description = "channel.callsign = 'ESPN' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Exclude one station");
    rule.subtitle.clear();
    rule.description = "channel.callsign != 'GOLF' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Match related callsigns");
    rule.subtitle.clear();
    rule.description = "channel.callsign LIKE 'HBO%' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only channels from a specific video source");
    rule.subtitle.clear();
    rule.description = "channel.sourceid = 2 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));
  
    rule.title = tr("Only channels marked as commercial free");
    rule.subtitle.clear();
    rule.description = QString("channel.commmethod = %1 ")
                               .arg(COMM_DETECT_COMMFREE); 
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only shows marked as HDTV");
    rule.subtitle.clear();
    rule.description = "program.hdtv > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only shows marked as widescreen");
    rule.subtitle.clear();
    rule.description = "FIND_IN_SET('WIDESCREEN', program.videoprop) > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Exclude H.264 encoded streams (EIT only)");
    rule.subtitle.clear();
    rule.description = "FIND_IN_SET('AVC', program.videoprop) = 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Only shows with in-vision signing");
    rule.subtitle.clear();
    rule.description = "FIND_IN_SET('SIGNED', program.subtitletypes) > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));
    
    rule.title = tr("Only shows with in-vision subtitles");
    rule.subtitle.clear();
    rule.description = "FIND_IN_SET('ONSCREEN', program.subtitletypes) > 0 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));
    
    rule.title = tr("Limit by category");
    rule.subtitle.clear();
    if (!m_pginfo->GetCategory().isEmpty())
        rule.description = QString("program.category = '%1' ")
            .arg(m_pginfo->GetCategory());
    else
        rule.description = "program.category = 'Reality' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("All matches for a genre (Data Direct)");
    rule.subtitle = "LEFT JOIN programgenres ON "
                    "program.chanid = programgenres.chanid AND "
                    "program.starttime = programgenres.starttime ";
    if (!m_pginfo->GetCategory().isEmpty())
        rule.description = QString("programgenres.genre = '%1' ")
            .arg(m_pginfo->GetCategory());
    else
        rule.description = "programgenres.genre = 'Reality' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Limit by MPAA or VCHIP rating (Data Direct)");
    rule.subtitle = "LEFT JOIN programrating ON "
                    "program.chanid = programrating.chanid AND "
                    "program.starttime = programrating.starttime ";
    rule.description = "(programrating.rating = 'G' OR programrating.rating "
                       "LIKE 'TV-Y%') ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = QString(tr("Category type") +
                   " ('movie', 'series', 'sports' " + tr("or") + " 'tvshow')");
    rule.subtitle.clear();
    rule.description = "program.category_type = 'sports' ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Limit movies by the year of release");
    rule.subtitle.clear();
    rule.description = "program.category_type = 'movie' AND "
                       "program.airdate >= 2000 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Minimum star rating (0.0 to 1.0 for movies only)");
    rule.subtitle.clear();
    rule.description = "program.stars >= 0.75 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Person named in the credits (Data Direct)");
    rule.subtitle = ", people, credits";
    rule.description = "people.name = 'Tom Hanks' \n"
                       "AND credits.person = people.person \n"
                       "AND program.chanid = credits.chanid \n"
                       "AND program.starttime = credits.starttime ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

/*  This shows how to use oldprogram but is a bad idea in practice.
    This would match all future showings until the day after the first
    showing when all future showing are no longer 'new' titles.

    rule.title = tr("Only titles from the New Titles list");
    rule.subtitle = "LEFT JOIN oldprogram"
                    " ON oldprogram.oldtitle = program.title ";
    rule.description = "oldprogram.oldtitle IS NULL ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));
*/

    rule.title = tr("Multiple sports teams (complete example)"); 
    rule.subtitle.clear();
    rule.description = "program.title = 'NBA Basketball' \n"
                 "AND program.subtitle REGEXP '(Miami|Cavaliers|Lakers)' \n"
                 "AND program.first > 0 \n";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Sci-fi B-movies (complete example)");
    rule.subtitle.clear();
    rule.description = "program.category_type='movie' \n"
                       "AND program.category='Science fiction' \n"
                       "AND program.stars <= 0.5 AND program.airdate < 1970 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title =
        tr("SportsCenter Overnight (complete example - use FindDaily)");
    rule.subtitle.clear();
    rule.description = "program.title = 'SportsCenter' \n"
                       "AND HOUR(program.starttime) >= 2 \n"
                       "AND HOUR(program.starttime) <= 6 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("Movie of the Week (complete example - use FindWeekly)");
    rule.subtitle.clear();
    rule.description = "program.category_type='movie' \n"
                  "AND program.stars >= 1.0 AND program.airdate >= 1965 \n"
                  "AND DAYNAME(program.starttime) = 'Friday' \n"
                  "AND HOUR(program.starttime) >= 12 ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    rule.title = tr("First Episodes (complete example for Data Direct)");
    rule.subtitle.clear();
    rule.description = "program.first > 0 \n"
                  "AND program.programid LIKE 'EP%0001' \n"
                  "AND program.originalairdate = DATE(program.starttime) ";
    new MythUIButtonListItem(m_clauseList, rule.title,
                             qVariantFromValue(rule));

    maxex = m_clauseList->GetCount();

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT rulename,fromclause,whereclause,search "
                   "FROM customexample;");

    if (result.exec())
    {
        while (result.next())
        {
            QString str = result.value(0).toString();

            if (result.value(3).toInt() > 0)
                str += seSuffix;
            else
                str += exSuffix;

            rule.title = str;
            rule.subtitle = result.value(1).toString();
            rule.description = result.value(2).toString();
            new MythUIButtonListItem(m_clauseList, rule.title,
                                     qVariantFromValue(rule));
        }
    }
}

void CustomEdit::ruleChanged(MythUIButtonListItem *item)
{
    int curItem = m_ruleList->GetCurrentPos();

    if (curItem == prevItem)
        return;

    if (!item)
        return;

    CustomRuleInfo rule = qVariantValue<CustomRuleInfo>(item->GetData());

    m_titleEdit->SetText(rule.title);
    m_descriptionEdit->SetText(rule.description);
    m_subtitleEdit->SetText(rule.subtitle);

    textChanged();

    prevItem = curItem;
}

void CustomEdit::textChanged(void)
{
    bool hastitle = !m_titleEdit->GetText().isEmpty();
    bool hasdesc = !m_descriptionEdit->GetText().isEmpty();

    m_testButton->SetEnabled(hasdesc);
    m_recordButton->SetEnabled(hastitle && hasdesc);
    m_storeButton->SetEnabled(m_clauseList->GetCurrentPos() >= maxex ||
                              (hastitle && hasdesc));
}

void CustomEdit::clauseChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    CustomRuleInfo rule = qVariantValue<CustomRuleInfo>(item->GetData());

    QString msg = rule.description;
    msg.replace('\n', ' ');
    msg.replace(QRegExp(" [ ]*"), " ");
    msg = QString("\"%1\"").arg(msg);
    m_clauseText->SetText(msg);

    bool hastitle = !m_titleEdit->GetText().isEmpty();
    bool hasdesc = !m_descriptionEdit->GetText().isEmpty();

    m_storeButton->SetEnabled(m_clauseList->GetCurrentPos() >= maxex ||
                              (hastitle && hasdesc));
}

void CustomEdit::clauseClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    CustomRuleInfo rule = qVariantValue<CustomRuleInfo>(item->GetData());

    QString clause;
    QString desc = m_descriptionEdit->GetText();

    if (desc.contains(QRegExp("\\S")))
        clause = "AND ";
    clause += rule.description;
    m_descriptionEdit->SetText(desc.append(clause));

    QString sub = m_subtitleEdit->GetText();
    m_subtitleEdit->SetText(sub.append(rule.subtitle));
}

void CustomEdit::testClicked(void)
{
    if (!checkSyntax())
    {
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plSQLSearch,
                                    m_descriptionEdit->GetText(), 
                                    m_subtitleEdit->GetText());
    if (pl->Create())
    {
        mainStack->AddScreen(pl);
    }
    else
        delete pl;
}

void CustomEdit::recordClicked(void)
{
    if (!checkSyntax())
        return;

    RecordingRule *record = new RecordingRule();

    MythUIButtonListItem* item = m_ruleList->GetItemCurrent();
    CustomRuleInfo rule = qVariantValue<CustomRuleInfo>(item->GetData());
     
    int cur_recid = rule.recordid.toInt();
    if (cur_recid > 0)
    {
        record->ModifyPowerSearchByID(cur_recid, m_titleEdit->GetText(),
                                      m_descriptionEdit->GetText(),
                                      m_subtitleEdit->GetText());
    }
    else
    {
        record->LoadBySearch(kPowerSearch, m_titleEdit->GetText(),
                             m_descriptionEdit->GetText(),
                             m_subtitleEdit->GetText());
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        connect(schededit, SIGNAL(ruleSaved(int)), SLOT(scheduleCreated(int)));
    }
    else
        delete schededit;
}

void CustomEdit::scheduleCreated(int ruleID)
{
    if (ruleID > 0)
        Close();
}

void CustomEdit::storeClicked(void)
{
    bool nameExists = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT rulename,whereclause FROM customexample "
                  "WHERE rulename = :RULE;");
    query.bindValue(":RULE", m_titleEdit->GetText());

    if (query.exec() && query.next())
        nameExists = true;

    QString msg = QString("%1: %2\n\n").arg(tr("Current Example"))
                                       .arg(m_titleEdit->GetText());

    if (m_subtitleEdit->GetText().length())
        msg += m_subtitleEdit->GetText() + "\n\n";

    msg += m_descriptionEdit->GetText();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MythDialogBox *storediag = new MythDialogBox(msg, mainStack,
                                                 "storePopup", true);

    storediag->SetReturnEvent(this, "storeruledialog");
    if (storediag->Create())
    {
        QString action = tr("Store");
        if (nameExists)
            action = tr("Replace");

        QString str = QString("%1 \"%2\"").arg(action)
                                          .arg(m_titleEdit->GetText());

        if (!m_titleEdit->GetText().isEmpty())
        {
            QString str2;
            str2 = QString("%1 %2").arg(str).arg(tr("as a search"));
            storediag->AddButton(str2);

            str2 = QString("%1 %2").arg(str).arg(tr("as an example"));
            storediag->AddButton(str2);
        }
        if (m_clauseList->GetCurrentPos() >= maxex)
        {
            MythUIButtonListItem* item = m_clauseList->GetItemCurrent();
            str = QString("%1 \"%2\"").arg(tr("Delete"))
                                      .arg(item->GetText());
            storediag->AddButton(str);
        }
        mainStack->AddScreen(storediag);
    }
    else
        delete storediag;
}


bool CustomEdit::checkSyntax(void)
{
    bool ret = false;
    QString msg;

    QString desc = m_descriptionEdit->GetText();
    QString from = m_subtitleEdit->GetText();
    if (desc.contains(QRegExp("^\\s*AND\\s", Qt::CaseInsensitive)))
    {
        msg = tr("Power Search rules no longer reqiure a leading \"AND\".");
    }
    else if (desc.contains(';'))
    {
        msg  = tr("Power Search rules can not include semicolon ( ; ) ");
        msg += tr("statement terminators.");
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("SELECT NULL FROM (program,channel) "
                              "%1 WHERE\n%2").arg(from).arg(desc));

        if (query.exec())
        {
            ret = true;
        }
        else
        {
            msg = tr("An error was found when checking") + ":\n\n";
            msg += query.executedQuery();
            msg += "\n\n" + tr("The database error was") + ":\n";
            msg += query.lastError().databaseText();
        }
    }

    if (!msg.isEmpty())
    {
        MythScreenStack *popupStack = GetMythMainWindow()->
                                              GetStack("popup stack");
        MythConfirmationDialog *checkSyntaxPopup =
               new MythConfirmationDialog(popupStack, msg, false);

        if (checkSyntaxPopup->Create())
        {
            checkSyntaxPopup->SetReturnEvent(this, "checkSyntaxPopup");
            popupStack->AddScreen(checkSyntaxPopup);
        }
        else
        {
            delete checkSyntaxPopup;
        }
        ret = false;
    }
    return ret;
}

void CustomEdit::storeRule(bool is_search, bool is_new)
{
    CustomRuleInfo rule;
    rule.recordid = '0';
    rule.title = m_titleEdit->GetText();
    rule.subtitle = m_subtitleEdit->GetText();
    rule.description = m_descriptionEdit->GetText();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("REPLACE INTO customexample "
                   "(rulename,fromclause,whereclause,search) "
                   "VALUES(:RULE,:FROMC,:WHEREC,:SEARCH);");
    query.bindValue(":RULE", rule.title);
    query.bindValue(":FROMC", rule.subtitle);
    query.bindValue(":WHEREC", rule.description);
    query.bindValue(":SEARCH", is_search);

    if (is_search)
        rule.title += seSuffix;
    else
        rule.title += exSuffix;

    if (!query.exec())
        MythDB::DBError("Store custom example", query);
    else if (is_new)
    {
        new MythUIButtonListItem(m_clauseList, rule.title,
                                 qVariantFromValue(rule));
    }
    else
    {
        /* Modify the existing entry.  We know one exists from the database
           search but do not know its position in the clause list.  It may
           or may not be the current item. */
        for (int i = maxex; i < m_clauseList->GetCount(); i++)
        {
            MythUIButtonListItem* item = m_clauseList->GetItemAt(i);
            QString removedStr = item->GetText().remove(seSuffix)
                                                .remove(exSuffix);
            if (m_titleEdit->GetText() == removedStr)
            {
                item->SetData(qVariantFromValue(rule));
                clauseChanged(item);
                break;
            }
        }
    }
}

void CustomEdit::deleteRule(void)
{
    MythUIButtonListItem* item = m_clauseList->GetItemCurrent();
    if (!item)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM customexample "
                  "WHERE rulename = :RULE;");
    query.bindValue(":RULE", item->GetText().remove(seSuffix)
                                            .remove(exSuffix));
    
    if (!query.exec())
        MythDB::DBError("Delete custom example", query);
    else
    {
        m_clauseList->RemoveItem(item);
    }
}

void CustomEdit::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "storeruledialog")
        {
             if (resulttext.startsWith(tr("Delete")))
             {
                 deleteRule();
             }
             else if (!resulttext.isEmpty())
             {
                 storeRule(resulttext.contains(tr("as a search")),
                           !resulttext.startsWith(tr("Replace")));
             }
        }
    }
}
