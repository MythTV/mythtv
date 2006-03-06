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

#include "customrecord.h"

#include "mythcontext.h"
#include "dialogbox.h"
#include "programinfo.h"
#include "proglist.h"
#include "scheduledrecording.h"
#include "recordingtypes.h"
#include "mythdbcon.h"

CustomRecord::CustomRecord(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    prevItem = 0;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QVBoxLayout *vkbox = new QVBoxLayout(vbox, (int)(1 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vkbox, (int)(1 * wmult));

    // Edit selection
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QString message = tr("Edit Rule") + ": ";
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_rule = new MythComboBox( false, this, "rule");
    m_rule->setBackgroundOrigin(WindowOrigin);

    m_rule->insertItem(tr("<New rule>"));
    m_recid   << "0";
    m_recsub  << "";
    m_recdesc << "";

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, title, subtitle, description "
                   "FROM record WHERE search = :SEARCH ORDER BY title;");
    result.bindValue(":SEARCH", kPowerSearch);

    if (result.exec() && result.isActive())
    {
        while (result.next())
        {
            QString trimTitle = QString::fromUtf8(result.value(1).toString());
            trimTitle.remove(QRegExp(" \\(.*\\)$"));

            m_rule->insertItem(trimTitle);
            m_recid   << result.value(0).toString();
            m_recsub  << QString::fromUtf8(result.value(2).toString());
            m_recdesc << QString::fromUtf8(result.value(3).toString());
        }
    }
    else
        MythContext::DBError("Get power search rules query", result);

    hbox->addWidget(m_rule);

    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Rule Name") + ": ";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythRemoteLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    m_clause = new MythComboBox( false, this, "clause");
    m_clause->setBackgroundOrigin(WindowOrigin);

    m_clause->insertItem(tr("Match an exact title"));
    m_cfrom << "";
    m_csql << "program.title = 'Nova' ";

    m_clause->insertItem(tr("Match an exact episode"));
    m_cfrom << "";
    m_csql << QString("program.title = 'Seinfeld' \n"
                      "AND program.subtitle = 'The Soup' ");

    m_clause->insertItem(tr("Match words in the title"));
    m_cfrom << "";
    m_csql << "program.title LIKE '%Junkyard%' ";

    m_clause->insertItem(tr("Match in any descriptive field"));
    m_cfrom << "";
    m_csql << QString("(program.title LIKE '%Japan%' \n"
                      "     OR program.subtitle LIKE '%Japan%' \n"
                      "     OR program.description LIKE '%Japan%') ");

    m_clause->insertItem(tr("Limit by category"));
    m_cfrom << "";
    m_csql << "program.category = 'Reality' ";

    m_clause->insertItem(tr("All matches for a genre (Data Direct)"));
    m_cfrom << "LEFT JOIN programgenres ON "
               "program.chanid = programgenres.chanid AND "
               "program.starttime = programgenres.starttime ";
    m_csql << "programgenres.genre = 'Reality' ";

    m_clause->insertItem(tr("Limit by MPAA or VCHIP rating (Data Direct)"));
    m_cfrom << "LEFT JOIN programrating ON "
               "program.chanid = programrating.chanid AND "
               "program.starttime = programrating.starttime ";
    m_csql << "(programrating.rating = 'G' OR programrating.rating LIKE 'TV-Y%') ";

    m_clause->insertItem(tr("New episodes only"));
    m_cfrom << "";
    m_csql << "program.previouslyshown = 0 ";

    m_clause->insertItem(tr("Exclude unidentified episodes"));
    m_cfrom << "";
    m_csql << "program.generic = 0 ";

    m_clause->insertItem(QString(tr("Category type") +
            " ('movie', 'series', 'sports' " + tr("or") + " 'tvshow')"));
    m_cfrom << "";
    m_csql << "program.category_type = 'sports' ";

    m_clause->insertItem(tr("Limit movies by the year of release"));
    m_cfrom << "";
    m_csql << "program.category_type = 'movie' AND program.airdate >= 2000 ";

    m_clause->insertItem(tr("Minimum star rating (0.0 to 1.0 for movies only)"));
    m_cfrom << "";
    m_csql << "program.stars >= 0.75 ";

    m_clause->insertItem(tr("Person named in the credits (Data Direct)"));
    m_cfrom << ", people, credits";
    m_csql << QString("people.name = 'Tom Hanks' \n"
                      "AND credits.person = people.person \n"
                      "AND program.chanid = credits.chanid \n"
                      "AND program.starttime = credits.starttime ");

    m_clause->insertItem(tr("Only on a specific station"));
    m_cfrom << "";
    m_csql << "channel.callsign = 'ESPN' ";

    m_clause->insertItem(tr("Exclude one station"));
    m_cfrom << "";
    m_csql << "channel.callsign != 'GOLF' ";

    m_clause->insertItem(tr("Match related callsigns"));
    m_cfrom << "";
    m_csql << "channel.callsign LIKE 'HBO%' ";

    m_clause->insertItem(tr("Only on channels marked as favorites"));
    m_cfrom << ", favorites";
    m_csql << "program.chanid = favorites.chanid ";

    m_clause->insertItem(tr("Only channels from a specific video source"));
    m_cfrom << "";
    m_csql << "channel.sourceid = 2 ";

    m_clause->insertItem(tr("Only channels marked as commercial free"));
    m_cfrom << "";
    m_csql << "channel.commfree > 0 ";

    m_clause->insertItem(tr("Only shows marked as HDTV"));
    m_cfrom << "";
    m_csql << "program.hdtv > 0 ";

    m_clause->insertItem(tr("Anytime on a specific day of the week"));
    m_cfrom << "";
    m_csql << "DAYNAME(program.starttime) = 'Tuesday' ";

    m_clause->insertItem(tr("Only on weekdays (Monday through Friday)"));
    m_cfrom << "";
    m_csql << "WEEKDAY(program.starttime) < 5 ";

    m_clause->insertItem(tr("Only on weekends"));
    m_cfrom << "";
    m_csql << "WEEKDAY(program.starttime) >= 5 ";

    m_clause->insertItem(tr("Only in primetime"));
    m_cfrom << "";
    m_csql << QString("HOUR(program.starttime) >= 19 \n"
                      "AND HOUR(program.starttime) < 23 ");

    m_clause->insertItem(tr("Not in primetime"));
    m_cfrom << "";
    m_csql << QString("(HOUR(program.starttime) < 19 \n"
                      "      OR HOUR(program.starttime) >= 23) ");

/*  This shows how to use oldprogram but is a bad idea in practice.
    This would match all future showings until the day after the first
    showing when all future showing are no longer 'new' titles.

    m_clause->insertItem(tr("Only titles from the New Titles list"));
    m_cfrom << "LEFT JOIN oldprogram ON oldprogram.oldtitle = program.title ";
    m_csql << "oldprogram.oldtitle IS NULL ";
*/

    m_clause->insertItem(tr("Multiple sports teams (complete example)"));
    m_cfrom << "";
    m_csql << QString("program.title = 'NBA Basketball' \n"
              "AND program.subtitle REGEXP '(Miami|Cavaliers|Lakers)' ");

    m_clause->insertItem(tr("Sci-fi B-movies (complete example)"));
    m_cfrom << "";
    m_csql << QString("program.category_type='movie' \n"
              "AND program.category='Science fiction' \n"
              "AND program.stars <= 0.5 AND program.airdate < 1970 ");

    m_clause->insertItem(tr("SportsCenter Overnight (complete example - use FindDaily)"));
    m_cfrom << "";
    m_csql << QString("program.title = 'SportsCenter' \n"
              "AND HOUR(program.starttime) >= 2 \n"
              "AND HOUR(program.starttime) <= 6 ");

    m_clause->insertItem(tr("Movie of the Week (complete example - use FindWeekly)"));
    m_cfrom << "";
    m_csql << QString("program.category_type='movie' \n"
              "AND program.stars >= 1.0 AND program.airdate >= 1965 \n"
              "AND DAYNAME(program.starttime) = 'Friday' \n"
              "AND HOUR(program.starttime) >= 12 ");

    m_clause->insertItem(tr("First Episodes (complete example for Data Direct)"));
    m_cfrom << "";
    m_csql << QString("program.previouslyshown = 0 \n"
              "AND program.programid LIKE 'EP%0001' \n"
              "AND DAYOFYEAR(program.originalairdate) = \n"
              "    DAYOFYEAR(program.starttime) ");

    vbox->addWidget(m_clause);

    // Preview box
    m_preview = new MythRemoteLineEdit( this, "preview" );
    m_preview->setBackgroundOrigin(WindowOrigin);
    m_preview->setEnabled(false);
    vbox->addWidget(m_preview);

    //  Add Button
    m_addButton = new MythPushButton( this, "add" );
    m_addButton->setBackgroundOrigin(WindowOrigin);
    m_addButton->setText( tr( "Add this example clause" ) );
    m_addButton->setEnabled(true);

    vbox->addWidget(m_addButton);

    // Subtitle edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Additional Tables") + ": ";
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_subtitle = new MythRemoteLineEdit(this, "subtitle" );
    m_subtitle->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_subtitle);

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

    //  Record Button
    m_recordButton = new MythPushButton( this, "record" );
    m_recordButton->setBackgroundOrigin(WindowOrigin);
    m_recordButton->setText( tr( "Record" ) );
    m_recordButton->setEnabled(false);

    hbox->addWidget(m_recordButton);

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
    connect(m_recordButton, SIGNAL(clicked()), this, SLOT(recordClicked()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));

    gContext->addListener(this);
    gContext->addCurrentLocation("CustomRecord");

    if (m_title->text().isEmpty())
        m_rule->setFocus();
    else 
        m_clause->setFocus();

    clauseChanged();
}

CustomRecord::~CustomRecord(void)
{
    gContext->removeListener(this);
    gContext->removeCurrentLocation();
}

void CustomRecord::ruleChanged(void)
{
    int curItem = m_rule->currentItem();
    if (curItem == prevItem)
        return;

    prevItem = curItem;

    if (curItem > 0)
        m_title->setText(m_rule->currentText());
    else
        m_title->setText("");

    m_subtitle->setText(m_recsub[curItem]);
    m_description->setText(m_recdesc[curItem]);
    textChanged();
}

void CustomRecord::textChanged(void)
{
    bool hastitle = !m_title->text().isEmpty();
    bool hasdesc = !m_description->text().isEmpty();

    m_testButton->setEnabled(hasdesc);
    m_recordButton->setEnabled(hastitle && hasdesc);
}

void CustomRecord::clauseChanged(void)
{
    QString msg = m_csql[m_clause->currentItem()];
    msg.replace("\n", " ");
    msg.replace(QRegExp(" [ ]*"), " ");
    m_preview->setText(msg);
}

void CustomRecord::addClicked(void)
{
    QString clause = "";

    if (m_description->text().contains(QRegExp("\\S")))
        clause = "AND ";

    clause += m_csql[m_clause->currentItem()];
    m_description->append(clause);
    m_subtitle->append(m_cfrom[m_clause->currentItem()]);
}

void CustomRecord::testClicked(void)
{
    if (!checkSyntax())
    {
        m_testButton->setFocus();
        return;
    }

    ProgLister *pl = new ProgLister(plSQLSearch, m_description->text(), 
                                    m_subtitle->text(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;

    m_testButton->setFocus();
}

void CustomRecord::recordClicked(void)
{
    if (!checkSyntax())
    {
        m_recordButton->setFocus();
        return;
    }

    ScheduledRecording record;

    int cur_recid = m_recid[m_rule->currentItem()].toInt();

    if (cur_recid > 0)
        record.modifyPowerSearchByID(cur_recid, m_title->text(),
                                     m_subtitle->text(),
                                     m_description->text()); 
    else
        record.loadBySearch(kPowerSearch, m_title->text(),
                            m_subtitle->text(), m_description->text());
    record.exec();

    if (record.getRecordID())
        accept();
    else
        m_recordButton->setFocus();
}

void CustomRecord::cancelClicked(void)
{
    accept();
}

bool CustomRecord::checkSyntax(void)
{
    bool ret = false;
    QString msg = "";

    QString desc = m_description->text();
    QString from = m_subtitle->text();
    if (desc.contains(QRegExp("^\\s*AND\\s", false)))
    {
        msg = "Power Search rules no longer reqiure a leading \"AND\"";
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("SELECT NULL FROM (program,channel) "
                              "%1 WHERE\n%2").arg(from).arg(desc));

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
        }
    }

    if (msg > "")
    {
        DialogBox *errdiag = new DialogBox(gContext->GetMainWindow(), msg);
        errdiag->AddButton(QObject::tr("OK"));
        errdiag->exec();

        delete errdiag;
        ret = false;
    }
    return ret;
}
