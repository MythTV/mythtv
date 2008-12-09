// c
#include <stdlib.h>
#include <unistd.h>
#include <cstdlib>

// qt
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextStream>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>
#include <mythtv/libmythui/mythuihelper.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythmainwindow.h>

// mytharchive
#include "mythburn.h"
#include "themeselector.h"

ThemeSelector::ThemeSelector(MythScreenStack *parent, MythScreenType *destinationScreen,
                             ArchiveDestination archiveDestination, QString name)
                : MythScreenType(parent, name, true)
{
    m_destinationScreen = destinationScreen;
    m_archiveDestination = archiveDestination;
    themeDir = GetShareDir() + "mytharchive/themes/";
}

ThemeSelector::~ThemeSelector(void)
{
    saveConfiguration();
}

bool ThemeSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "themeselector", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

        // theme preview images
    UIUtilE::Assign(this, intro_image, "intro_image", &err);
    UIUtilE::Assign(this, mainmenu_image, "mainmenu_image", &err);
    UIUtilE::Assign(this, chapter_image, "chapter_image", &err);
    UIUtilE::Assign(this, details_image, "details_image", &err);

        // menu theme
    UIUtilE::Assign(this, themedesc_text, "themedescription", &err);
    UIUtilE::Assign(this, theme_image, "theme_image", &err);
    UIUtilE::Assign(this, theme_selector, "theme_selector", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'themeselector'");
        return false;
    }

    m_nextButton->SetText(tr("Next"));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));

    m_prevButton->SetText(tr("Previous"));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));

    getThemeList();
    connect(theme_selector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(themeChanged(MythUIButtonListItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool ThemeSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    if (MythScreenType::keyPressEvent(event))
        return true;

    return false;
}

void ThemeSelector::handleNextPage()
{
    saveConfiguration();

    // show next page
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythBurn *burn = new MythBurn(mainStack, m_destinationScreen, this,
                                  m_archiveDestination, "MythBurn");

    if (burn->Create())
        mainStack->AddScreen(burn);
}

void ThemeSelector::handlePrevPage()
{
    Close();
}

void ThemeSelector::handleCancel()
{
    m_destinationScreen->Close();
    Close();
}

void ThemeSelector::getThemeList(void)
{
    theme_list.clear();
    QDir d;

    d.setPath(themeDir);
    if (d.exists())
    {
        QStringList filters;
        filters << "*";
        QFileInfoList list = d.entryInfoList(filters, QDir::Dirs, QDir::Name);

        int count = 0;
        for (int x = 0; x < list.size(); x++)
        {
            QFileInfo fi = list.at(x);
            if (QFile::exists(themeDir + fi.fileName() + "/preview.png"))
            {
                theme_list.append(fi.fileName());
                QString filename = fi.fileName().replace(QString("_"), QString(" "));
                new MythUIButtonListItem(theme_selector, filename);
                ++count;
            }
        }
    }
    else
        VERBOSE(VB_IMPORTANT, "MythArchive:  Theme directory does not exist!");
}

void ThemeSelector::themeChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int itemNo = theme_selector->GetCurrentPos();

    if (itemNo < 0 || itemNo > theme_list.size() - 1)
        itemNo = 0;

    theme_no = itemNo;

    if (QFile::exists(themeDir + theme_list[itemNo] + "/preview.png"))
        theme_image->SetFilename(themeDir + theme_list[itemNo] + "/preview.png");
    else
        theme_image->SetFilename("blank.png");
    theme_image->Load();

    if (QFile::exists(themeDir + theme_list[itemNo] + "/intro_preview.png"))
        intro_image->SetFilename(themeDir + theme_list[itemNo] + "/intro_preview.png");
    else
        intro_image->SetFilename("blank.png");
    intro_image->Load();

    if (QFile::exists(themeDir + theme_list[itemNo] + "/mainmenu_preview.png"))
        mainmenu_image->SetFilename(themeDir + theme_list[itemNo] + "/mainmenu_preview.png");
    else
        mainmenu_image->SetFilename("blank.png");
    mainmenu_image->Load();

    if (QFile::exists(themeDir + theme_list[itemNo] + "/chaptermenu_preview.png"))
        chapter_image->SetFilename(themeDir + theme_list[itemNo] + "/chaptermenu_preview.png");
    else
        chapter_image->SetFilename("blank.png");
    chapter_image->Load();

    if (QFile::exists(themeDir + theme_list[itemNo] + "/details_preview.png"))
        details_image->SetFilename(themeDir + theme_list[itemNo] + "/details_preview.png");
    else
        details_image->SetFilename("blank.png");
    details_image->Load();

    if (QFile::exists(themeDir + theme_list[itemNo] + "/description.txt"))
    {
        QString desc = loadFile(themeDir + theme_list[itemNo] + "/description.txt");
        themedesc_text->SetText(desc);
    }
    else
        themedesc_text->SetText("No description found!");
}

QString ThemeSelector::loadFile(const QString &filename)
{
    QString res = "";

    QFile file(filename);

    if (!file.exists())
        return "";

    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);

        while ( !stream.atEnd() )
        {
            res = res + stream.readLine();
        }
        file.close();
    }
    else
        return "";

    return res;
}

void ThemeSelector::loadConfiguration(void)
{
    QString theme = gContext->GetSetting("MythBurnMenuTheme", "");
    theme = theme.replace(QString("_"), QString(" "));
    theme_selector->MoveToNamedPosition(theme);
}

void ThemeSelector::saveConfiguration(void)
{
    QString theme = theme_selector->GetValue();
    theme = theme.replace(QString(" "), QString("_"));
    gContext->SaveSetting("MythBurnMenuTheme", theme);
}
