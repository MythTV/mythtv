// C++ headers
#include <iostream>

// QT headers
#include <QStringList>

// Myth headers
#include <mythtv/libmythdb/mythverbose.h>

// Mythui headers
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibutton.h>

// Mythvideo headers
#include "videopopups.h"

CastDialog::CastDialog(MythScreenStack *parent, Metadata *metadata)
           : MythScreenType(parent, "videocastpopup")
{
    m_metadata = metadata;
}

bool CastDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "castpopup", this);

    if (!foundtheme)
        return false;

    MythUIButtonList *castList = dynamic_cast<MythUIButtonList *>
                                                        (GetChild("cast"));
    MythUIButton *okButton = dynamic_cast<MythUIButton *>
                                                        (GetChild("ok"));

    if (!castList || !okButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    connect(okButton, SIGNAL(buttonPressed()), this, SLOT(Close()));

    okButton->SetText(tr("Ok"));

    QStringList cast = GetCastList(*m_metadata);
    QStringListIterator castIterator(cast);
    while (castIterator.hasNext())
    {
        new MythUIButtonListItem(castList, castIterator.next());
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

/////////////////////////////////////////////////////////////

PlotDialog::PlotDialog(MythScreenStack *parent, Metadata *metadata)
           : MythScreenType(parent, "videoplotpopup")
{
    m_metadata = metadata;
}

bool PlotDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "plotpopup", this);

    if (!foundtheme)
        return false;

    MythUIText *plotText = dynamic_cast<MythUIText *>
                                            (GetChild("plot"));
    MythUIButton *okButton = dynamic_cast<MythUIButton *>
                                        (GetChild("ok"));

    if (!plotText || !okButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    plotText->SetText(m_metadata->Plot());
    okButton->SetText(tr("Ok"));

    connect(okButton, SIGNAL(buttonPressed()), this, SLOT(Close()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

/////////////////////////////////////////////////////////////

SearchResultsDialog::SearchResultsDialog(MythScreenStack *parent,
                                         const SearchListResults &results)
           : MythScreenType(parent, "videosearchresultspopup")
{
    m_results = results;
    m_resultsList = NULL;
}

bool SearchResultsDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "moviesel", this);

    if (!foundtheme)
        return false;

    m_resultsList = dynamic_cast<MythUIButtonList *> (GetChild("results"));
//     MythUIButton *cancelButton = dynamic_cast<MythUIButton *>
//                                         (GetChild("cancel"));

    if (!m_resultsList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    QMapIterator<QString, QString> resultsIterator(m_results);
    while (resultsIterator.hasNext())
    {
        resultsIterator.next();
        MythUIButtonListItem *button =
            new MythUIButtonListItem(m_resultsList, resultsIterator.value());
        QString key = resultsIterator.key();
        button->SetData(key);
    }

     connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem*)),
             this, SLOT(sendResult(MythUIButtonListItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

void SearchResultsDialog::sendResult(MythUIButtonListItem* item)
{
    QString video_uid = item->GetData().toString();
    emit haveResult(video_uid);
    Close();
}
