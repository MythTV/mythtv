#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qsqldatabase.h>
#include <qhbox.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "custompriority.h"

#include "mythcontext.h"
#include "dialogbox.h"
#include "programinfo.h"
#include "proglist.h"
#include "scheduledrecording.h"
#include "recordingtypes.h"
#include "viewschdiff.h"
#include "mythdbcon.h"

CustomPriority::CustomPriority(MythMainWindow *parent, const char *name,
                       ProgramInfo *pginfo)
              : MythDialog(parent, name)
{
    ProgramInfo *p = new ProgramInfo();

    if (pginfo)
    {
        delete p;
        p = pginfo;
    }

    QString baseTitle = p->title;
    baseTitle.remove(QRegExp(" \\(.*\\)$"));

    QString quoteTitle = baseTitle;
    quoteTitle.replace("\'","\'\'");

    prevItem = 0;
    addString = tr("Add");

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QVBoxLayout *vkbox = new QVBoxLayout(vbox, (int)(1 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vkbox, (int)(1 * wmult));

    // Edit selection
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QString message = tr("Edit Priority Rule") + ": ";
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_rule = new MythComboBox( false, this, "rule");
    m_rule->setBackgroundOrigin(WindowOrigin);

    m_rule->insertItem(tr("<New priority rule>"));
    m_recpri   << "1";
    m_recdesc << "";

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT priorityname, recpriority, selectclause "
                   "FROM powerpriority ORDER BY priorityname;");

    int titlematch = -1;
    if (result.exec() && result.isActive())
    {
        while (result.next())
        {
            QString trimTitle = QString::fromUtf8(result.value(0).toString());
            trimTitle.remove(QRegExp(" \\(.*\\)$"));

            m_rule->insertItem(trimTitle);
            m_recpri   << result.value(1).toString();
            m_recdesc << QString::fromUtf8(result.value(2).toString());

            if (trimTitle == baseTitle)
                titlematch = m_rule->count() - 1;
        }
    }
    else
        MythContext::DBError("Get power search rules query", result);

    hbox->addWidget(m_rule);

    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Priority Rule Name") + ": ";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythRemoteLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    // Value edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Priority Value") + ": ";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_value = new MythSpinBox( this, "value" );
    m_value->setMinValue(-99);
    m_value->setMaxValue(99);
    m_value->setValue(1);
    m_value->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_value);

    // Example clause combo box
    m_clause = new MythComboBox( false, this, "clause");
    m_clause->setBackgroundOrigin(WindowOrigin);

    m_clause->insertItem(tr("Modify priority for an input (Input priority)"));
    m_csql << "cardinput.cardinputid = 1";

    m_clause->insertItem(tr("Modify priority for all inputs on a card"));
    m_csql << "cardinput.cardid = 2";

    m_clause->insertItem(tr("Modify priority for every card on a host"));
    m_csql << "capturecard.hostname = 'mythbox'";

    m_clause->insertItem(tr("Only one specific channel ID (Channel priority)"));
    m_csql << "channel.chanid = '1003' ";

    m_clause->insertItem(tr("Only a certian channel number"));
    m_csql << "channel.channum = '3' ";

    m_clause->insertItem(tr("Only channels that carry a specific station"));
    m_csql << "channel.callsign = 'ESPN' ";

    m_clause->insertItem(tr("Match related callsigns"));
    m_csql << "channel.callsign LIKE 'HBO%' ";

    m_clause->insertItem(tr("Only channels marked as commercial free"));
    m_csql << "channel.commmethod = -2 ";

    m_clause->insertItem(tr("Modify priority for a station on an input"));
    m_csql << "channel.callsign = 'ESPN' AND cardinput.cardinputid = 2";

    m_clause->insertItem(tr("Priority for all matching titles"));
    m_csql << "program.title LIKE 'CSI: %' ";

    m_clause->insertItem(tr("Only shows marked as HDTV"));
    m_csql << "program.hdtv > 0 ";

    m_clause->insertItem(tr("Close Captioned priority"));
    m_csql << "program.closecaptioned > 0 ";

    m_clause->insertItem(tr("New episodes only"));
    m_csql << "program.previouslyshown = 0 ";

    m_clause->insertItem(tr("Modify unidentified episodes"));
    m_csql << "program.generic = 0 ";

    m_clause->insertItem(tr("First showing of each episode"));
    m_csql << "program.first > 0 ";

    m_clause->insertItem(tr("Last showing of each episode"));
    m_csql << "program.last > 0 ";

    m_clause->insertItem(tr("Priority for any show with End Late time"));
    m_csql << "RECTABLE.endoffset > 0 ";

    m_clause->insertItem(tr("Priority for a category"));
    m_csql << "program.category = 'Reality' ";

    m_clause->insertItem(QString(tr("Priority for a category type") +
            " ('movie', 'series', 'sports' " + tr("or") + " 'tvshow')"));
    m_csql << "program.category_type = 'sports' ";

    m_clause->insertItem(tr("Modify priority by star rating (0.0 to 1.0 for movies only)"));
    m_csql << "program.stars >= 0.75 ";

    m_clause->insertItem(tr("Priority when shown once (complete example)"));
    m_csql << "program.first > 0 AND program.last > 0";

    m_clause->insertItem(tr("Prefer a host for a storage group (complete example)"));
    m_csql << QString("RECTABLE.storagegroup = 'Archive' \n"
                      "AND capturecard.hostname = 'mythbox' ");

    m_clause->insertItem(tr("Priority for HD shows under two hours (complete example)"));
    m_csql << QString("program.hdtv > 0 AND \nprogram.starttime > "
                      "DATE_SUB(program.endtime, INTERVAL 2 HOUR) ");

    m_clause->insertItem(tr("Priority for movies by the year of release (complete example)"));
    m_csql << "program.category_type = 'movie' AND program.airdate >= 2006 ";

    m_clause->insertItem(tr("Prefer movies when shown at night (complete example)"));
    m_csql << QString("program.category_type = 'movie' \n"
                      "AND HOUR(program.starttime) < 6 ");

    m_clause->insertItem(tr("Prefer a host for live sports with overtime (complete example)"));
    m_csql << QString("RECTABLE.endoffset > 0 \n"
                      "AND program.category = 'Sports event' \n"
                      "AND capturecard.hostname = 'mythbox' ");

    m_clause->insertItem(tr("Avoid poor signal quality (complete example)"));
    m_csql << QString("cardinput.cardinputid = 1 AND \n"
                      "channel.channum IN (3, 5, 39, 66) ");

    vbox->addWidget(m_clause);

    //  Add Button
    m_addButton = new MythPushButton( this, "add" );
    m_addButton->setBackgroundOrigin(WindowOrigin);
    m_addButton->setText(addString);
    m_addButton->setEnabled(true);

    vbox->addWidget(m_addButton);

    // Description edit box
    m_description = new MythRemoteLineEdit(5, this, "description" );
    m_description->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(m_description);

    //  Test Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_testButton = new MythPushButton( this, "test" );
    m_testButton->setBackgroundOrigin(WindowOrigin);
    m_testButton->setText( tr( "Test" ) );
    m_testButton->setEnabled(false);

    hbox->addWidget(m_testButton);

    //  Install Button
    m_installButton = new MythPushButton( this, "install" );
    m_installButton->setBackgroundOrigin(WindowOrigin);
    m_installButton->setText( tr( "Install" ) );
    m_installButton->setEnabled(false);

    hbox->addWidget(m_installButton);

    //  Delete Button
    m_deleteButton = new MythPushButton( this, "delete" );
    m_deleteButton->setBackgroundOrigin(WindowOrigin);
    m_deleteButton->setText( tr( "Delete" ) );
    m_deleteButton->setEnabled(false);

    hbox->addWidget(m_deleteButton);

    //  Cancel Button
    m_cancelButton = new MythPushButton( this, "cancel" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);

    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));
     
    connect(m_rule, SIGNAL(activated(int)), this, SLOT(ruleChanged(void)));
    connect(m_rule, SIGNAL(highlighted(int)), this, SLOT(ruleChanged(void)));
    connect(m_title, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));
    connect(m_addButton, SIGNAL(clicked()), this, SLOT(addClicked()));
    connect(m_clause, SIGNAL(activated(int)), this, SLOT(clauseChanged(void)));
    connect(m_clause, SIGNAL(highlighted(int)), this, SLOT(clauseChanged(void)));
    connect(m_description, SIGNAL(textChanged(void)), this,
            SLOT(textChanged(void)));
    connect(m_testButton, SIGNAL(clicked()), this, SLOT(testClicked()));
    connect(m_installButton, SIGNAL(clicked()), this, SLOT(installClicked()));
    connect(m_deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));

    gContext->addListener(this);
    gContext->addCurrentLocation("CustomPriority");

    if (titlematch >= 0)
    {
        m_rule->setCurrentItem(titlematch);
        ruleChanged();
    }
    else if (p->title > "")
    {
        m_title->setText(baseTitle);
        //m_value->setText("");
        m_description->setText("program.title = '" + quoteTitle + "' ");
        textChanged();
    }

    if (m_title->text().isEmpty())
        m_rule->setFocus();
    else 
        m_clause->setFocus();

    clauseChanged();
}

CustomPriority::~CustomPriority(void)
{
    gContext->removeListener(this);
    gContext->removeCurrentLocation();
}

void CustomPriority::ruleChanged(void)
{
    int curItem = m_rule->currentItem();
    if (curItem == prevItem)
        return;

    prevItem = curItem;

    if (curItem > 0)
        m_title->setText(m_rule->currentText());
    else
        m_title->setText("");

    m_description->setText(m_recdesc[curItem]);
    m_value->setValue(m_recpri[curItem].toInt());
    m_deleteButton->setEnabled((bool) curItem);
    textChanged();
}

void CustomPriority::textChanged(void)
{
    bool hastitle = !m_title->text().isEmpty();
    bool hasdesc = !m_description->text().isEmpty();

    m_testButton->setEnabled(hasdesc);
    m_installButton->setEnabled(hastitle && hasdesc);
}

void CustomPriority::clauseChanged(void)
{
    QString msg = m_csql[m_clause->currentItem()];
    msg.replace("\n", " ");
    msg.replace(QRegExp(" [ ]*"), " ");
    msg = QString("%1: \"%2\"").arg(addString).arg(msg);
    if (msg.length() > 50)
    {
        msg.truncate(48);
        msg += "...\"";
    }
    m_addButton->setText(msg);
}

void CustomPriority::addClicked(void)
{
    QString clause = "";

    if (m_description->text().contains(QRegExp("\\S")))
        clause = "AND ";

    clause += m_csql[m_clause->currentItem()];
    m_description->append(clause);
}

void CustomPriority::testClicked(void)
{
    if (!checkSyntax())
    {
        m_testButton->setFocus();
        return;
    }
    testSchedule();
    m_testButton->setFocus();
}

void CustomPriority::installClicked(void)
{
    if (!checkSyntax())
    {
        m_installButton->setFocus();
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM powerpriority WHERE priorityname = :NAME;");
    query.bindValue(":NAME", m_title->text());

    if (!query.exec())
        MythContext::DBError("Install power search delete", query);

    query.prepare("INSERT INTO powerpriority "
                  "(priorityname, recpriority, selectclause) "
                  "VALUES(:NAME,:VALUE,:CLAUSE);");
    query.bindValue(":NAME", m_title->text());
    query.bindValue(":VALUE", m_value->value());
    query.bindValue(":CLAUSE", m_description->text());

    if (!query.exec())
        MythContext::DBError("Install power search insert", query);
    else
        ScheduledRecording::signalChange(0);

    accept();
}

void CustomPriority::deleteClicked(void)
{
    if (!checkSyntax())
    {
        m_deleteButton->setFocus();
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM powerpriority "
                   "WHERE priorityname=:NAME;");
    query.bindValue(":NAME", m_title->text());

    if (!query.exec())
        MythContext::DBError("Delete power search query", query);
    else
        ScheduledRecording::signalChange(0);

    accept();
}

void CustomPriority::cancelClicked(void)
{
    accept();
}

bool CustomPriority::checkSyntax(void)
{
    bool ret = false;
    QString msg = "";

    QString desc = m_description->text();

    if (desc.contains(QRegExp("^\\s*AND\\s", false)))
    {
        msg = "Power Priority rules do not reqiure a leading \"AND\"";
    }
    else if (desc.contains(";", false))
    {
        msg  = "Power Priority rules can not include semicolon ( ; ) ";
        msg += "statement terminators.";
    }
    else
    {
        QString qstr = QString("SELECT (%1)\nFROM (recordmatch, record, "
                               "program, channel, cardinput, capturecard, "
                               "oldrecorded) WHERE NULL").arg(desc);
        while (1)
        {
            int i = qstr.find("RECTABLE");
            if (i == -1) break;
            qstr = qstr.replace(i, strlen("RECTABLE"), "record");
        }

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(qstr);

        if (query.exec() && query.isActive())
        {
            ret = true;
        }
        else
        {
            msg = tr("An error was found when checking") + ":\n\n";
#if QT_VERSION >= 0x030200
            msg += query.executedQuery();
#else
            msg += query.lastQuery();
#endif
            msg += "\n\n" + tr("The database error was") + ":\n";
            msg += query.lastError().databaseText();
            ret = false;
        }
    }

    if (!msg.isEmpty())
    {
        DialogBox *errdiag = new DialogBox(gContext->GetMainWindow(), msg);
        errdiag->AddButton(QObject::tr("OK"));
        errdiag->exec();
        errdiag->deleteLater();
    }
    return ret;
}

void CustomPriority::testSchedule(void)
{

    QString ttable = "powerpriority_tmp";

    MSqlQueryInfo dbcon = MSqlQuery::SchedCon();
    MSqlQuery query(dbcon);
    QString thequery;

    thequery ="SELECT GET_LOCK(:LOCK, 2);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (Obtaining lock in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    thequery = QString("DROP TABLE IF EXISTS %1;").arg(ttable);
    query.prepare(thequery);
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (deleting old table in testRecording): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    thequery = QString("CREATE TABLE %1 SELECT * FROM powerpriority;")
                       .arg(ttable);
    query.prepare(thequery);
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (create new table): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }

    query.prepare(QString("DELETE FROM %1 WHERE priorityname = :NAME;")
                          .arg(ttable));
    query.bindValue(":NAME", m_title->text());

    if (!query.exec())
        MythContext::DBError("Test power search delete", query);

    thequery = QString("INSERT INTO %1 "
                       "(priorityname, recpriority, selectclause) "
                       "VALUES(:NAME,:VALUE,:CLAUSE);").arg(ttable);
    query.prepare(thequery);
    query.bindValue(":NAME", m_title->text());
    query.bindValue(":VALUE", m_value->value());
    query.bindValue(":CLAUSE", m_description->text());

    if (!query.exec())
        MythContext::DBError("Test power search insert", query);

    QString ltitle = tr("Power Priority");
    if (!m_title->text().isEmpty())
        ltitle = m_title->text();

    ViewScheduleDiff vsd(gContext->GetMainWindow(), "Preview Schedule Changes",
                         ttable, 0, ltitle);

    thequery = "SELECT RELEASE_LOCK(:LOCK);";
    query.prepare(thequery);
    query.bindValue(":LOCK", "DiffSchedule");
    query.exec();
    if (query.lastError().type() != QSqlError::None)
    {
        QString msg =
            QString("DB Error (free lock): \n"
                    "Query was: %1 \nError was: %2 \n")
            .arg(thequery)
            .arg(MythContext::DBErrorMessage(query.lastError()));
        VERBOSE(VB_IMPORTANT, msg);
        return;
    }
    vsd.exec();
}
