
// c++
#include <cstdlib>
#include <unistd.h>

// qt
#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QTimer>

// mythtv
#include <libmyth/mythcontext.h>
#include <libmythbase/mthread.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythtimer.h>
#include <libmythbase/programinfo.h>
#include <libmythbase/remoteutil.h>
#include <libmythbase/stringutil.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// mytharchive
#include "archiveutil.h"
#include "recordingselector.h"

class GetRecordingListThread : public MThread
{
  public:
    explicit GetRecordingListThread(RecordingSelector *parent) :
        MThread("GetRecordingList"), m_parent(parent)
    {
        start();
    }

    void run(void) override // MThread
    {
        RunProlog();
        m_parent->getRecordingList();
        RunEpilog();
    }

    RecordingSelector *m_parent;
};

RecordingSelector::~RecordingSelector(void)
{
    delete m_recordingList;
    while (!m_selectedList.isEmpty())
        delete m_selectedList.takeFirst();
}

bool RecordingSelector::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "recording_selector", this);
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

    connect(m_okButton, &MythUIButton::Clicked, this, &RecordingSelector::OKPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &RecordingSelector::cancelPressed);

    new MythUIButtonListItem(m_categorySelector, tr("All Recordings"));
    connect(m_categorySelector, &MythUIButtonList::itemSelected,
            this, &RecordingSelector::setCategory);

    connect(m_recordingButtonList, &MythUIButtonList::itemSelected,
            this, &RecordingSelector::titleChanged);
    connect(m_recordingButtonList, &MythUIButtonList::itemClicked,
            this, &RecordingSelector::toggleSelected);

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

    auto *busyPopup = new
            MythUIBusyDialog(message, popupStack, "recordingselectorbusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);
    else
    {
        delete busyPopup;
        busyPopup = nullptr;
    }

    auto *thread = new GetRecordingListThread(this);
    while (thread->isRunning())
    {
        QCoreApplication::processEvents();
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

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            ShowMenu();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RecordingSelector::ShowMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Clear All"), &RecordingSelector::clearAll);
    menuPopup->AddButton(tr("Select All"), &RecordingSelector::selectAll);
}

void RecordingSelector::selectAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    for (auto *p : *m_recordingList)
        m_selectedList.append(p);

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
        int index = m_selectedList.indexOf(item->GetData().value<ProgramInfo *>());
        if (index != -1)
            m_selectedList.takeAt(index);
        item->setChecked(MythUIButtonListItem:: NotChecked);
    }
    else
    {
        int index = m_selectedList.indexOf(item->GetData().value<ProgramInfo *>());
        if (index == -1)
            m_selectedList.append(item->GetData().value<ProgramInfo *>());

        item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void RecordingSelector::titleChanged(MythUIButtonListItem *item)
{
    auto *p = item->GetData().value<ProgramInfo *>();

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
        m_filesizeText->SetText(StringUtil::formatKBytes(p->GetFilesize() / 1024));
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
    // remove any items that have been removed from the list
    QList<ArchiveItem *> tempAList;
    for (auto *a : std::as_const(*m_archiveList))
    {
        bool found = false;

        for (auto *p : std::as_const(m_selectedList))
        {
            if (a->type != "Recording" || a->filename == p->GetPlaybackURL(false, true))
            {
                found = true;
                break;
            }
        }

        if (!found)
            tempAList.append(a);
    }

    for (auto *x : std::as_const(tempAList))
        m_archiveList->removeAll(x);

    // remove any items that are already in the list
    QList<ProgramInfo *> tempSList;
    for (auto *p : std::as_const(m_selectedList))
    {
        for (const auto *a : std::as_const(*m_archiveList))
        {
            if (a->filename == p->GetPlaybackURL(false, true))
            {
                tempSList.append(p);
                break;
            }
        }
    }

    for (auto *x : std::as_const(tempSList))
        m_selectedList.removeAll(x);

    // add all that are left
    for (auto *p : std::as_const(m_selectedList))
    {
        auto *a = new ArchiveItem;
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
        a->encoderProfile = nullptr;
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
        for (auto *p : *m_recordingList)
        {
            if (p->GetTitle() == m_categorySelector->GetValue() ||
                m_categorySelector->GetValue() == tr("All Recordings"))
            {
                auto* item = new MythUIButtonListItem(
                    m_recordingButtonList,
                    p->GetTitle() + " ~ " +
                    p->GetScheduledStartTime().toLocalTime()
                    .toString("dd MMM yy (hh:mm)"));
                item->setCheckable(true);
                if (m_selectedList.indexOf(p) != -1)
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
                    .arg(MythDate::toString(recstartts,MythDate::kDateTimeFull),
                         MythDate::toString(recendts, MythDate::kTime));

                uint season = p->GetSeason();
                uint episode = p->GetEpisode();
                QString seasone;
                QString seasonx;

                if (season && episode)
                {
                    seasone = QString("s%1e%2")
                        .arg(StringUtil::intToPaddedString(season, 2),
                             StringUtil::intToPaddedString(episode, 2));
                    seasonx = QString("%1x%2")
                        .arg(StringUtil::intToPaddedString(season, 1),
                             StringUtil::intToPaddedString(episode, 2));
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
                item->SetText(StringUtil::formatKBytes(p->GetFilesize() / 1024),
                              "filesize_str");

                item->SetText(QString::number(season), "season");
                item->SetText(QString::number(episode), "episode");
                item->SetText(seasonx, "00x00");
                item->SetText(seasone, "s00e00");

                item->DisplayState(p->HasCutlist() ? "yes" : "no", "cutlist");

                item->SetData(QVariant::fromValue(p));
            }
            QCoreApplication::processEvents();
        }
    }

    m_recordingButtonList->SetItemCurrent(m_recordingButtonList->GetItemFirst());
    titleChanged(m_recordingButtonList->GetItemCurrent());
}

void RecordingSelector::getRecordingList(void)
{
    m_recordingList = RemoteGetRecordedList(-1);
    m_categories.clear();

    if (m_recordingList && !m_recordingList->empty())
    {
        auto i = m_recordingList->begin();
        for ( ; i != m_recordingList->end(); ++i)
        {
            ProgramInfo *p = *i;
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

void RecordingSelector::setCategory([[maybe_unused]] MythUIButtonListItem *item)
{
    updateRecordingList();
}

void RecordingSelector::updateSelectedList()
{
    if (!m_recordingList)
        return;

    m_selectedList.clear();

    for (const auto *a : std::as_const(*m_archiveList))
    {
        for (auto *p : *m_recordingList)
        {
            if (p->GetPlaybackURL(false, true) == a->filename)
            {
                if (m_selectedList.indexOf(p) == -1)
                    m_selectedList.append(p);
                break;
            }

            QCoreApplication::processEvents();
        }
    }
}
