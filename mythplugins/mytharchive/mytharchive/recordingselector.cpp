
// c
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>

// qt
#include <QDir>
#include <QKeyEvent>
#include <QTimer>
#include <QApplication>

// mythtv
#include <mythcontext.h>
#include <mythdb.h>
#include <mthread.h>
#include <programinfo.h>
#include <remoteutil.h>
#include <mythtimer.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuiimage.h>
#include <mythuibuttonlist.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>
#include <mythdialogbox.h>
#include <mythlogging.h>
#include <mythdate.h>

// mytharchive
#include "recordingselector.h"
#include "archiveutil.h"

class GetRecordingListThread : public MThread
{
  public:
    GetRecordingListThread(RecordingSelector *parent) :
        MThread("GetRecordingList"), m_parent(parent)
    {
        start();
    }

    virtual void run(void)
    {
        RunProlog();
        m_parent->getRecordingList();
        RunEpilog();
    }

    RecordingSelector *m_parent;
};

RecordingSelector::RecordingSelector(
    MythScreenStack *parent, QList<ArchiveItem *> *archiveList) :
    MythScreenType(parent, "RecordingSelector"),
    m_archiveList(archiveList),
    m_recordingList(NULL),
    m_recordingButtonList(NULL),
    m_okButton(NULL),
    m_cancelButton(NULL),
    m_categorySelector(NULL),
    m_titleText(NULL),
    m_datetimeText(NULL),
    m_filesizeText(NULL),
    m_descriptionText(NULL),
    m_previewImage(NULL),
    m_cutlistImage(NULL)
{
}

RecordingSelector::~RecordingSelector(void)
{
    if (m_recordingList)
        delete m_recordingList;

    while (!m_selectedList.isEmpty())
        delete m_selectedList.takeFirst();
}

bool RecordingSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "recording_selector", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_categorySelector, "category_selector", &err);
    UIUtilE::Assign(this, m_recordingButtonList, "recordinglist", &err);

    UIUtilW::Assign(this, m_titleText, "progtitle", &err);
    UIUtilW::Assign(this, m_datetimeText, "progdatetime", &err);
    UIUtilW::Assign(this, m_descriptionText, "progdescription", &err);
    UIUtilW::Assign(this, m_filesizeText, "filesize", &err);
    UIUtilW::Assign(this, m_previewImage, "preview_image", &err);
    UIUtilW::Assign(this, m_cutlistImage, "cutlist_image", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'recording_selector'");
        return false;
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(OKPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    new MythUIButtonListItem(m_categorySelector, tr("All Recordings"));
    connect(m_categorySelector, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(setCategory(MythUIButtonListItem *)));

    connect(m_recordingButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(titleChanged(MythUIButtonListItem *)));
    connect(m_recordingButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(toggleSelected(MythUIButtonListItem *)));

    if (m_cutlistImage)
        m_cutlistImage->Hide();

    BuildFocusList();

    SetFocusWidget(m_recordingButtonList);

    return true;
}

void RecordingSelector::Init(void)
{
    QString message = tr("Retrieving Recording List.\nPlease Wait...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new
            MythUIBusyDialog(message, popupStack, "recordingselectorbusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);
    else
    {
        delete busyPopup;
        busyPopup = NULL;
    }

    GetRecordingListThread *thread = new GetRecordingListThread(this);
    while (thread->isRunning())
    {
        qApp->processEvents();
        usleep(2000);
    }

    if (!m_recordingList || m_recordingList->empty())
    {
        ShowOkPopup(tr("Either you don't have any recordings or "
                       "no recordings are available locally!"));
        if (busyPopup)
            busyPopup->Close();

        Close();
        return;
    }

    updateCategorySelector();
    updateSelectedList();
    updateRecordingList();

    if (busyPopup)
        busyPopup->Close();
}

bool RecordingSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RecordingSelector::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Clear All"), SLOT(clearAll()));
    menuPopup->AddButton(tr("Select All"), SLOT(selectAll()));
}

void RecordingSelector::selectAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    ProgramInfo *p;
    vector<ProgramInfo *>::iterator i = m_recordingList->begin();
    for ( ; i != m_recordingList->end(); ++i)
    {
        p = *i;
        m_selectedList.append(p);
    }

    updateRecordingList();
}

void RecordingSelector::clearAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    updateRecordingList();
}

void RecordingSelector::toggleSelected(MythUIButtonListItem *item)
{
    if (item->state() == MythUIButtonListItem:: FullChecked)
    {
        int index = m_selectedList.indexOf(
                                qVariantValue<ProgramInfo *>(item->GetData()));
        if (index != -1)
            m_selectedList.takeAt(index);
        item->setChecked(MythUIButtonListItem:: NotChecked);
    }
    else
    {
        int index = m_selectedList.indexOf(
                                qVariantValue<ProgramInfo *>(item->GetData()));
        if (index == -1)
            m_selectedList.append(qVariantValue<ProgramInfo *>(item->GetData()));

        item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void RecordingSelector::titleChanged(MythUIButtonListItem *item)
{
    ProgramInfo *p;

    p = qVariantValue<ProgramInfo *>(item->GetData());

    if (!p)
        return;

    if (m_titleText)
        m_titleText->SetText(p->GetTitle());

    if (m_datetimeText)
        m_datetimeText->SetText(p->GetScheduledStartTime().toLocalTime()
                                .toString("dd MMM yy (hh:mm)"));

    if (m_descriptionText)
    {
        m_descriptionText->SetText(
            ((!p->GetSubtitle().isEmpty()) ? p->GetSubtitle() + "\n" : "") +
            p->GetDescription());
    }

    if (m_filesizeText)
    {
        m_filesizeText->SetText(formatSize(p->GetFilesize() / 1024));
    }

    if (m_cutlistImage)
    {
        if (p->HasCutlist())
            m_cutlistImage->Show();
        else
            m_cutlistImage->Hide();
    }

    if (m_previewImage)
    {
        // try to locate a preview image
        if (QFile::exists(p->GetPathname() + ".png"))
        {
            m_previewImage->SetFilename(p->GetPathname() + ".png");
            m_previewImage->Load();
        }
        else
        {
            m_previewImage->SetFilename("blank.png");
            m_previewImage->Load();
        }
    }
}

void RecordingSelector::OKPressed()
{
    // loop though selected recordings and add them to the list
    ProgramInfo *p;
    ArchiveItem *a;

    // remove any items that have been removed from the list
    QList<ArchiveItem *> tempAList;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        bool found = false;

        for (int y = 0; y < m_selectedList.size(); y++)
        {
            p = m_selectedList.at(y);
            if (a->type != "Recording" || a->filename == p->GetPlaybackURL(false, true))
            {
                found = true;
                break;
            }
        }

        if (!found)
            tempAList.append(a);
    }

    for (int x = 0; x < tempAList.size(); x++)
        m_archiveList->removeAll(tempAList.at(x));

    // remove any items that are already in the list
    QList<ProgramInfo *> tempSList;
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        p = m_selectedList.at(x);

        for (int y = 0; y < m_archiveList->size(); y++)
        {
            a = m_archiveList->at(y);
            if (a->filename == p->GetPlaybackURL(false, true))
            {
                tempSList.append(p);
                break;
            }
        }
    }

    for (int x = 0; x < tempSList.size(); x++)
        m_selectedList.removeAll(tempSList.at(x));

    // add all that are left
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        p = m_selectedList.at(x);
        a = new ArchiveItem;
        a->type = "Recording";
        a->title = p->GetTitle();
        a->subtitle = p->GetSubtitle();
        a->description = p->GetDescription();
        a->startDate = p->GetScheduledStartTime()
            .toLocalTime().toString("dd MMM yy");
        a->startTime = p->GetScheduledStartTime()
            .toLocalTime().toString("(hh:mm)");
        a->size = p->GetFilesize();
        a->filename = p->GetPlaybackURL(false, true);
        a->hasCutlist = p->HasCutlist();
        a->useCutlist = false;
        a->duration = 0;
        a->cutDuration = 0;
        a->videoWidth = 0;
        a->videoHeight = 0;
        a->fileCodec = "";
        a->videoCodec = "";
        a->encoderProfile = NULL;
        a->editedDetails = false;
        m_archiveList->append(a);
    }

    emit haveResult(true);
    Close();
}

void RecordingSelector::cancelPressed()
{
    emit haveResult(false);
    Close();
}

void RecordingSelector::updateRecordingList(void)
{
    if (!m_recordingList || m_recordingList->empty())
        return;

    m_recordingButtonList->Reset();

    if (m_categorySelector)
    {
        ProgramInfo *p;
        vector<ProgramInfo *>::iterator i = m_recordingList->begin();
        for ( ; i != m_recordingList->end(); ++i)
        {
            p = *i;

            if (p->GetTitle() == m_categorySelector->GetValue() ||
                m_categorySelector->GetValue() == tr("All Recordings"))
            {
                MythUIButtonListItem* item = new MythUIButtonListItem(
                    m_recordingButtonList,
                    p->GetTitle() + " ~ " +
                    p->GetScheduledStartTime().toLocalTime()
                    .toString("dd MMM yy (hh:mm)"));
                item->setCheckable(true);
                if (m_selectedList.indexOf((ProgramInfo *) p) != -1)
                {
                    item->setChecked(MythUIButtonListItem::FullChecked);
                }
                else
                {
                    item->setChecked(MythUIButtonListItem::NotChecked);
                }

                QString title = p->GetTitle();
                QString subtitle = p->GetSubtitle();

                QDateTime recstartts = p->GetScheduledStartTime();
                QDateTime recendts   = p->GetScheduledEndTime();

                QString timedate = QString("%1 - %2")
                    .arg(MythDate::toString(recstartts,MythDate::kDateTimeFull))
                    .arg(MythDate::toString(recendts, MythDate::kTime));

                uint season = p->GetSeason();
                uint episode = p->GetEpisode();
                QString seasone, seasonx;

                if (season && episode)
                {
                    seasone = QString("s%1e%2")
                        .arg(format_season_and_episode(season, 2))
                        .arg(format_season_and_episode(episode, 2));
                    seasonx = QString("%1x%2")
                        .arg(format_season_and_episode(season, 1))
                        .arg(format_season_and_episode(episode, 2));
                }

                item->SetText(title, "title");
                item->SetText(subtitle, "subtitle");
                if (subtitle.isEmpty())
                    item->SetText(title, "titlesubtitle");
                else
                    item->SetText(title + " - \"" + subtitle + '"',
                                  "titlesubtitle");

                item->SetText(timedate, "timedate");
                item->SetText(p->GetDescription(), "description");
                item->SetText(formatSize(p->GetFilesize() / 1024),
                              "filesize_str");

                item->SetText(QString::number(season), "season");
                item->SetText(QString::number(episode), "episode");
                item->SetText(seasonx, "00x00");
                item->SetText(seasone, "s00e00");

                item->DisplayState(p->HasCutlist() ? "yes" : "no", "cutlist");

                item->SetData(qVariantFromValue(p));
            }
            qApp->processEvents();
        }
    }

    m_recordingButtonList->SetItemCurrent(m_recordingButtonList->GetItemFirst());
    titleChanged(m_recordingButtonList->GetItemCurrent());
}

void RecordingSelector::getRecordingList(void)
{
    ProgramInfo *p;
    m_recordingList = RemoteGetRecordedList(-1);
    m_categories.clear();

    if (m_recordingList && !m_recordingList->empty())
    {
        vector<ProgramInfo *>::iterator i = m_recordingList->begin();
        for ( ; i != m_recordingList->end(); ++i)
        {
            p = *i;
            // ignore live tv and deleted recordings
            if (p->GetRecordingGroup() == "LiveTV" ||
                p->GetRecordingGroup() == "Deleted")
            {
                i = m_recordingList->erase(i);
                --i;
                continue;
            }

            if (m_categories.indexOf(p->GetTitle()) == -1)
                m_categories.append(p->GetTitle());
        }
    }
}

void RecordingSelector::updateCategorySelector(void)
{
    // sort and add categories to selector
    m_categories.sort();

    if (m_categorySelector)
    {
        new MythUIButtonListItem(m_categorySelector, tr("All Recordings"));
        m_categorySelector->SetItemCurrent(0);

        for (int x = 0; x < m_categories.count(); x++)
        {
            new MythUIButtonListItem(m_categorySelector, m_categories[x]);
        }
    }
}

void RecordingSelector::setCategory(MythUIButtonListItem *item)
{
    (void)item;
    updateRecordingList();
}

void RecordingSelector::updateSelectedList()
{
    if (!m_recordingList)
        return;

    m_selectedList.clear();

    ProgramInfo *p;
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        for (uint y = 0; y < m_recordingList->size(); y++)
        {
            p = m_recordingList->at(y);
            if (p->GetPlaybackURL(false, true) == a->filename)
            {
                if (m_selectedList.indexOf(p) == -1)
                    m_selectedList.append(p);
                break;
            }

            qApp->processEvents();
        }
    }
}
