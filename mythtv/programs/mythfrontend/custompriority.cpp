
#include "custompriority.h"

// qt
#include <QSqlError>

// libmythbase
#include "mythdb.h"
#include "mythlogging.h"

// libmyth
#include "mythcorecontext.h"

// libmythtv
#include "scheduledrecording.h"
#include "channelutil.h"

// libmythui
#include "mythuibuttonlist.h"
#include "mythuispinbox.h"
#include "mythuitextedit.h"
#include "mythuibutton.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"

//mythfrontend
#include "viewschedulediff.h"

CustomPriority::CustomPriority(MythScreenStack *parent, ProgramInfo *proginfo)
              : MythScreenType(parent, "CustomPriority"),
                m_ruleList(NULL),      m_clauseList(NULL),
                m_titleEdit(NULL),     m_descriptionEdit(NULL),
                m_prioritySpin(NULL),  m_addButton(NULL),
                m_testButton(NULL),    m_installButton(NULL),
                m_deleteButton(NULL),  m_cancelButton(NULL)
{
    if (proginfo)
        m_pginfo = new ProgramInfo(*proginfo);
    else
        m_pginfo = new ProgramInfo();

    gCoreContext->addListener(this);
}

CustomPriority::~CustomPriority(void)
{
    delete m_pginfo;

    gCoreContext->removeListener(this);
}

bool CustomPriority::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "custompriority", this))
        return false;

    m_ruleList = dynamic_cast<MythUIButtonList *>(GetChild("rules"));
    m_clauseList = dynamic_cast<MythUIButtonList *>(GetChild("clauses"));

    m_prioritySpin = dynamic_cast<MythUISpinBox *>(GetChild("priority"));

    m_titleEdit = dynamic_cast<MythUITextEdit *>(GetChild("title"));
    m_descriptionEdit = dynamic_cast<MythUITextEdit *>(GetChild("description"));

    m_addButton = dynamic_cast<MythUIButton *>(GetChild("add"));
    m_installButton = dynamic_cast<MythUIButton *>(GetChild("install"));
    m_testButton = dynamic_cast<MythUIButton *>(GetChild("test"));
    m_deleteButton = dynamic_cast<MythUIButton *>(GetChild("delete"));
    m_cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));

    if (!m_ruleList || !m_clauseList || !m_prioritySpin || !m_titleEdit ||
        !m_descriptionEdit || !m_addButton || !m_installButton ||
        !m_testButton || !m_deleteButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "CustomPriority, theme is missing required elements");
        return false;
    }

    connect(m_ruleList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(ruleChanged(MythUIButtonListItem *)));

    connect(m_titleEdit, SIGNAL(valueChanged()), SLOT(textChanged()));
    m_titleEdit->SetMaxLength(128);
    connect(m_descriptionEdit, SIGNAL(valueChanged()), SLOT(textChanged()));
    m_descriptionEdit->SetMaxLength(0);

    connect(m_addButton, SIGNAL(Clicked()), SLOT(addClicked()));
    connect(m_testButton, SIGNAL(Clicked()), SLOT(testClicked()));
    connect(m_installButton, SIGNAL(Clicked()), SLOT(installClicked()));
    connect(m_deleteButton, SIGNAL(Clicked()), SLOT(deleteClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    loadData();

    BuildFocusList();

    return true;
}

void CustomPriority::loadData()
{
    QString baseTitle = m_pginfo->GetTitle();
    baseTitle.remove(QRegExp(" \\(.*\\)$"));

    QString quoteTitle = baseTitle;
    quoteTitle.replace("\'","\'\'");

    m_prioritySpin->SetRange(-99,99,1);
    m_prioritySpin->SetValue(1);

    RuleInfo rule;
    rule.priority = QString().setNum(1);

    new MythUIButtonListItem(m_ruleList, tr("<New priority rule>"),
                             qVariantFromValue(rule));

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT priorityname, recpriority, selectclause "
                   "FROM powerpriority ORDER BY priorityname;");

    if (result.exec())
    {
        MythUIButtonListItem *item = NULL;
        while (result.next())
        {
            QString trimTitle = result.value(0).toString();
            trimTitle.remove(QRegExp(" \\(.*\\)$"));

            rule.title = trimTitle;
            rule.priority = result.value(1).toString();
            rule.description = result.value(2).toString();

            item = new MythUIButtonListItem(m_ruleList, rule.title,
                                            qVariantFromValue(rule));

            if (trimTitle == baseTitle)
                m_ruleList->SetItemCurrent(item);
        }
    }
    else
        MythDB::DBError("Get power search rules query", result);

    loadExampleRules();

    if (!m_pginfo->GetTitle().isEmpty())
    {
        m_titleEdit->SetText(baseTitle);
        m_descriptionEdit->SetText("program.title = '" + quoteTitle + "' ");
        textChanged();
    }

    if (m_titleEdit->GetText().isEmpty())
        SetFocusWidget(m_ruleList);
    else
        SetFocusWidget(m_clauseList);
}

void CustomPriority::loadExampleRules()
{
    QMap<QString, QString> examples;
    examples.insert(tr("Modify priority for an input (Input priority)"),
                    "cardinput.cardinputid = 1");
    examples.insert(tr("Modify priority for all inputs on a card"),
                    "cardinput.cardid = 2");
    examples.insert(tr("Modify priority for every card on a host"),
                    "capturecard.hostname = 'mythbox'");
    examples.insert(tr("Only one specific channel ID (Channel priority)"),
                    "channel.chanid = '1003' ");
    examples.insert(tr("Only a certain channel number"),
                    "channel.channum = '3' ");
    examples.insert(tr("Only channels that carry a specific station"),
                    "channel.callsign = 'ESPN' ");
    examples.insert(tr("Match related callsigns"),
                    "channel.callsign LIKE 'HBO%' ");
    examples.insert(tr("Only channels marked as commercial free"),
                    QString("channel.commmethod = %1 ")
                        .arg(COMM_DETECT_COMMFREE));
    examples.insert(tr("Modify priority for a station on an input"),
                    "channel.callsign = 'ESPN' AND cardinput.cardinputid = 2");
    examples.insert(tr("Priority for all matching titles"),
                    "program.title LIKE 'CSI: %' ");
    examples.insert(tr("Only shows marked as HDTV"),
                    "program.hdtv > 0 ");
    examples.insert(tr("Close Captioned priority"),
                    "program.closecaptioned > 0 ");
    examples.insert(tr("New episodes only"),
                    "program.previouslyshown = 0 ");
    examples.insert(tr("Modify unidentified episodes"),
                    "program.generic = 0 ");
    examples.insert(tr("First showing of each episode"),
                    "program.first > 0 ");
    examples.insert(tr("Last showing of each episode"),
                    "program.last > 0 ");
    examples.insert(tr("Priority for any show with End Late time"),
                    "RECTABLE.endoffset > 0 ");
    examples.insert(tr("Priority for a category"),
                    "program.category = 'Reality' ");
    examples.insert(QString("%1 ('movie', 'series', 'sports', 'tvshow')")
                    .arg(tr("Priority for a category type")),
                    "program.category_type = 'sports' ");
    examples.insert(tr("Modify priority by star rating (0.0 to 1.0 for "
                       "movies only)"),
                    "program.stars >= 0.75 ");
    examples.insert(tr("Priority when shown once (complete example)"),
                    "program.first > 0 AND program.last > 0");
    examples.insert(tr("Prefer a host for a storage group (complete example)"),
                    QString("RECTABLE.storagegroup = 'Archive' "
                            "AND capturecard.hostname = 'mythbox' "));
    examples.insert(tr("Priority for HD shows under two hours (complete "
                       "example)"),
                    "program.hdtv > 0 AND program.starttime > "
                    "DATE_SUB(program.endtime, INTERVAL 2 HOUR) ");
    examples.insert(tr("Priority for movies by the year of release (complete "
                       "example)"),
                    "program.category_type = 'movie' "
                    "AND program.airdate >= 2006 ");
    examples.insert(tr("Prefer movies when shown at night (complete example)"),
                    "program.category_type = 'movie' "
                    "AND HOUR(program.starttime) < 6 ");
    examples.insert(tr("Prefer a host for live sports with overtime (complete "
                    "example)"),
                    "RECTABLE.endoffset > 0 "
                    "AND program.category = 'Sports event' "
                    "AND capturecard.hostname = 'mythbox' ");
    examples.insert(tr("Avoid poor signal quality (complete example)"),
                    "cardinput.cardinputid = 1 AND "
                    "channel.channum IN (3, 5, 39, 66) ");

    QMapIterator<QString, QString> it(examples);
    while (it.hasNext())
    {
        it.next();
        new MythUIButtonListItem(m_clauseList, it.key(),
                                 qVariantFromValue(it.value()));
    }
}

void CustomPriority::ruleChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    RuleInfo rule = item->GetData().value<RuleInfo>();

    m_titleEdit->SetText(rule.title);

    m_descriptionEdit->SetText(rule.description);
    m_prioritySpin->SetValue(rule.priority);
    m_deleteButton->SetEnabled((bool)m_ruleList->GetCurrentPos());
    textChanged();
}

void CustomPriority::textChanged(void)
{
    bool hastitle = !m_titleEdit->GetText().isEmpty();
    bool hasdesc = !m_descriptionEdit->GetText().isEmpty();

    m_testButton->SetEnabled(hasdesc);
    m_installButton->SetEnabled(hastitle && hasdesc);
}

void CustomPriority::addClicked(void)
{
    MythUIButtonListItem *item = m_clauseList->GetItemCurrent();

    if (!item)
        return;

    QString clause;

    QString desc = m_descriptionEdit->GetText();

    if (desc.contains(QRegExp("\\S")))
        clause = "AND ";
    clause = item->GetData().toString();
    m_descriptionEdit->SetText(desc.append(clause));
}

void CustomPriority::testClicked(void)
{
    if (!checkSyntax())
        return;

    testSchedule();
}

void CustomPriority::installClicked(void)
{
    if (!checkSyntax())
        return;

    MythUIButtonListItem *item = m_prioritySpin->GetItemCurrent();
    if (!item)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM powerpriority WHERE priorityname = :NAME;");
    query.bindValue(":NAME", m_titleEdit->GetText());

    if (!query.exec())
        MythDB::DBError("Install power search delete", query);

    query.prepare("INSERT INTO powerpriority "
                  "(priorityname, recpriority, selectclause) "
                  "VALUES(:NAME,:VALUE,:CLAUSE);");
    query.bindValue(":NAME", m_titleEdit->GetText());
    query.bindValue(":VALUE", item->GetText());
    query.bindValue(":CLAUSE", m_descriptionEdit->GetText());

    if (!query.exec())
        MythDB::DBError("Install power search insert", query);
    else
        ScheduledRecording::ReschedulePlace("InstallCustomPriority");

    Close();
}

void CustomPriority::deleteClicked(void)
{
    if (!checkSyntax())
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM powerpriority "
                   "WHERE priorityname=:NAME;");
    query.bindValue(":NAME", m_titleEdit->GetText());

    if (!query.exec())
        MythDB::DBError("Delete power search query", query);
    else
        ScheduledRecording::ReschedulePlace("DeleteCustomPriority");

    Close();
}

bool CustomPriority::checkSyntax(void)
{
    bool ret = false;
    QString msg;

    QString desc = m_descriptionEdit->GetText();

    if (desc.contains(QRegExp("^\\s*AND\\s", Qt::CaseInsensitive)))
    {
        msg = "Power Priority rules do not reqiure a leading \"AND\"";
    }
    else if (desc.contains(';'))
    {
        msg  = "Power Priority rules cannot include semicolon ( ; ) ";
        msg += "statement terminators.";
    }
    else
    {
        QString qstr = QString("SELECT (%1) FROM (recordmatch, record, "
                               "program, channel, cardinput, capturecard, "
                               "oldrecorded) WHERE NULL").arg(desc);
        while (1)
        {
            int i = qstr.indexOf("RECTABLE");
            if (i == -1) break;
            qstr = qstr.replace(i, strlen("RECTABLE"), "record");
        }

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(qstr);

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
            ret = false;
        }
    }

    if (!msg.isEmpty())
        ShowOkPopup(msg);

    return ret;
}

void CustomPriority::testSchedule(void)
{
    MythUIButtonListItem *item = m_prioritySpin->GetItemCurrent();
    if (!item)
        return;

    QString ttable = "powerpriority_tmp";

    MSqlQueryInfo dbcon = MSqlQuery::SchedCon();
    MSqlQuery query(dbcon);
    QString thequery;

    thequery = "SELECT GET_LOCK(:LOCK, 2);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    if (!query.exec())
    {
        QString msg =
            QString("DB Error (Obtaining lock in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythDB::DBErrorMessage(query.lastError()));
        LOG(VB_GENERAL, LOG_ERR, msg);
        return;
    }

    thequery = QString("DROP TABLE IF EXISTS %1;").arg(ttable);
    query.prepare(thequery);
    if (!query.exec())
    {
        QString msg =
            QString("DB Error (deleting old table in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythDB::DBErrorMessage(query.lastError()));
        LOG(VB_GENERAL, LOG_ERR, msg);
        return;
    }

    thequery = QString("CREATE TABLE %1 SELECT * FROM powerpriority;")
                       .arg(ttable);
    query.prepare(thequery);
    if (!query.exec())
    {
        QString msg =
            QString("DB Error (create new table): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythDB::DBErrorMessage(query.lastError()));
        LOG(VB_GENERAL, LOG_ERR, msg);
        return;
    }

    query.prepare(QString("DELETE FROM %1 WHERE priorityname = :NAME;")
                          .arg(ttable));
    query.bindValue(":NAME", m_titleEdit->GetText());

    if (!query.exec())
        MythDB::DBError("Test power search delete", query);

    thequery = QString("INSERT INTO %1 "
                       "(priorityname, recpriority, selectclause) "
                       "VALUES(:NAME,:VALUE,:CLAUSE);").arg(ttable);
    query.prepare(thequery);
    query.bindValue(":NAME", m_titleEdit->GetText());
    query.bindValue(":VALUE", item->GetText());
    query.bindValue(":CLAUSE", m_descriptionEdit->GetText());

    if (!query.exec())
        MythDB::DBError("Test power search insert", query);

    QString ltitle = tr("Power Priority");
    if (!m_titleEdit->GetText().isEmpty())
        ltitle = m_titleEdit->GetText();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ViewScheduleDiff *vsd = new ViewScheduleDiff(mainStack, ttable, 0, ltitle);

    if (vsd->Create())
        mainStack->AddScreen(vsd);
    else
        delete vsd;

    thequery = "SELECT RELEASE_LOCK(:LOCK);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    if (!query.exec())
    {
        QString msg =
            QString("DB Error (free lock): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythDB::DBErrorMessage(query.lastError()));
        LOG(VB_GENERAL, LOG_ERR, msg);
    }
}
