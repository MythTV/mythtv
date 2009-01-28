
#include <QApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "mythverbose.h"

#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuiutils.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuistatetype.h"

MythDialogBox::MythDialogBox(const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen)
         : MythScreenType(parent, name, false)
{
    m_id = "";
    m_retObject = NULL;
    m_titlearea = NULL;
    m_title = "";
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_useSlots = false;
}

MythDialogBox::MythDialogBox(const QString &title, const QString &text,
                             MythScreenStack *parent, const char *name,
                             bool fullscreen)
         : MythScreenType(parent, name, false)
{
    m_id = "";
    m_retObject = NULL;
    m_title = title;
    m_titlearea = NULL;
    m_text = text;
    m_textarea = NULL;
    m_buttonList = NULL;

    m_fullscreen = fullscreen;
    m_useSlots = false;
}

bool MythDialogBox::Create(void)
{
    QString windowName = (m_fullscreen ? "MythDialogBox" : "MythPopupBox");

    if (!CopyWindowFromBase(windowName, this))
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_titlearea, "title");
    UIUtilE::Assign(this, m_textarea, "messagearea", &err);
    UIUtilE::Assign(this, m_buttonList, "list", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen '" + windowName + "'");
        return false;
    }

    if (m_titlearea)
        m_titlearea->SetText(m_title);
    m_textarea->SetText(m_text);
    m_buttonList->SetActive(true);

    connect(m_buttonList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(Select(MythUIButtonListItem*)));

    return true;
}

void MythDialogBox::Select(MythUIButtonListItem* item)
{
    const char *slot = qVariantValue<const char *>(item->GetData());
    if (m_useSlots && slot)
    {
        connect(this, SIGNAL(Selected()), m_retObject, slot,
                Qt::QueuedConnection);
        emit Selected();
    }

    SendEvent(m_buttonList->GetItemPos(item), item->GetText(), item->GetData());
    m_ScreenStack->PopScreen(false);
}

void MythDialogBox::SetReturnEvent(QObject *retobject,
                               const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title, QVariant data, bool newMenu,
                              bool setCurrent)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);
    button->SetData(data);
    button->setDrawArrow(newMenu);

    if (setCurrent)
        m_buttonList->SetItemCurrent(button);
}

void MythDialogBox::AddButton(const QString &title, const char *slot,
                              bool newMenu, bool setCurrent)
{
    MythUIButtonListItem *button = new MythUIButtonListItem(m_buttonList, title);

    m_useSlots = true;

    if (slot)
        button->SetData(qVariantFromValue(slot));
    button->setDrawArrow(newMenu);

    if (setCurrent)
        m_buttonList->SetItemCurrent(button);
}

bool MythDialogBox::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", event, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE" || action == "LEFT" || action == "MENU")
            {
                SendEvent(-1);
                m_ScreenStack->PopScreen();
            }
            else if (action == "RIGHT")
            {
                MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
                Select(item);
            }
            else
                handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythDialogBox::SendEvent(int res, QString text, QVariant data)
{
    if (!m_retObject)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, text, data);
    QApplication::postEvent(m_retObject, dce);
}

/////////////////////////////////////////////////////////////////

MythConfirmationDialog::MythConfirmationDialog(MythScreenStack *parent,
                                               const QString &message,
                                               bool showCancel)
                       : MythScreenType(parent, "mythconfirmpopup")
{
    m_message = message;
    m_showCancel = showCancel;

    m_id = "";
    m_retObject = NULL;
}

bool MythConfirmationDialog::Create(void)
{
    if (!CopyWindowFromBase("MythConfirmationDialog", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilE::Assign(this, cancelButton, "cancel", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythConfirmationDialog'");
        return false;
    }

    if (m_showCancel)
    {
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Cancel()));
    }
    else
        cancelButton->SetVisible(false);

    connect(okButton, SIGNAL(Clicked()), SLOT(Confirm()));

    messageText->SetText(m_message);

    BuildFocusList();

    SetFocusWidget(okButton);

    return true;
}

bool MythConfirmationDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", event, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                sendResult(false);
            else
                handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythConfirmationDialog::SetReturnEvent(QObject *retobject,
                                            const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythConfirmationDialog::Confirm()
{
    sendResult(true);
}

void MythConfirmationDialog::Cancel()
{
    sendResult(false);
}

void MythConfirmationDialog::sendResult(bool ok)
{
    emit haveResult(ok);

    if (m_retObject)
    {
        int res = 0;
        if (ok)
            res = 1;

        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res, "",
                                                               m_resultData);
        QApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/**
 * Non-blocking version of MythPopupBox::showOkPopup()
 */
MythConfirmationDialog  *ShowOkPopup(const QString &message, QObject *parent,
                                     const char *slot, bool showCancel)
{
    QString                  LOC = "ShowOkPopup('" + message + "') - ";
    MythConfirmationDialog  *pop;
    static MythScreenStack  *stk = NULL;


    if (!stk)
    {
        MythMainWindow *win = GetMythMainWindow();

        if (win)
            stk = win->GetStack("popup stack");
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + "no main window?");
            return NULL;
        }

        if (!stk)
        {
            VERBOSE(VB_IMPORTANT, LOC + "no popup stack?\n"
                                        "Is there a MythThemeBase?");
            return NULL;
        }
    }

    pop = new MythConfirmationDialog(stk, message, showCancel);
    if (pop->Create())
    {
        stk->AddScreen(pop);
        if (parent && slot)
            QObject::connect(pop, SIGNAL(haveResult(bool)), parent, slot, Qt::QueuedConnection);
    }
    else
    {
        delete pop;
        pop = NULL;
        VERBOSE(VB_IMPORTANT, LOC + "Couldn't Create() Dialog");
    }

    return pop;
}

/////////////////////////////////////////////////////////////////

MythTextInputDialog::MythTextInputDialog(MythScreenStack *parent,
                                         const QString &message,
                                         InputFilter filter,
                                         bool isPassword,
                                         const QString &defaultValue)
           : MythScreenType(parent, "mythtextinputpopup")
{
    m_filter = filter;
    m_isPassword = isPassword;
    m_message = message;
    m_defaultValue = defaultValue;
    m_textEdit = NULL;

    m_id = "";
    m_retObject = NULL;
}

bool MythTextInputDialog::Create(void)
{
    if (!CopyWindowFromBase("MythTextInputDialog", this))
        return false;

    MythUIText *messageText = NULL;
    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_textEdit, "input", &err);
    UIUtilE::Assign(this, messageText, "message", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythTextInputDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(okButton, SIGNAL(Clicked()), SLOT(sendResult()));

    m_textEdit->SetFilter(m_filter);
    m_textEdit->SetText(m_defaultValue);
    m_textEdit->SetPassword(m_isPassword);

    messageText->SetText(m_message);

    BuildFocusList();

    return true;
}

void MythTextInputDialog::SetReturnEvent(QObject *retobject,
                                         const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythTextInputDialog::sendResult()
{
    QString inputString = m_textEdit->GetText();
    emit haveResult(inputString);

    if (m_retObject)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            inputString, "");
        QApplication::postEvent(m_retObject, dce);
    }

    Close();
}

/////////////////////////////////////////////////////////////////////


/** \fn MythUISearchDialog::MythUISearchDialog(MythScreenStack*,
                                   const QString&,
                                   const QStringList&,
                                   bool  matchAnywhere,
                                   const QString&)
 *  \brief the classes constructor
 *  \param parent the MythScreenStack this widget belongs to
 *  \param title  the text to show as the title
 *  \param list   the list of text strings to search
 *  \param matchAnywhere if true will match the input text anywhere in the string.
                         if false will match only strings that start with the input text.
                         Default is false.
 *  \param defaultValue  The initial value for the input text. Default is ""
 */
MythUISearchDialog::MythUISearchDialog(MythScreenStack *parent,
                                   const QString &title,
                                   const QStringList &list,
                                   bool  matchAnywhere,
                                   const QString &defaultValue)
                : MythScreenType(parent, "mythsearchdialogpopup")
{
    m_list = list;
    m_matchAnywhere = matchAnywhere;
    m_title = title;
    m_defaultValue = defaultValue;

    m_titleText = NULL;
    m_matchesText = NULL;
    m_textEdit = NULL;
    m_itemList = NULL;

    m_id = "";
    m_retObject = NULL;
}

bool MythUISearchDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSearchDialog", this))
        return false;

    MythUIButton *okButton = NULL;
    MythUIButton *cancelButton = NULL;

    bool err = false;
    UIUtilE::Assign(this, m_textEdit, "input", &err);
    UIUtilE::Assign(this, m_titleText, "title", &err);
    UIUtilW::Assign(this, m_matchesText, "matches");
    UIUtilE::Assign(this, m_itemList, "itemlist", &err);
    UIUtilE::Assign(this, okButton, "ok", &err);
    UIUtilW::Assign(this, cancelButton, "cancel");

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythSearchDialog'");
        return false;
    }

    if (cancelButton)
        connect(cancelButton, SIGNAL(Clicked()), SLOT(Close()));

    connect(okButton, SIGNAL(Clicked()), SLOT(slotSendResult()));

    connect(m_itemList, SIGNAL(itemClicked(MythUIButtonListItem*)), SLOT(slotSendResult()));

    m_textEdit->SetText(m_defaultValue);
    connect(m_textEdit, SIGNAL(valueChanged()), SLOT(slotUpdateList()));

    m_titleText->SetText(m_title);
    if (m_matchesText)
        m_matchesText->SetText(tr("0 matches"));

    BuildFocusList();

    slotUpdateList();

    return true;
}

void MythUISearchDialog::SetReturnEvent(QObject *retobject,
                                        const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythUISearchDialog::slotSendResult()
{
    if (!m_itemList->GetItemCurrent())
        return;

    QString result = m_itemList->GetValue();

    emit haveResult(result);

    if (m_retObject)
    {
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                            result, "");
        QApplication::postEvent(m_retObject, dce);
    }

    Close();
}

void MythUISearchDialog::slotUpdateList(void)
{
    m_itemList->Reset();

    for (int x = 0; x < m_list.size(); x++)
    {
        QString item = m_list.at(x);

        if (m_matchAnywhere)
        {
            if (!item.contains(m_textEdit->GetText(), Qt::CaseInsensitive))
                continue;
        }
        else
        {
            if (!item.startsWith(m_textEdit->GetText(), Qt::CaseInsensitive))
                continue;
        }

        // add item to list
        new MythUIButtonListItem(m_itemList, item, NULL, false);
    }

    m_itemList->SetItemCurrent(0);

    if (m_matchesText)
        m_matchesText->SetText(tr("%1 matches").arg(m_list.size()));
}

/////////////////////////////////////////////////////////////////////

/** \fn MythUIFileBrowser::MythUIFileBrowser(MythScreenStack*,
                                   const QString&)
 *  \brief Browse the filesystem at a given starting path for a directory or
 *         file, returns the selected file. Includes previews of images and
 *         file metadata.
 *
 *         Accepts filtering on name (*.png, *.pl) and by type (directory, file)
 */

MythUIFileBrowser::MythUIFileBrowser(MythScreenStack *parent,
                                  QObject *retobject, const QString &startPath)
                :MythScreenType(parent, "mythuifilebrowser")
{
    m_retObject = retobject;

    m_curDirectory = startPath;
    m_typeFilter = (QDir::AllDirs | QDir::Drives | QDir::Files | QDir::Readable
                    | QDir::Writable | QDir::Executable);
    m_nameFilter << "*";

    m_previewTimer = new QTimer();
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, SIGNAL(timeout()), SLOT(LoadPreview()));
}

MythUIFileBrowser::~MythUIFileBrowser()
{
    if (m_previewTimer)
        delete m_previewTimer;
}

bool MythUIFileBrowser::Create()
{
    if (!CopyWindowFromBase("MythFileBrowser", this))
        return false;

    m_fileList = dynamic_cast<MythUIButtonList *>(GetChild("filelist"));
    m_locationEdit = dynamic_cast<MythUITextEdit *>(GetChild("location"));
    m_okButton = dynamic_cast<MythUIButton *>(GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));
    m_backButton = dynamic_cast<MythUIButton *>(GetChild("back"));
    m_homeButton = dynamic_cast<MythUIButton *>(GetChild("home"));
    m_previewImage = dynamic_cast<MythUIImage *>(GetChild("preview"));
    m_infoText = dynamic_cast<MythUIText *>(GetChild("info"));
    m_filenameText = dynamic_cast<MythUIText *>(GetChild("filename"));
    m_fullpathText = dynamic_cast<MythUIText *>(GetChild("fullpath"));

    if (!m_fileList || !m_locationEdit || !m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "MythUIFileBrowser: Your theme is missing"
                              " some UI elements! Bailing out.");
        return false;
    }

    connect(m_fileList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(PathClicked(MythUIButtonListItem *)));
    connect(m_fileList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(PathSelected(MythUIButtonListItem *)));
    connect(m_locationEdit, SIGNAL(LosingFocus()), SLOT(editLostFocus()));
    connect(m_okButton, SIGNAL(Clicked()), SLOT(OKPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(cancelPressed()));
    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(backPressed()));
    if (m_homeButton)
        connect(m_homeButton, SIGNAL(Clicked()), SLOT(homePressed()));

    BuildFocusList();
    updateFileList();

    return true;
}

void MythUIFileBrowser::LoadPreview()
{
    if (m_previewImage)
        m_previewImage->Load();
}

void MythUIFileBrowser::PathSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_previewImage)
        m_previewImage->Reset();

    QFileInfo file = qVariantValue<QFileInfo>(item->GetData());
    if (file.fileName() == "..")
    {
        if (m_infoText)
            m_infoText->Reset();
        if (m_infoText)
            m_infoText->Reset();
        if (m_filenameText)
            m_filenameText->Reset();
        if (m_fullpathText)
            m_fullpathText->Reset();
    }
    else
    {
        if (IsImage(file.suffix()) && m_previewImage)
        {
            m_previewImage->SetFilename(file.absoluteFilePath());
            m_previewTimer->start(250);
        }

        if (m_infoText)
            m_infoText->SetText(FormatSize(file.size()));

        if (m_filenameText)
            m_filenameText->SetText(file.fileName());

        if (m_fullpathText)
            m_fullpathText->SetText(file.fileName());
    }
}

void MythUIFileBrowser::PathClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    QFileInfo file = qVariantValue<QFileInfo>(item->GetData());

    if (file.isFile())
    {
        OKPressed();
        return;
    }

    if (!file.isDir())
        return;

    if (file.fileName() == "..")
    {
        backPressed();
    }
    else
    {
        if (!m_curDirectory.endsWith("/"))
            m_curDirectory += "/";
        m_curDirectory += file.fileName();
    }

    updateFileList();
}

bool MythUIFileBrowser::IsImage(QString extension)
{
    if (extension.isEmpty())
        return false;

    extension = extension.toLower();

    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    if (formats.contains(extension.toAscii()))
        return true;

    return false;
}

void MythUIFileBrowser::editLostFocus()
{
    m_curDirectory = m_locationEdit->GetText();
    updateFileList();
}

void MythUIFileBrowser::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.lastIndexOf('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void MythUIFileBrowser::homePressed()
{
    char *home = getenv("HOME");
    m_curDirectory = home;

    updateFileList();
}

void MythUIFileBrowser::OKPressed()
{
    MythUIButtonListItem *item = m_fileList->GetItemCurrent();
    QFileInfo file = qVariantValue<QFileInfo>(item->GetData());

    if (file.isFile())
    {
        if (!m_curDirectory.endsWith("/"))
            m_curDirectory += "/";
        m_curDirectory += file.fileName();
    }

    DialogCompletionEvent *dce = new DialogCompletionEvent("filebrowser",
                                                            0, m_curDirectory,
                                                            item->GetData());
    QApplication::postEvent(m_retObject, dce);

    Close();
}

void MythUIFileBrowser::cancelPressed()
{
    Close();
}

void MythUIFileBrowser::updateFileList()
{
    m_fileList->Reset();

    QDir d;

    d.setPath(m_curDirectory);
    d.setNameFilters(m_nameFilter);
    d.setFilter(m_typeFilter);
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    if (!d.exists())
    {
        VERBOSE(VB_IMPORTANT,
                "MythUIFileBrowser: current directory does not exist!");
        m_locationEdit->SetText("/");
        m_curDirectory = "/";
        d.setPath("/");
    }

    QFileInfoList list = d.entryInfoList();

    if (list.isEmpty())
    {
        MythUIButtonListItem* item = new MythUIButtonListItem(m_fileList,
                                                        tr("Parent Directory"));
        item->DisplayState("upfolder", "nodetype");
    }
    else
    {
        QFileInfoList::const_iterator it = list.begin();
        const QFileInfo *fi;
        while (it != list.end())
        {
            fi = &(*it);
            if (fi->fileName() != ".")
            {
                QString displayName = fi->fileName();
                QString type;
                if (displayName == "..")
                {
                    displayName = tr("Parent");
                    type = "upfolder";
                }
                else if (fi->isDir())
                {
                    type = "folder";
                }
                else if (fi->isExecutable())
                {
                    type = "executable";
                }
                else if (fi->isFile())
                {
                    type = "file";
                }

                MythUIButtonListItem* item = new MythUIButtonListItem(
                                                    m_fileList, displayName);

                if (IsImage(fi->suffix()))
                {
                    item->SetImage(fi->absoluteFilePath());
                    type = "image";
                }

                item->SetText(FormatSize(fi->size()), "filesize");
                item->SetText(fi->absoluteFilePath(), "fullpath");
                item->DisplayState(type, "nodetype");
                item->SetData(qVariantFromValue(*fi));
            }
            ++it;
        }
    }

    m_locationEdit->SetText(m_curDirectory);
}

QString MythUIFileBrowser::FormatSize(int size)
{
    QString filesize("%L1 %2");

    if (size < 1000000)
        filesize = filesize.arg((double)(size / 1000)).arg("KB");
    else if (size < 1000000000)
        filesize = filesize.arg((double)(size / 1000000)).arg("MB");
    else
        filesize = filesize.arg((double)(size / 1000000000)).arg("GB");

    return filesize;
}

