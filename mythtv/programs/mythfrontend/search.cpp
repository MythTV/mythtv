#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <qhbox.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "search.h"

#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"

Search::Search(MythMainWindow *parent, const char *name)
              : MythDialog(parent, name)
{
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(80 * wmult),
                                        (int)(20 * wmult));

    // Window title
    QString message = tr("Search Program Listings");
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    vbox->addWidget(label);

    QVBoxLayout *vkbox = new QVBoxLayout(vbox, (int)(1 * wmult));
    QHBoxLayout *hbox = new QHBoxLayout(vkbox, (int)(1 * wmult));

    // Title edit box
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Keyword:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_title = new MythRemoteLineEdit( this, "title" );
    m_title->setBackgroundOrigin(WindowOrigin);
    hbox->addWidget(m_title);

    //  Search Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_searchButton = new MythPushButton( this, "Program" );
    m_searchButton->setBackgroundOrigin(WindowOrigin);
    m_searchButton->setText( tr( "Search for this keyword in titles" ) );
    m_searchButton->setEnabled(false);

    hbox->addWidget(m_searchButton);

    //  Description Search Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_descButton = new MythPushButton( this, "Program" );
    m_descButton->setBackgroundOrigin(WindowOrigin);
    m_descButton->setText( tr( "Search in titles and descriptions" ) );
    m_descButton->setEnabled(false);

    hbox->addWidget(m_descButton);

    //  What's New Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_newButton = new MythPushButton( this, "Program" );
    m_newButton->setBackgroundOrigin(WindowOrigin);
    m_newButton->setText( tr( "What's New" ) );
    m_newButton->setEnabled(true);

    hbox->addWidget(m_newButton);

    //  Cancel Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_cancelButton = new MythPushButton( this, "Program" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);

    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

    connect(m_title, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));
    connect(m_searchButton, SIGNAL(clicked()), this, SLOT(runSearch()));
    connect(m_descButton, SIGNAL(clicked()), this, SLOT(runDescSearch()));
    connect(m_newButton, SIGNAL(clicked()), this, SLOT(runNewList()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    
    gContext->addListener(this);
}

Search::~Search(void)
{
    gContext->removeListener(this);
}

void Search::textChanged(void)
{
    m_searchButton->setEnabled(!m_title->text().isEmpty());
    m_descButton->setEnabled(!m_title->text().isEmpty());
}

void Search::runSearch(void)
{
    ProgLister *pl = new ProgLister(plTitleSearch, m_title->text(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::runDescSearch(void)
{
    ProgLister *pl = new ProgLister(plDescSearch, m_title->text(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::runNewList(void)
{
    ProgLister *pl = new ProgLister(plNewListings, m_title->text(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::cancelClicked(void)
{
    accept();
}
