#include <QImageReader>
#include <QApplication>

#include <mythcontext.h>
#include <mythdirs.h>

#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythuibuttonlist.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythuifilebrowser.h>

#include "romedit.h"
#include "rominfo.h"

EditRomInfoDialog::EditRomInfoDialog(MythScreenStack *parent,
        QString name, RomInfo *romInfo) : MythScreenType(parent, name),
    m_id(""), m_retObject(0), m_gamenameEdit(0), m_genreEdit(0),
    m_yearEdit(0), m_countryEdit(0), m_plotEdit(0), m_publisherEdit(0), 
    m_favoriteCheck(0), m_screenshotButton(0), m_fanartButton(0), 
    m_fanartText(0), m_boxartButton(0), m_boxartText(0), m_doneButton(0)
{
    m_workingRomInfo = new RomInfo(*romInfo);
}

EditRomInfoDialog::~EditRomInfoDialog()
{
    delete m_workingRomInfo;
}

bool EditRomInfoDialog::Create()
{
    if (!LoadWindowFromXML("game-ui.xml", "edit_metadata", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_gamenameEdit, "title_edit", &err);
    UIUtilE::Assign(this, m_genreEdit, "genre_edit", &err);
    UIUtilE::Assign(this, m_yearEdit, "year_edit", &err);
    UIUtilE::Assign(this, m_countryEdit, "country_edit", &err);
    UIUtilE::Assign(this, m_plotEdit, "description_edit", &err);
    UIUtilE::Assign(this, m_publisherEdit, "publisher_edit", &err);

    UIUtilE::Assign(this, m_favoriteCheck, "favorite_check", &err);

    UIUtilE::Assign(this, m_screenshotButton, "screenshot_button", &err);
    UIUtilE::Assign(this, m_screenshotText, "screenshot_text", &err);
    UIUtilE::Assign(this, m_fanartButton, "fanart_button", &err);
    UIUtilE::Assign(this, m_fanartText, "fanart_text", &err);
    UIUtilE::Assign(this, m_boxartButton, "coverart_button", &err);
    UIUtilE::Assign(this, m_boxartText, "coverart_text", &err);

    UIUtilE::Assign(this, m_doneButton, "done_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'edit_metadata'");
        return false;
    }

    fillWidgets();

    BuildFocusList();

    connect(m_gamenameEdit, SIGNAL(valueChanged()), SLOT(SetGamename()));
    connect(m_genreEdit, SIGNAL(valueChanged()), SLOT(SetGenre()));
    connect(m_yearEdit, SIGNAL(valueChanged()), SLOT(SetYear()));
    connect(m_countryEdit, SIGNAL(valueChanged()), SLOT(SetCountry()));
    connect(m_plotEdit, SIGNAL(valueChanged()), SLOT(SetPlot()));
    connect(m_publisherEdit, SIGNAL(valueChanged()), SLOT(SetPublisher()));

    connect(m_favoriteCheck, SIGNAL(valueChanged()), SLOT(ToggleFavorite()));

    connect(m_screenshotButton, SIGNAL(Clicked()), SLOT(FindScreenshot()));
    connect(m_fanartButton, SIGNAL(Clicked()), SLOT(FindFanart()));
    connect(m_boxartButton, SIGNAL(Clicked()), SLOT(FindBoxart()));

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(SaveAndExit()));
 
    return true;
}

namespace
{
    QStringList GetSupportedImageExtensionFilter()
    {
        QStringList ret;

        QList<QByteArray> exts = QImageReader::supportedImageFormats();
        for (QList<QByteArray>::iterator p = exts.begin(); p != exts.end(); ++p)
        {
            ret.append(QString("*.").append(*p));
        }

        return ret;
    }

    void FindImagePopup(const QString &prefix, const QString &prefixAlt,
            QObject &inst, const QString &returnEvent)
    {
        QString fp = prefix.isEmpty() ? prefixAlt : prefix;

        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, fp);
        fb->SetNameFilter(GetSupportedImageExtensionFilter());
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
            delete fb;
    }

    const QString CEID_SCREENSHOTFILE = "screenshotfile";
    const QString CEID_FANARTFILE = "fanartfile";
    const QString CEID_BOXARTFILE = "boxartfile";
}

void EditRomInfoDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);
        const QString resultid = dce->GetId();

        if (resultid == CEID_FANARTFILE)
            SetFanart(dce->GetResultText());
        else if (resultid == CEID_SCREENSHOTFILE)
            SetScreenshot(dce->GetResultText());
        else if (resultid == CEID_BOXARTFILE)
            SetBoxart(dce->GetResultText());
    }
}

void EditRomInfoDialog::fillWidgets()
{
    m_gamenameEdit->SetText(m_workingRomInfo->Gamename());
    m_genreEdit->SetText(m_workingRomInfo->Genre());
    m_yearEdit->SetText(m_workingRomInfo->Year());
    m_countryEdit->SetText(m_workingRomInfo->Country());
    m_plotEdit->SetText(m_workingRomInfo->Plot());
    m_publisherEdit->SetText(m_workingRomInfo->Publisher());

    if (m_workingRomInfo->Favorite())
        m_favoriteCheck->SetCheckState(MythUIStateType::Full);
 
    m_screenshotText->SetText(m_workingRomInfo->Screenshot());
    m_fanartText->SetText(m_workingRomInfo->Fanart());
    m_boxartText->SetText(m_workingRomInfo->Boxart());
}

void EditRomInfoDialog::SetReturnEvent(QObject *retobject,
                                       const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void EditRomInfoDialog::SaveAndExit()
{
    if (m_retObject)
    {
        RomInfo *romInfo = new RomInfo(*m_workingRomInfo);
        DialogCompletionEvent *dce =
            new DialogCompletionEvent(m_id, 0, "",
                                      qVariantFromValue(romInfo));

        QApplication::postEvent(m_retObject, dce);
    }
    Close();
}

void EditRomInfoDialog::SetGamename()
{
    m_workingRomInfo->setGamename(m_gamenameEdit->GetText());
}

void EditRomInfoDialog::SetGenre()
{
    m_workingRomInfo->setGenre(m_genreEdit->GetText());
}

void EditRomInfoDialog::SetYear()
{
    m_workingRomInfo->setYear(m_yearEdit->GetText());
}

void EditRomInfoDialog::SetCountry()
{
    m_workingRomInfo->setCountry(m_countryEdit->GetText());
}

void EditRomInfoDialog::SetPlot()
{
    m_workingRomInfo->setPlot(m_plotEdit->GetText());
}

void EditRomInfoDialog::SetPublisher()
{
    m_workingRomInfo->setPublisher(m_publisherEdit->GetText());
}

void EditRomInfoDialog::ToggleFavorite()
{
    m_workingRomInfo->setFavorite();
}

void EditRomInfoDialog::FindScreenshot()
{
    FindImagePopup(gCoreContext->GetSetting("mythgame.screenshotDir"),
            GetConfDir() + "/MythGame/Screenshots",
            *this, CEID_SCREENSHOTFILE);
}

void EditRomInfoDialog::FindFanart()
{
    FindImagePopup(gCoreContext->GetSetting("mythgame.fanartDir"),
            GetConfDir() + "/MythGame/Fanart",
            *this, CEID_FANARTFILE);
}

void EditRomInfoDialog::FindBoxart()
{
    FindImagePopup(gCoreContext->GetSetting("mythgame.boxartDir"),
            GetConfDir() + "/MythGame/Boxart",
            *this, CEID_BOXARTFILE);
}

void EditRomInfoDialog::SetScreenshot(QString file)
{
    if (file.isEmpty())
        return;

    m_workingRomInfo->setScreenshot(file);
    m_screenshotText->SetText(file);
}

void EditRomInfoDialog::SetFanart(QString file)
{
    if (file.isEmpty())
        return;

    m_workingRomInfo->setFanart(file);
    m_fanartText->SetText(file);
}

void EditRomInfoDialog::SetBoxart(QString file)
{
    if (file.isEmpty())
        return;

    m_workingRomInfo->setBoxart(file);
    m_boxartText->SetText(file);
}
