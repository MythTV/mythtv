// Qt includes
#include <qlayout.h>
#include <qfontmetrics.h>
#include <qpainter.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythMusic includes
#include "search.h"

SearchDialog::SearchDialog(MythMainWindow *parent, const char *name) 
    : MythPopupBox(parent, name)
{
    // Create the widgets

    // Caption
    caption = addLabel(QString(tr("Search Music Database")));
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

    // LineEdit for search string
    searchText = new MythLineEdit(this);
    searchText->setRW();
    searchText->setFocus();
    searchText->setPopupPosition(VK_POSBOTTOMDIALOG);
    connect(searchText, SIGNAL(textChanged(const QString &)),
            this, SLOT(searchTextChanged(const QString &)));
    addWidget(searchText); 

    // Listbox for search results
    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    connect(listbox, SIGNAL(accepted(int)), this, SLOT(itemSelected(int)));
    addWidget(listbox);

    // buttons
    okButton = addButton(tr("OK"), this, SLOT(okPressed()));
    cancelButton = addButton(tr("Cancel"), this, SLOT(cancelPressed()));

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

    QStringList list = QStringList::split(QRegExp("[>,]"), searchText);
    whereClause = "";
    if (!searchText.isEmpty())
    {
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
        QString aComposer(query.value(1).toString());
        QString aTitle(query.value(2).toString());
        aTitle.truncate( 30 );
        // Append artist/title/album info... ok this is ugly :(
        QString text(aComposer + ": " + aTitle + "; " +
                     query.value(3).toString() );

        // Highlight matches as appropriate
        if(!searchText.isEmpty()) 
        {
            if (substringSearch) // alpha
            {
                for (uint i = 0; i < list.count(); i++) 
                {
                    QString stxt = list[i].stripWhiteSpace();
                    int index = -1;
                    while( (index = text.findRev(stxt, index, false)) != -1 ) 
                    {
                        text.insert(index + stxt.length(), "]");
                        text.insert(index, "[");
                    }
                }
            }
            else // numeric
            {
                for (uint i = 0; i < list.count(); i++) 
                {
                    QString stxt = list[i].stripWhiteSpace();
                    int index = -1;
                    while( (index = text.findRev(QRegExp(stxt, false), index)) != -1 )
                    {
                        text.insert(index + stxt.contains('['), "]");
                        text.insert(index, "[");
                    }
                }
            }
        }

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
    done(0);
}


void SearchDialog::getWhereClause(QString &whereClause)
{
    whereClause = this->whereClause;
}

SearchDialog::~SearchDialog()
{
}

void SearchDialog::okPressed(void)
{
    done(0);
}

void SearchDialog::cancelPressed(void)
{
    done(-1);
}

void SearchListBoxItem::paint(QPainter *p)
{
    int itemHeight = height(listBox());
    QFontMetrics fm = p->fontMetrics();
    int yPos = ((itemHeight - fm.height()) / 2) + fm.ascent();

    QColor normalColor = p->pen().color();
    QColor highlightColor = QColor("yellow"); // should be themeable

    QString sText = text();
    int xPos = 3;
    int start, end;
    int index = 0;
    QString sNormal, sHighlight;

    while (index < (int) sText.length())
    {
        start = sText.find('[', index);
        end = sText.find(']', start);

        if (start != -1 && end != -1)
        {
            sNormal = sText.mid(index, start - index);
            sHighlight = sText.mid(start + 1, end - start -1);
            index = end + 1;
        }
        else
        {
            sNormal = sText.mid(index, 0xffffffff);
            sHighlight = "";
            index = sText.length();
        } 

        if (sNormal != "")
        {
            p->setPen(normalColor);
            p->drawText(xPos, yPos, sNormal);
            xPos += fm.width(sNormal);
        }

        if (sHighlight != "")
        {
            p->setPen(highlightColor);
            p->drawText(xPos, yPos, sHighlight);
            xPos += fm.width(sHighlight);
        }
    }
}
