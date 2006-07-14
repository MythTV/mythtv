#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <qapplication.h>
#include <qdir.h>
#include <qdom.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/util.h>

// mytharchive
#include "mytharchivewizard.h"
#include "mythburnwizard.h"
#include "mythnativewizard.h"
#include "fileselector.h"

// last page in wizard
const int LAST_PAGE = 1;

//Max size of a DVD-R in Mbytes
const int MAX_DVDR_SIZE_SL = 4489;
const int MAX_DVDR_SIZE_DL = 8978;

struct ArchiveFormat ArchiveFormats[] =
{
    {"Create DVD ISO Image",     "Create a DVD ISO image complete with themed menus. "
                                 "Optionally burn the image to a DVD."},
    {"Archive to native format", "Save MythTV recordings to a native archive format. "
                                 "Saves the video file and any associated metadata."},
};

int ArchiveFormatsCount = sizeof(ArchiveFormats) / sizeof(ArchiveFormats[0]);

struct ArchiveDestination ArchiveDestinations[] =
{
    {AD_DVD_SL,   "Single Layer DVD", "Single Layer DVD (4489Mb)", 4489*1024},
    {AD_DVD_DL,   "Dual Layer DVD",   "Dual Layer DVD (8979Mb)",   8979*1024},
    {AD_DVD_RW,   "DVD +/- RW",       "Rewritable DVD",            4489*1024},
    {AD_FILE,     "File",             "Any file accessable from "
                                      "your filesystem.",          -1},
};

int ArchiveDestinationsCount = sizeof(ArchiveDestinations) / sizeof(ArchiveDestinations[0]);

QString formatSize(long long sizeKB, int prec)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

MythArchiveWizard::MythArchiveWizard(MythMainWindow *parent, QString window_name,
                                 QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    setContext(1);
    wireUpTheme();
    assignFirstFocus();
    updateForeground();

    loadConfiguration();
}

MythArchiveWizard::~MythArchiveWizard(void)
{
    saveConfiguration();
}

void MythArchiveWizard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Archive", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            done(0);
        }
        else if (action == "DOWN")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEDOWN")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == format_selector)
                format_selector->push(false);
            else if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == format_selector)
                format_selector->push(true);
            else if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else if (action == "MENU")
        {
        }
        else if (action == "INFO")
        {
        }
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void MythArchiveWizard::wireUpTheme()
{
    // finish button
    next_button = getUITextButtonType("next_button");
    if (next_button)
    {
        next_button->setText(tr("Next"));
        connect(next_button, SIGNAL(pushed()), this, SLOT(handleNextPage()));
    }

    // prev button
    prev_button = getUITextButtonType("prev_button");
    if (prev_button)
    {
        prev_button->setText(tr("Previous"));
        prev_button->hide();
        connect(prev_button, SIGNAL(pushed()), this, SLOT(handlePrevPage()));
    }

    // cancel button
    cancel_button = getUITextButtonType("cancel_button");
    if (cancel_button)
    {
        cancel_button->setText(tr("Cancel"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(handleCancel()));
    }

    // format selector
    format_selector = getUISelectorType("format_selector");
    if (format_selector)
    {
        connect(format_selector, SIGNAL(pushed(int)),
                this, SLOT(setFormat(int)));

        for (int x = 0; x < ArchiveFormatsCount; x++)
            format_selector->addItem(x, ArchiveFormats[x].name);
    }

    format_text = getUITextType("format_text");

    // destination selector
    destination_selector = getUISelectorType("destination_selector");
    if (destination_selector)
    {
        connect(destination_selector, SIGNAL(pushed(int)),
                this, SLOT(setDestination(int)));

        for (int x = 0; x < ArchiveDestinationsCount; x++)
            destination_selector->addItem(ArchiveDestinations[x].type,
                                          ArchiveDestinations[x].name);
    }

    destination_text = getUITextType("destination_text");

    // optional destination items
    erase_check = getUICheckBoxType("erase_check");
    erase_text = getUITextType("erase_text");

    // find button
    find_button = getUITextButtonType("find_button");
    if (find_button)
    {
        find_button->setText(tr("Choose File..."));
        connect(find_button, SIGNAL(pushed()), this, SLOT(handleFind()));
    }

    filename_edit = getUIRemoteEditType("filename_edit");
    if (filename_edit)
    {
        filename_edit->createEdit(this);
        connect(filename_edit, SIGNAL(loosingFocus()), this, 
                SLOT(filenameEditLostFocus()));
    }

    freespace_text = getUITextType("freespace_text");

    // advanced button
    advanced_button = getUITextButtonType("advanced_button");
    if (advanced_button)
    {
        advanced_button->setText(tr("Advanced Options"));
        connect(advanced_button, SIGNAL(pushed()), this, SLOT(advancedPressed()));
    }

    setFormat(0);
    setDestination(0);
}

void MythArchiveWizard::filenameEditLostFocus()
{
    long long dummy;
    ArchiveDestinations[AD_FILE].freeSpace = 
            getDiskSpace(filename_edit->getText(), dummy, dummy);

    // if we don't get a valid freespace value it probably means the file doesn't
    // exist yet so try looking up the freespace for the parent directory 
    if (ArchiveDestinations[AD_FILE].freeSpace == -1)
    {
        QString dir = filename_edit->getText();
        int pos = dir.findRev('/');
        if (pos > 0)
            dir = dir.left(pos);
        else
            dir = "/";

        ArchiveDestinations[AD_FILE].freeSpace = getDiskSpace(dir, dummy, dummy);
    }

    if (ArchiveDestinations[AD_FILE].freeSpace != -1)
    {
        freespace_text->SetText(formatSize(ArchiveDestinations[AD_FILE].freeSpace));
    }
    else
    {
        freespace_text->SetText("Unknown");
    }
}

void MythArchiveWizard::handleNextPage()
{
    if (getContext() == LAST_PAGE)
    {
        if (format_no == 0)
            runMythBurnWizard();
        else
            runMythNativeWizard();
    }
    else
        setContext(getContext() + 1);

    updateForeground();
    buildFocusList();
}

void MythArchiveWizard::runMythBurnWizard()
{
    saveConfiguration();

    QString commandline;
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "")
        return;

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";

    // show the mythburn wizard
    MythburnWizard *wiz;

    wiz = new MythburnWizard(ArchiveDestinations[destination_no], 
                             gContext->GetMainWindow(),
                             "mythburn_wizard", "mytharchive-");

    if (destination_no == AD_FILE)
        wiz->setSaveFilename(filename_edit->getText());

    qApp->unlock();
    int res = wiz->exec();
    qApp->lock();
    qApp->processEvents();

    // reload config something may have changed
    loadConfiguration();

    if (res == 0)
    {
        delete wiz;
        return;
    }

    // remove existing progress.log if present
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    wiz->createConfigFile(configDir + "/mydata.xml");
    commandline = "python " + gContext->GetShareDir() + "mytharchive/scripts/mythburn.py";
    commandline += " -j " + configDir + "/mydata.xml";             // job file
    commandline += " -l " + logDir + "/progress.log";              // progress log
    commandline += " > "  + logDir + "/mythburn.log 2>&1 &";       //Logs

    delete wiz;

    int state = system(commandline);

    if (state != 0) 
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Myth Archive"),
                                  QObject::tr("It was not possible to create the DVD. "  
                                          " An error occured when running the scripts") );
        done(Rejected);
        return;
    }

    done(Accepted);
}

void MythArchiveWizard::runMythNativeWizard()
{
    saveConfiguration();

    QString commandline;
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "")
        return;

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";

    // show the mythburn wizard
    MythNativeWizard *wiz;

    wiz = new MythNativeWizard(ArchiveDestinations[destination_no],
                             gContext->GetMainWindow(),
                             "mythnative_wizard", "mytharchive-");

    if (destination_no == AD_FILE)
        wiz->setSaveFilename(filename_edit->getText());

    qApp->unlock();
    int res = wiz->exec();
    qApp->lock();
    qApp->processEvents();

    // reload config something may have changed
    loadConfiguration();

    if (res == 0)
    {
        delete wiz;
        return;
    }

    // remove existing progress.log if prescent
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    wiz->createConfigFile(configDir + "/mydata.xml");
    commandline = "mytharchivehelper -n " + configDir + "/mydata.xml";  // job file
//    commandline += " -l " + logDir + "/progress.log";                   // progress log
    commandline += " > "  + logDir + "/progress.log 2>&1 &";            // Logs
//    commandline += " > "  + logDir + "/mythburn.log 2>&1 &";            // Logs

    delete wiz;
    int state = system(commandline);

    if (state != 0) 
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Myth Archive"),
                                  QObject::tr("It was not possible to create the Archive. "  
                                          " An error occured when running 'mytharchivehelper'") );
        done(Rejected);
        return;
    }

    done(Accepted);
}

void MythArchiveWizard::handlePrevPage()
{
    if (getContext() > 1)
        setContext(getContext() - 1);

    if (next_button)
        next_button->setText(tr("Next"));

    updateForeground();
    buildFocusList();
}

void MythArchiveWizard::handleCancel()
{
    done(Rejected);
}

void MythArchiveWizard::setFormat(int item)
{
    if (item < 0 || item > ArchiveFormatsCount - 1)
        item = 0;

    format_no = item;
    if (format_text)
    {
        format_text->SetText(ArchiveFormats[item].description);
    }
}

void MythArchiveWizard::setDestination(int item)
{
    if (item < 0 || item > ArchiveDestinationsCount - 1)
        item = 0;

    destination_no = item;
    if (destination_text)
    {
        destination_text->SetText(ArchiveDestinations[item].description);
    }

    switch(item)
    {
        case AD_DVD_SL:
        case AD_DVD_DL:
            filename_edit->hide();
            find_button->hide();
            erase_check->hide();
            erase_text->hide();
            break;
        case AD_DVD_RW:
            filename_edit->hide();
            find_button->hide();
            erase_check->show();
            erase_text->show();
            break;
        case AD_FILE:
            long long dummy;
            ArchiveDestinations[item].freeSpace = 
                    getDiskSpace(filename_edit->getText(), dummy, dummy);

            filename_edit->show();
            find_button->show();
            erase_check->hide();
            erase_text->hide();
            break;
    }

    filename_edit->refresh();
    erase_check->refresh();
    erase_text->refresh();
    find_button->refresh();

    // update free space
    if (ArchiveDestinations[item].freeSpace != -1)
    {
        freespace_text->SetText(formatSize(ArchiveDestinations[item].freeSpace));
    }
    else
    {
        freespace_text->SetText("Unknown");
    }

    buildFocusList();
}

void MythArchiveWizard::loadConfiguration(void)
{
    bool bEraseDvdRw = (gContext->GetSetting("MythArchiveEraseDvdRw", "0") == "1");
    erase_check->setState(bEraseDvdRw);

    format_no = gContext->GetNumSetting("MythArchiveFormatType", 0);
    format_selector->setToItem(ArchiveFormats[format_no].name);
    setFormat(format_no);

    destination_no = gContext->GetNumSetting("MythArchiveMediaType", 0);
    destination_selector->setToItem(ArchiveDestinations[destination_no].name);
    setDestination(destination_no);

    filename_edit->setText(gContext->GetSetting("MythArchiveSaveFilename", ""));
}

void MythArchiveWizard::saveConfiguration(void)
{
    gContext->SaveSetting("MythArchiveMediaType", destination_no);
    gContext->SaveSetting("MythArchiveFormatType", format_no);
    gContext->SaveSetting("MythArchiveSaveFilename", filename_edit->getText());
    gContext->SaveSetting("MythArchiveEraseDvdRw", (erase_check->getState() ? "1" : "0"));
}

void MythArchiveWizard::handleFind(void)
{
    FileSelector selector(FSTYPE_FILE, "/", "*.*", gContext->GetMainWindow(),
                          "file_selector", "mytharchive-", "file selector");
    qApp->unlock();
    bool res = selector.exec();

    if (res)
    {
        filename_edit->setText(selector.getSelected());
        filenameEditLostFocus();
    }
    qApp->lock();
}

void MythArchiveWizard::advancedPressed()
{
    AdvancedOptions *dialog = new AdvancedOptions(gContext->GetMainWindow(),
            "advanced_options", "mytharchive-", "advanced options");
    int res = dialog->exec();
    delete dialog;

    if (res)
    {
        // need to reload our copy of setting in case they have changed
        loadConfiguration();
    }
}
