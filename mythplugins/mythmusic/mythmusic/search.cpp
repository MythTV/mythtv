// Qt includes
#include <qlayout.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythMusic includes
#include "search.h"

SearchDialog::SearchDialog(MythMainWindow *parent, const char *name) 
    : MythPopupBox(parent, name)
{
    vbox = new QVBoxLayout((QWidget *) 0, (int)(10 * hmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));
        
    // Create the widgets

    // Caption
    caption = new QLabel(QString(tr("Search Music Database")), this);
    QFont font = caption->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    caption->setFont(font);
    caption->setPaletteForegroundColor(QColor("yellow"));
    caption->setBackgroundOrigin(ParentOrigin);
    caption->setAlignment(Qt::AlignCenter);
    caption->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                       QSizePolicy::Fixed));
    caption->setMinimumWidth((int)(600 * hmult));
    caption->setMaximumWidth((int)(600 * hmult));  
    hbox->addWidget(caption);
    
    // LineEdit for search string
    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    searchText = new MyLineEdit(this);
    searchText->setRW();
    searchText->setFocus();
    connect(searchText, SIGNAL(textChanged(const QString &)),
            this, SLOT(searchTextChanged(const QString &)));
    connect(searchText, SIGNAL(returnPressed()),
            this, SLOT(accept()));
    hbox->addWidget(searchText);    
    
    // Listbox for search results
    hbox = new QHBoxLayout(vbox, (int)(5 * hmult));
    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    connect(listbox, SIGNAL(accepted(int)),
            this, SLOT(itemSelected(int)));
    connect(this, SIGNAL(done()),
            this, SLOT(accept()));
    hbox->addWidget(listbox);
      
    addLayout(vbox);
    
    // Initially, fill list with all music
    runQuery("");
}

void SearchDialog::searchTextChanged(const QString &searchText)
{
    runQuery(searchText);
}

const char *mapping[] =
{
    "[0 ]",   "1",       "[2abc]", "[3def]", "[4ghi]",
    "[5jkl]", "[6mno]", "[7pqrs]", "[8tuv]", "[9wxyz]"
};

void SearchDialog::runQuery(QString searchText)
{
    // This method will perform a search in the 'musicmetadata' table and fill
    // the 'listbox' widget with the results.
    // The following columns are searched: filename, artist, album, title.
    // To facilitate usage with a remote, two search modes exist and
    // are enabled as follows:
    // - If searchText contains non-digits characters, a case-insensitive
    //   substring search is done.
    // - Otherwise, a regexp search is performed, where each digit can
    //   represent either itself or a letter as printed on some remotes
    //   i.e., 0 -> (0 or <space>), 1 -> 1, 2 -> (2 or a or b or c), etc.

    bool substringSearch = true;
    bool isNumber = false;
    searchText.toULongLong(&isNumber);

    if (!isNumber)
    {
        QString testString = searchText;
        testString.replace(">","");
        testString.toULongLong(&isNumber);
    }

    if (isNumber)
    {
        // Input contains only digits. Enable digit-to-char mapping mode
        for (int i = 0; i < 10; i++)
        {
            char c = '0' + i;
            searchText.replace(QChar(c), QString(mapping[i]));
        }
        substringSearch = false;
    }

    listbox->clear();

    MSqlQuery query(MSqlQuery::InitCon());

    QString queryString("SELECT filename, artist, album, title, intid "
                        "FROM musicmetadata ");

    if (!searchText.isEmpty())
    {
        QStringList list = QStringList::split(
            QRegExp("[>,]"), searchText);
        whereClause = "";
        if (substringSearch) // alpha
        {
             for (uint i = 0; i < list.count(); i++)
             {
                 QString stxt = list[i].stripWhiteSpace();
                 whereClause += (i) ? " AND ( " : "WHERE (";
                 whereClause +=
                    "filename LIKE '%" + stxt + "%' OR "
                    "artist   LIKE '%" + stxt + "%' OR "
                    "album    LIKE '%" + stxt + "%' OR "
                    "title    LIKE '%" + stxt + "%')";
             }
             VERBOSE(VB_GENERAL, QString("alpha whereClause " + whereClause ));
        }
        else // numeric
        {
            for (uint i = 0; i < list.count(); i++) 
            {
                QString stxt = list[i].stripWhiteSpace();
                whereClause += (i) ? " AND ( " : "WHERE (";
                whereClause +=
                    "filename REGEXP '" + stxt + "' OR "
                    "artist   REGEXP '" + stxt + "' OR "
                    "album    REGEXP '" + stxt + "' OR "
                    "title    REGEXP '" + stxt + "')";
            }
            VERBOSE(VB_GENERAL,QString("numeric whereClause " + whereClause ));
        }
    }

    queryString += whereClause;
    queryString += " ORDER BY artist, album, title, intid, filename ";

    query.prepare(queryString);

    bool has_entries = true;
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Search music database", query);
        has_entries = false;
    }
    has_entries &= (query.numRowsAffected() > 0);

    uint matchCount = 0;
    while (has_entries && query.next())
    {
        // Strip path from filename
        QString fileName(query.value(0).toString());
        fileName.replace(QRegExp(".*/"), "");

        QString aComposer(query.value(1).toString());
        // cut off composer's first name
        aComposer.replace(QRegExp(",.*"),"");

        QString aTitle(query.value(2).toString());
        // truncate Title at 30, cut off any trailing whitespace or ,
        aTitle.truncate( 30 );
        aTitle.replace(QRegExp(",*\\s*$"),"");

        // Append artist/title/album info... ok this is ugly :(
        QString text(aComposer + ": " + aTitle + "; " +
                     query.value(3).toString() );

        // Insert item into listbox, including song identifier
        listbox->insertItem(new SearchListBoxItem(
                                QString::fromUtf8(text),
                                query.value(4).toUInt()));

        matchCount++;
    }

    if (!has_entries)
    {
        listbox->setCurrentItem(0);
        listbox->setTopItem(0);
    }

    QString captionText = 
        tr("Search Music Database (%1 matches)").arg(matchCount);
    caption->setText(captionText);
}

void SearchDialog::itemSelected(int i)
{
    unsigned int id = ((SearchListBoxItem*)listbox->item(i))->getId();
    whereClause = QString("WHERE intid='%1';").arg(id);
    emit done();
}


void SearchDialog::getWhereClause(QString &whereClause)
{
    whereClause = this->whereClause;
}

SearchDialog::~SearchDialog()
{
    if (vbox)
    {
        delete vbox;
        vbox = NULL;
    }
}
