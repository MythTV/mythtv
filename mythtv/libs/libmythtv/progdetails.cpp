#include <iostream>
using namespace std;

#include <QPixmap>
#include <QImage>
#include <QApplication>
#include <QKeyEvent>

#include "mythcontext.h"
#include "mythdialogs.h"
#include "uitypes.h"

#include "progdetails.h"

ProgDetails::ProgDetails(MythScreenStack *parent, const char *name)
           : MythScreenType (parent, name)
{
    m_browser = NULL;
}

bool ProgDetails::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("ui.xml", "progdetails", this);

    if (!foundtheme)
        return false;

    m_browser = dynamic_cast<MythUIWebBrowser *>(GetChild("browser"));

    if (!m_browser)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_browser);

    m_browser->SetBackgroundColor(QColor(100,100,100,100));

    return true;
}

void ProgDetails::setDetails(const QString &details)
{
    m_browser->SetHtml(details);
}

QString ProgDetails::themeText(const QString &fontName, const QString &text, int size)
{
    if (size < 1) size = 1;
    if (size > 7) size = 7;

//    XMLParse *theme = getTheme();
//    if (!theme)
//        return text;

    MythFontProperties *font = GetFont(fontName);

    if (!font)
        return text;

    QString res = QString("<font color=\"%1\" face=\"%2\" size=\"%3\"</font>")
            .arg(font->color().name())
            .arg(font->face().family())
            .arg(size);

    bool bItalic = font->face().italic();
    bool bBold = font->face().bold();
    bool bUnderline = font->face().underline();

    if (bItalic)
        res += "<i>";
    if (bBold)
        res += "<b>";
    if (bUnderline)
        res += "<u>";

    res += text;

    if (bItalic)
        res += "</i>";
    if (bBold)
        res += "</b>";
    if (bUnderline)
        res += "</u>";

    return res;
}

ProgDetails::~ProgDetails(void)
{
}

bool ProgDetails::keyPressEvent(QKeyEvent *event)
{
    cout << "ProgDetails - keypress1" << endl;
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    cout << "ProgDetails - keypress2" << endl;

    if (MythScreenType::keyPressEvent(event))
        return true;

    return false;
}
