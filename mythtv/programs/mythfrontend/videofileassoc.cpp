#include <algorithm>
#include <vector>
#include <iterator>
#include <map>

#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythuicheckbox.h"
#include "mythuibutton.h"
#include "dbaccess.h"
#include "videoutils.h"

#include "videofileassoc.h"

namespace
{
    template <typename T, typename Inst, typename FuncType>
    void assign_if_changed_notify(T &oldVal, const T &newVal, Inst *inst,
            FuncType func)
    {
        if (oldVal != newVal)
        {
            oldVal = newVal;
            func(inst);
        }
    }

    class FileAssociationWrap
    {
      public:
        enum FA_State {
            efsNONE,
            efsDELETE,
            efsSAVE
        };

      public:
        FileAssociationWrap(const QString &new_extension) : m_state(efsSAVE)
        {
            m_fa.extension = new_extension;
        }

        FileAssociationWrap(const FileAssociations::file_association &fa) :
            m_fa(fa), m_state(efsNONE) {}

        unsigned int GetIDx(void) const { return m_fa.id; }
        QString GetExtension(void) const { return m_fa.extension; }
        QString GetCommand(void) const { return m_fa.playcommand; }
        bool GetDefault(void) const { return m_fa.use_default; }
        bool GetIgnore(void) const { return m_fa.ignore; }

        FA_State GetState() const { return m_state; }

        void CommitChanges()
        {
            switch (m_state)
            {
                case efsDELETE:
                {
                    FileAssociations::getFileAssociation().remove(m_fa.id);
                    m_fa.id = 0;
                    m_state = efsNONE;
                    break;
                }
                case efsSAVE:
                {
                    if (FileAssociations::getFileAssociation().add(m_fa))
                    {
                        m_state = efsNONE;
                    }
                    break;
                }
                case efsNONE:
                default: {}
            }
        }

        void MarkForDeletion()
        {
            m_state = efsDELETE;
        }

        void SetDefault(bool yes_or_no)
        {
            assign_if_changed_notify(m_fa.use_default, yes_or_no, this,
                    std::mem_fun(&FileAssociationWrap::SetChanged));
        }

        void SetIgnore(bool yes_or_no)
        {
            assign_if_changed_notify(m_fa.ignore, yes_or_no, this,
                    std::mem_fun(&FileAssociationWrap::SetChanged));
        }

        void SetCommand(const QString &new_command)
        {
            assign_if_changed_notify(m_fa.playcommand, new_command, this,
                    std::mem_fun(&FileAssociationWrap::SetChanged));
        }

      private:
        void SetChanged() { m_state = efsSAVE; }

      private:
        FileAssociations::file_association m_fa;
        FA_State m_state;
    };

    class BlockSignalsGuard
    {
      public:
        void Block(QObject *o)
        {
            o->blockSignals(true);
            m_objects.push_back(o);
        }

        ~BlockSignalsGuard()
        {
            for (list_type::iterator p = m_objects.begin();
                    p != m_objects.end(); ++p)
            {
                (*p)->blockSignals(false);
            }
        }

      private:
        typedef std::vector<QObject *> list_type;

      private:
        list_type m_objects;
    };

    struct UIDToFAPair
    {
        typedef unsigned int UID_type;

        UIDToFAPair() : m_uid(0), m_file_assoc(0) {}

        UIDToFAPair(UID_type uid, FileAssociationWrap *assoc) :
            m_uid(uid), m_file_assoc(assoc) {}

        UID_type m_uid;
        FileAssociationWrap *m_file_assoc;
    };


    bool operator<(const UIDToFAPair &lhs, const UIDToFAPair &rhs)
    {
        if (lhs.m_file_assoc && rhs.m_file_assoc)
            return QString::localeAwareCompare(lhs.m_file_assoc->GetExtension(),
                    rhs.m_file_assoc->GetExtension()) < 0;

        return rhs.m_file_assoc;
    }
}

////////////////////////////////////////////////////////////////////////

class FileAssocDialogPrivate
{
  public:
    typedef std::vector<UIDToFAPair> UIReadyList_type;

  public:
    FileAssocDialogPrivate() : m_nextFAID(0), m_selectionOverride(0)
    {
        LoadFileAssociations();
    }

    ~FileAssocDialogPrivate()
    {
        for (FA_collection::iterator p = m_fileAssociations.begin();
                p != m_fileAssociations.end(); ++p)
        {
            delete p->second;
        }
    }

   void SaveFileAssociations()
   {
        for (FA_collection::iterator p = m_fileAssociations.begin();
                p != m_fileAssociations.end(); ++p)
        {
            p->second->CommitChanges();
        }
    }

    bool AddExtension(QString newExtension, UIDToFAPair::UID_type &new_id)
    {
        if (newExtension.length())
        {
            new_id = ++m_nextFAID;
            m_fileAssociations.insert(FA_collection::value_type(new_id,
                            new FileAssociationWrap(newExtension)));
            return true;
        }

        return false;
    }

    bool DeleteExtension(UIDToFAPair::UID_type uid)
    {
        FA_collection::iterator p = m_fileAssociations.find(uid);
        if (p != m_fileAssociations.end())
        {
            p->second->MarkForDeletion();

            return true;
        }

        return false;
    }

    // Returns a list sorted by extension
    UIReadyList_type GetUIReadyList()
    {
        UIReadyList_type ret;
        std::transform(m_fileAssociations.begin(), m_fileAssociations.end(),
                std::back_inserter(ret), fa_col_ent_2_UIDFAPair());
        UIReadyList_type::iterator deleted = std::remove_if(ret.begin(),
                ret.end(), test_fa_state<FileAssociationWrap::efsDELETE>());

        if (deleted != ret.end())
            ret.erase(deleted, ret.end());

        std::sort(ret.begin(), ret.end());

        return ret;
    }

    FileAssociationWrap *GetCurrentFA(MythUIButtonList *buttonList)
    {
        MythUIButtonListItem *item = buttonList->GetItemCurrent();
        if (item)
        {
            UIDToFAPair key = item->GetData().value<UIDToFAPair>();
            if (key.m_file_assoc)
            {
                return key.m_file_assoc;
            }
        }

        return NULL;
    }

    void SetSelectionOverride(UIDToFAPair::UID_type new_sel)
    {
        m_selectionOverride = new_sel;
    }

    UIDToFAPair::UID_type GetSelectionOverride() const
    {
        return m_selectionOverride;
    }

  private:
    typedef std::map<UIDToFAPair::UID_type, FileAssociationWrap *>
            FA_collection;

  private:
    struct fa_col_ent_2_UIDFAPair
    {
        UIDToFAPair operator()(
                const FileAssocDialogPrivate::FA_collection::value_type &from)
        {
            return UIDToFAPair(from.first, from.second);
        }
    };

    template <FileAssociationWrap::FA_State against>
    struct test_fa_state
    {
        bool operator()(const UIDToFAPair &item)
        {
            if (item.m_file_assoc && item.m_file_assoc->GetState() == against)
                return true;
            return false;
        }
    };

    void LoadFileAssociations()
    {
        typedef std::vector<UIDToFAPair> tmp_fa_list;

        const FileAssociations::association_list &fa_list =
                FileAssociations::getFileAssociation().getList();
        tmp_fa_list tmp_fa;
        tmp_fa.reserve(fa_list.size());

        for (FileAssociations::association_list::const_iterator p =
                fa_list.begin(); p != fa_list.end(); ++p)
        {
            tmp_fa.push_back(UIDToFAPair(++m_nextFAID,
                            new FileAssociationWrap(*p)));
        }

        std::random_shuffle(tmp_fa.begin(), tmp_fa.end());

        for (tmp_fa_list::const_iterator p = tmp_fa.begin(); p != tmp_fa.end();
                ++p)
        {
            m_fileAssociations.insert(FA_collection::value_type(p->m_uid,
                            p->m_file_assoc));
        }

        if (m_fileAssociations.empty())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("%1: Couldn't get any filetypes from your database.")
                    .arg(__FILE__));
        }
    }

  private:
    FA_collection m_fileAssociations;
    UIDToFAPair::UID_type m_nextFAID;
    UIDToFAPair::UID_type m_selectionOverride;
};

////////////////////////////////////////////////////////////////////////

FileAssocDialog::FileAssocDialog(MythScreenStack *screenParent,
        const QString &lname) :
    MythScreenType(screenParent, lname), m_commandEdit(0),
    m_extensionList(0), m_defaultCheck(0), m_ignoreCheck(0), m_doneButton(0),
    m_newButton(0), m_deleteButton(0), m_private(new FileAssocDialogPrivate)
{
}

FileAssocDialog::~FileAssocDialog()
{
    delete m_private;
}

bool FileAssocDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "file_associations", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_extensionList, "extension_select", &err);
    UIUtilE::Assign(this, m_commandEdit, "command", &err);
    UIUtilE::Assign(this, m_ignoreCheck, "ignore_check", &err);
    UIUtilE::Assign(this, m_defaultCheck, "default_check", &err);

    UIUtilE::Assign(this, m_doneButton, "done_button", &err);
    UIUtilE::Assign(this, m_newButton, "new_button", &err);
    UIUtilE::Assign(this, m_deleteButton, "delete_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'file_associations'");
        return false;
    }

    connect(m_extensionList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(OnFASelected(MythUIButtonListItem *)));
    connect(m_commandEdit, SIGNAL(valueChanged()),
            SLOT(OnPlayerCommandChanged()));
    connect(m_defaultCheck, SIGNAL(valueChanged()), SLOT(OnUseDefaltChanged()));
    connect(m_ignoreCheck, SIGNAL(valueChanged()), SLOT(OnIgnoreChanged()));

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(OnDonePressed()));
    connect(m_newButton, SIGNAL(Clicked()),
            SLOT(OnNewExtensionPressed()));
    connect(m_deleteButton, SIGNAL(Clicked()), SLOT(OnDeletePressed()));

    m_extensionList->SetHelpText(tr("Select a file extension from this list "
                                    "to modify or delete its settings."));
    m_commandEdit->SetHelpText(tr("The command to use when playing this kind "
                                  "of file.  To use MythTV's Internal player, "
                                  "use \"Internal\" as the player.  For all other "
                                  "players, you can use %s to substitute the filename."));
    m_ignoreCheck->SetHelpText(tr("When checked, this will cause the file extension "
                                  "to be ignored in scans of your library."));
    m_defaultCheck->SetHelpText(tr("When checked, this will cause the global player "
                                   "settings to override this one."));
    m_doneButton->SetHelpText(tr("Save and exit this screen."));
    m_newButton->SetHelpText(tr("Create a new file extension."));
    m_deleteButton->SetHelpText(tr("Delete this file extension."));

    UpdateScreen();

    BuildFocusList();

    return true;
}

void FileAssocDialog::OnFASelected(MythUIButtonListItem *item)
{
    (void) item;
    UpdateScreen();
}

void FileAssocDialog::OnUseDefaltChanged()
{
    if (m_private->GetCurrentFA(m_extensionList))
        m_private->GetCurrentFA(m_extensionList)->
                SetDefault(m_defaultCheck->GetBooleanCheckState());
}

void FileAssocDialog::OnIgnoreChanged()
{
    if (m_private->GetCurrentFA(m_extensionList))
        m_private->GetCurrentFA(m_extensionList)->
                SetIgnore(m_ignoreCheck->GetBooleanCheckState());
}

void FileAssocDialog::OnPlayerCommandChanged()
{
    if (m_private->GetCurrentFA(m_extensionList))
        m_private->GetCurrentFA(m_extensionList)->
                SetCommand(m_commandEdit->GetText());
}

void FileAssocDialog::OnDonePressed()
{
    m_private->SaveFileAssociations();
    Close();
}

void FileAssocDialog::OnDeletePressed()
{
    MythUIButtonListItem *item = m_extensionList->GetItemCurrent();
    if (item)
    {
        UIDToFAPair key = item->GetData().value<UIDToFAPair>();
        if (key.m_file_assoc && m_private->DeleteExtension(key.m_uid))
            delete item;
    }

    UpdateScreen();
}

void FileAssocDialog::OnNewExtensionPressed()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Enter the new extension:");

    MythTextInputDialog *newextdialog =
                                new MythTextInputDialog(popupStack, message);

    if (newextdialog->Create())
        popupStack->AddScreen(newextdialog);

    connect(newextdialog, SIGNAL(haveResult(QString)),
            SLOT(OnNewExtensionComplete(QString)));
}

void FileAssocDialog::OnNewExtensionComplete(QString newExtension)
{
    UIDToFAPair::UID_type new_sel = 0;
    if (m_private->AddExtension(newExtension, new_sel))
    {
        m_private->SetSelectionOverride(new_sel);
        UpdateScreen(true);
    }
}

void FileAssocDialog::UpdateScreen(bool useSelectionOverride /* = false*/)
{
    BlockSignalsGuard bsg;
    bsg.Block(m_extensionList);
    bsg.Block(m_commandEdit);
    bsg.Block(m_defaultCheck);
    bsg.Block(m_ignoreCheck);

    FileAssocDialogPrivate::UIReadyList_type tmp_list =
            m_private->GetUIReadyList();

    if (tmp_list.empty())
    {
        m_extensionList->SetVisible(false);
        m_commandEdit->SetVisible(false);
        m_defaultCheck->SetVisible(false);
        m_ignoreCheck->SetVisible(false);
        m_deleteButton->SetVisible(false);
    }
    else
    {
        UIDToFAPair::UID_type selected_id = 0;
        MythUIButtonListItem *current_item = m_extensionList->GetItemCurrent();
        if (current_item)
        {
            UIDToFAPair key = current_item->GetData().value<UIDToFAPair>();
            if (key.m_file_assoc)
            {
                selected_id = key.m_uid;
            }
        }

        if (useSelectionOverride)
            selected_id = m_private->GetSelectionOverride();

        m_extensionList->SetVisible(true);
        m_extensionList->Reset();

        for (FileAssocDialogPrivate::UIReadyList_type::iterator p =
                tmp_list.begin(); p != tmp_list.end(); ++p)
        {
            if (p->m_file_assoc)
            {
                MythUIButtonListItem *new_item =
                        new MythUIButtonListItem(m_extensionList,
                                p->m_file_assoc->GetExtension(),
                                QVariant::fromValue(*p));
                if (selected_id && p->m_uid == selected_id)
                    m_extensionList->SetItemCurrent(new_item);
            }
        }

        current_item = m_extensionList->GetItemCurrent();
        if (current_item)
        {
            UIDToFAPair key = current_item->GetData().value<UIDToFAPair>();
            if (key.m_file_assoc)
            {
                m_commandEdit->SetVisible(true);
                m_commandEdit->SetText(key.m_file_assoc->GetCommand());

                m_defaultCheck->SetVisible(true);
                m_defaultCheck->SetCheckState(key.m_file_assoc->GetDefault() ?
                        MythUIStateType::Full : MythUIStateType::Off);

                m_ignoreCheck->SetVisible(true);
                m_ignoreCheck->SetCheckState(key.m_file_assoc->GetIgnore() ?
                        MythUIStateType::Full : MythUIStateType::Off);

                m_deleteButton->SetVisible(true);
            }
        }
    }

    BuildFocusList();
}

Q_DECLARE_METATYPE(UIDToFAPair)
