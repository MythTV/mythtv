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

    m_searchButton = new MythPushButton( this, "search" );
    m_searchButton->setBackgroundOrigin(WindowOrigin);
    m_searchButton->setText( tr( "Search for this keyword in titles" ) );
    m_searchButton->setEnabled(false);

    hbox->addWidget(m_searchButton);

    //  Description Search Button
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    m_descButton = new MythPushButton( this, "description" );
    m_descButton->setBackgroundOrigin(WindowOrigin);
    m_descButton->setText( tr( "Search in titles and descriptions" ) );
    m_descButton->setEnabled(false);

    hbox->addWidget(m_descButton);

    // Channel
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Channel: ");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_channel = new MythComboBox( false, this, "channel");
    m_channel->setBackgroundOrigin(WindowOrigin);

    QSqlQuery query;

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");

    QString thequery= QString("SELECT name FROM channel ORDER BY %1;")
                              .arg(chanorder);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected())
    {
        while(query.next())
        {
            QString channel = query.value(0).toString();
            m_channel->insertItem(channel);
        }
    }

    hbox->addWidget(m_channel);

    // Channel Button

    m_chanButton = new MythPushButton( this, "chanbutton" );
    m_chanButton->setBackgroundOrigin(WindowOrigin);
    m_chanButton->setText( tr( "Channel list" ) );
    m_chanButton->setEnabled(false);

    hbox->addWidget(m_chanButton);

    // Category
    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    message = tr("Category:");
    label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    hbox->addWidget(label);

    m_category = new MythComboBox( false, this, "category");
    m_category->setBackgroundOrigin(WindowOrigin);

    thequery= "SELECT category FROM program GROUP BY category;";

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected())
    {
        while(query.next())
        {
            QString category = query.value(0).toString();

            if (category > " ")
                m_category->insertItem(category);
        }
    }

    hbox->addWidget(m_category);

    // Category Button

    m_catButton = new MythPushButton( this, "catbutton" );
    m_catButton->setBackgroundOrigin(WindowOrigin);
    m_catButton->setText( tr( "Category list" ) );
    m_catButton->setEnabled(false);

    hbox->addWidget(m_catButton);

    hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    // Movie Button

    m_movieButton = new MythPushButton( this, "movies" );
    m_movieButton->setBackgroundOrigin(WindowOrigin);
    m_movieButton->setText( tr( "Movies" ) );
    m_movieButton->setEnabled(true);

    hbox->addWidget(m_movieButton);

    //  What's New Button

    m_newButton = new MythPushButton( this, "new" );
    m_newButton->setBackgroundOrigin(WindowOrigin);
    m_newButton->setText( tr( "What's New" ) );
    m_newButton->setEnabled(true);

    hbox->addWidget(m_newButton);

    // Cancel Button

    m_cancelButton = new MythPushButton( this, "cancel" );
    m_cancelButton->setBackgroundOrigin(WindowOrigin);
    m_cancelButton->setText( tr( "Cancel" ) );
    m_cancelButton->setEnabled(true);

    hbox->addWidget(m_cancelButton);

    connect(this, SIGNAL(dismissWindow()), this, SLOT(accept()));

    connect(m_title, SIGNAL(textChanged(void)), this, SLOT(textChanged(void)));
    connect(m_searchButton, SIGNAL(clicked()), this, SLOT(runSearch()));
    connect(m_descButton, SIGNAL(clicked()), this, SLOT(runDescSearch()));
    connect(m_channel, SIGNAL(activated(int)), this,
            SLOT(channelChanged(void)));
    connect(m_channel, SIGNAL(highlighted(int)), this,
            SLOT(channelChanged(void)));
    connect(m_chanButton, SIGNAL(clicked()), this, SLOT(runChannelList()));
    connect(m_category, SIGNAL(activated(int)), this,
            SLOT(categoryChanged(void)));
    connect(m_category, SIGNAL(highlighted(int)), this,
            SLOT(categoryChanged(void)));
    connect(m_catButton, SIGNAL(clicked()), this, SLOT(runCategoryList()));
    connect(m_movieButton, SIGNAL(clicked()), this, SLOT(runMovieList()));
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

void Search::channelChanged(void)
{
    m_chanButton->setEnabled(true);
}

void Search::categoryChanged(void)
{
    m_catButton->setEnabled(true);
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
    ProgLister *pl = new ProgLister(plNewListings, "",
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::runChannelList(void)
{
    ProgLister *pl = new ProgLister(plChannel, m_channel->currentText(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::runCategoryList(void)
{
    ProgLister *pl = new ProgLister(plCategory, m_category->currentText(),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::runMovieList(void)
{
    ProgLister *pl = new ProgLister(plMovies, tr("Movie"),
                                    QSqlDatabase::database(),
                                    gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void Search::cancelClicked(void)
{
    accept();
}
