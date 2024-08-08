// c++
#include <cstdlib>
#include <unistd.h>

// qt
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextStream>
#include <QCoreApplication>

// myth
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdirs.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// mytharchive
#include "mythburn.h"
#include "themeselector.h"

DVDThemeSelector::DVDThemeSelector(
    MythScreenStack *parent, MythScreenType *destinationScreen,
    const ArchiveDestination& archiveDestination, const QString& name) :
    MythScreenType(parent, name, true),
    m_destinationScreen(destinationScreen),
    m_archiveDestination(archiveDestination),
    m_themeDir(GetShareDir() + "mytharchive/themes/")
{
}

DVDThemeSelector::~DVDThemeSelector(void)
{
    saveConfiguration();
}

bool DVDThemeSelector::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mythburn-ui.xml", "themeselector", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

        // theme preview images
    UIUtilE::Assign(this, m_introImage, "intro_image", &err);
    UIUtilE::Assign(this, m_mainmenuImage, "mainmenu_image", &err);
    UIUtilE::Assign(this, m_chapterImage, "chapter_image", &err);
    UIUtilE::Assign(this, m_detailsImage, "details_image", &err);

        // menu theme
    UIUtilE::Assign(this, m_themedescText, "themedescription", &err);
    UIUtilE::Assign(this, m_themeImage, "theme_image", &err);
    UIUtilE::Assign(this, m_themeSelector, "theme_selector", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'themeselector'");
        return false;
    }

    connect(m_nextButton, &MythUIButton::Clicked, this, &DVDThemeSelector::handleNextPage);
    connect(m_prevButton, &MythUIButton::Clicked, this, &DVDThemeSelector::handlePrevPage);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &DVDThemeSelector::handleCancel);

    getThemeList();

    connect(m_themeSelector, &MythUIButtonList::itemSelected,
            this, &DVDThemeSelector::themeChanged);

    BuildFocusList();

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool DVDThemeSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    if (MythScreenType::keyPressEvent(event))
        return true;

    return false;
}

void DVDThemeSelector::handleNextPage()
{
    saveConfiguration();

    // show next page
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *burn = new MythBurn(mainStack, m_destinationScreen, this,
                              m_archiveDestination, "MythBurn");

    if (burn->Create())
        mainStack->AddScreen(burn);
}

void DVDThemeSelector::handlePrevPage()
{
    Close();
}

void DVDThemeSelector::handleCancel()
{
    m_destinationScreen->Close();
    Close();
}

void DVDThemeSelector::getThemeList(void)
{
    m_themeList.clear();
    QDir d;

    d.setPath(m_themeDir);
    if (d.exists())
    {
        QStringList filters;
        filters << "*";
        QFileInfoList list = d.entryInfoList(filters, QDir::Dirs, QDir::Name);

        for (const auto & fi : std::as_const(list))
        {
            if (QFile::exists(m_themeDir + fi.fileName() + "/preview.png"))
            {
                m_themeList.append(fi.fileName());
                QString filename = fi.fileName().replace(QString("_"), QString(" "));
                new MythUIButtonListItem(m_themeSelector, filename);
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MythArchive:  Theme directory does not exist!");
    }
}

void DVDThemeSelector::themeChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int itemNo = m_themeSelector->GetCurrentPos();

    if (itemNo < 0 || itemNo > m_themeList.size() - 1)
        itemNo = 0;

    m_themeNo = itemNo;

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/preview.png"))
        m_themeImage->SetFilename(m_themeDir + m_themeList[itemNo] + "/preview.png");
    else
        m_themeImage->SetFilename("blank.png");
    m_themeImage->Load();

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/intro_preview.png"))
        m_introImage->SetFilename(m_themeDir + m_themeList[itemNo] + "/intro_preview.png");
    else
        m_introImage->SetFilename("blank.png");
    m_introImage->Load();

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/mainmenu_preview.png"))
        m_mainmenuImage->SetFilename(m_themeDir + m_themeList[itemNo] + "/mainmenu_preview.png");
    else
        m_mainmenuImage->SetFilename("blank.png");
    m_mainmenuImage->Load();

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/chaptermenu_preview.png"))
        m_chapterImage->SetFilename(m_themeDir + m_themeList[itemNo] + "/chaptermenu_preview.png");
    else
        m_chapterImage->SetFilename("blank.png");
    m_chapterImage->Load();

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/details_preview.png"))
        m_detailsImage->SetFilename(m_themeDir + m_themeList[itemNo] + "/details_preview.png");
    else
        m_detailsImage->SetFilename("blank.png");
    m_detailsImage->Load();

    if (QFile::exists(m_themeDir + m_themeList[itemNo] + "/description.txt"))
    {
        QString desc = loadFile(m_themeDir + m_themeList[itemNo] + "/description.txt");
        m_themedescText->SetText(QCoreApplication::translate("BurnThemeUI", 
                                desc.toUtf8().constData()));
    }
    else
    {
        m_themedescText->SetText(tr("No theme description file found!"));
    }
}

QString DVDThemeSelector::loadFile(const QString &filename)
{
    QString res = "";

    QFile file(filename);

    if (!file.exists())
    {
        res = tr("No theme description file found!");
    }
    else {
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);

            if (!stream.atEnd())
            {
                res = stream.readAll();
                res = res.replace("\n", " ").trimmed();
            }
            else {
                res = tr("Empty theme description!");
            }
            file.close();
        }
        else {
           res = tr("Unable to open theme description file!");
        }
    }

    return res;
}

void DVDThemeSelector::loadConfiguration(void)
{
    QString theme = gCoreContext->GetSetting("MythBurnMenuTheme", "");
    theme = theme.replace(QString("_"), QString(" "));
    m_themeSelector->MoveToNamedPosition(theme);
}

void DVDThemeSelector::saveConfiguration(void)
{
    QString theme = m_themeSelector->GetValue();
    theme = theme.replace(QString(" "), QString("_"));
    gCoreContext->SaveSetting("MythBurnMenuTheme", theme);
}
