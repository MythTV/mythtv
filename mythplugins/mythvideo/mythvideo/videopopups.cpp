#include <QStringList>

#include <mythtv/libmythdb/mythverbose.h>

#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibutton.h>

#include "videopopups.h"
#include "metadata.h"
#include "videoutils.h"

CastDialog::CastDialog(MythScreenStack *lparent, Metadata *metadata) :
    MythScreenType(lparent, "videocastpopup"), m_metadata(metadata)
{
}

bool CastDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "castpopup", this))
        return false;

    MythUIButtonList *castList;
    MythUIButton *okButton;

    try
    {
        UIUtilE::Assign(this, castList, "cast");
        UIUtilE::Assign(this, okButton, "ok");
    }
    catch (UIUtilException &e)
    {
        VERBOSE(VB_IMPORTANT, e.What());
        return false;
    }

    connect(okButton, SIGNAL(buttonPressed()), SLOT(Close()));

    okButton->SetText(tr("OK"));

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

PlotDialog::PlotDialog(MythScreenStack *lparent, Metadata *metadata) :
    MythScreenType(lparent, "videoplotpopup"), m_metadata(metadata)
{
}

bool PlotDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "plotpopup", this))
        return false;

    MythUIText *plotText;
    MythUIButton *okButton;

    try
    {
        UIUtilE::Assign(this, plotText, "plot");
        UIUtilE::Assign(this, okButton, "ok");
    }
    catch (UIUtilException &e)
    {
        VERBOSE(VB_IMPORTANT, e.What());
        return false;
    }

    plotText->SetText(m_metadata->Plot());
    okButton->SetText(tr("OK"));

    connect(okButton, SIGNAL(buttonPressed()), SLOT(Close()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

/////////////////////////////////////////////////////////////

SearchResultsDialog::SearchResultsDialog(MythScreenStack *lparent,
        const SearchListResults &results) :
    MythScreenType(lparent, "videosearchresultspopup"), m_results(results),
    m_resultsList(0)
{
}

bool SearchResultsDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "moviesel", this))
        return false;

    try
    {
        UIUtilE::Assign(this, m_resultsList, "results");
    }
    catch (UIUtilException &e)
    {
        VERBOSE(VB_IMPORTANT, e.What());
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

    connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(sendResult(MythUIButtonListItem *)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

void SearchResultsDialog::sendResult(MythUIButtonListItem* item)
{
    emit haveResult(item->GetData().toString());
    Close();
}
