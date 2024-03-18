// C++
#include <algorithm>
#include <functional>   //mem_fun
#include <iterator>
#include <map>
#include <random>
#include <utility>
#include <vector>

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythmetadata/dbaccess.h"
#include "libmythmetadata/videoutils.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuitextedit.h"

// MythFrontend
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
        enum FA_State : std::uint8_t {
            efsNONE,
            efsDELETE,
            efsSAVE
        };

      public:
        explicit FileAssociationWrap(const QString &new_extension) : m_state(efsSAVE)
        {
            m_fa.extension = new_extension;
        }

        explicit FileAssociationWrap(FileAssociations::file_association fa) :
            m_fa(std::move(fa)) {}

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
                    std::mem_fn(&FileAssociationWrap::SetChanged));
        }

        void SetIgnore(bool yes_or_no)
        {
            assign_if_changed_notify(m_fa.ignore, yes_or_no, this,
                    std::mem_fn(&FileAssociationWrap::SetChanged));
        }

        void SetCommand(const QString &new_command)
        {
            assign_if_changed_notify(m_fa.playcommand, new_command, this,
                    std::mem_fn(&FileAssociationWrap::SetChanged));
        }

      private:
        void SetChanged() { m_state = efsSAVE; }

      private:
        FileAssociations::file_association m_fa;
        FA_State m_state {efsNONE};
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
            for (auto & obj : m_objects)
                obj->blockSignals(false);
        }

      private:
        using list_type = std::vector<QObject *>;

      private:
        list_type m_objects;
    };

    struct UIDToFAPair
    {
        using UID_type = unsigned int;

        UIDToFAPair() = default;

        UIDToFAPair(UID_type uid, FileAssociationWrap *assoc) :
            m_uid(uid), m_fileAssoc(assoc) {}

        UID_type m_uid {0};
        FileAssociationWrap *m_fileAssoc {nullptr};
    };


    bool operator<(const UIDToFAPair lhs, const UIDToFAPair rhs)
    {
        if (lhs.m_fileAssoc && rhs.m_fileAssoc)
            return QString::localeAwareCompare(lhs.m_fileAssoc->GetExtension(),
                    rhs.m_fileAssoc->GetExtension()) < 0;

        return rhs.m_fileAssoc;
    }
}

////////////////////////////////////////////////////////////////////////

class FileAssocDialogPrivate
{
  public:
    using UIReadyList_type = std::vector<UIDToFAPair>;

  public:
    FileAssocDialogPrivate()
    {
        LoadFileAssociations();
    }

    ~FileAssocDialogPrivate()
    {
        for (auto & fa : m_fileAssociations)
            delete fa.second;
    }

   void SaveFileAssociations()
   {
        for (auto & fa : m_fileAssociations)
            fa.second->CommitChanges();
    }

    bool AddExtension(const QString& newExtension, UIDToFAPair::UID_type &new_id)
    {
        if (!newExtension.isEmpty())
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
        auto p = m_fileAssociations.find(uid);
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
        auto deleted = std::remove_if(ret.begin(),
                ret.end(), test_fa_state<FileAssociationWrap::efsDELETE>());

        if (deleted != ret.end())
            ret.erase(deleted, ret.end());

        std::sort(ret.begin(), ret.end());

        return ret;
    }

    static FileAssociationWrap *GetCurrentFA(MythUIButtonList *buttonList)
    {
        MythUIButtonListItem *item = buttonList->GetItemCurrent();
        if (item)
        {
            auto key = item->GetData().value<UIDToFAPair>();
            if (key.m_fileAssoc)
            {
                return key.m_fileAssoc;
            }
        }

        return nullptr;
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
    using FA_collection = std::map<UIDToFAPair::UID_type, FileAssociationWrap *>;

  private:
    struct fa_col_ent_2_UIDFAPair
    {
        UIDToFAPair operator()(
                const FileAssocDialogPrivate::FA_collection::value_type from)
        {
            return {from.first, from.second};
        }
    };

    template <FileAssociationWrap::FA_State against>
    struct test_fa_state
    {
        bool operator()(const UIDToFAPair item)
        {
            return item.m_fileAssoc && item.m_fileAssoc->GetState() == against;
        }
    };

    void LoadFileAssociations()
    {
        using tmp_fa_list = std::vector<UIDToFAPair>;

        const FileAssociations::association_list &fa_list =
                FileAssociations::getFileAssociation().getList();
        tmp_fa_list tmp_fa;
        tmp_fa.reserve(fa_list.size());

        auto newpair = [this](const auto & fa)
            { return UIDToFAPair(++m_nextFAID, new FileAssociationWrap(fa)); };
        std::transform(fa_list.cbegin(), fa_list.cend(), std::back_inserter(tmp_fa), newpair);

        std::shuffle(tmp_fa.begin(), tmp_fa.end(),
                     std::mt19937(std::random_device()()));

        for (const auto& fa : tmp_fa)
        {
            m_fileAssociations.insert(FA_collection::value_type(fa.m_uid,
                            fa.m_fileAssoc));
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
    UIDToFAPair::UID_type m_nextFAID          {0};
    UIDToFAPair::UID_type m_selectionOverride {0};
};

////////////////////////////////////////////////////////////////////////

FileAssocDialog::FileAssocDialog(MythScreenStack *screenParent,
        const QString &lname) :
    MythScreenType(screenParent, lname),
    m_private(new FileAssocDialogPrivate)
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

    connect(m_extensionList, &MythUIButtonList::itemSelected,
            this, &FileAssocDialog::OnFASelected);
    connect(m_commandEdit, &MythUITextEdit::valueChanged,
            this, &FileAssocDialog::OnPlayerCommandChanged);
    connect(m_defaultCheck, &MythUICheckBox::valueChanged, this, &FileAssocDialog::OnUseDefaltChanged);
    connect(m_ignoreCheck, &MythUICheckBox::valueChanged, this, &FileAssocDialog::OnIgnoreChanged);

    connect(m_doneButton, &MythUIButton::Clicked, this, &FileAssocDialog::OnDonePressed);
    connect(m_newButton, &MythUIButton::Clicked,
            this, &FileAssocDialog::OnNewExtensionPressed);
    connect(m_deleteButton, &MythUIButton::Clicked, this, &FileAssocDialog::OnDeletePressed);

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

void FileAssocDialog::OnFASelected([[maybe_unused]] MythUIButtonListItem *item)
{
    UpdateScreen();
}

void FileAssocDialog::OnUseDefaltChanged()
{
    FileAssociationWrap *wrap = FileAssocDialogPrivate::GetCurrentFA(m_extensionList);
    if (wrap != nullptr)
        wrap->SetDefault(m_defaultCheck->GetBooleanCheckState());
}

void FileAssocDialog::OnIgnoreChanged()
{
    FileAssociationWrap *wrap = FileAssocDialogPrivate::GetCurrentFA(m_extensionList);
    if (wrap != nullptr)
        wrap->SetIgnore(m_ignoreCheck->GetBooleanCheckState());
}

void FileAssocDialog::OnPlayerCommandChanged()
{
    FileAssociationWrap *wrap = FileAssocDialogPrivate::GetCurrentFA(m_extensionList);
    if (wrap != nullptr)
        wrap->SetCommand(m_commandEdit->GetText());
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
        auto key = item->GetData().value<UIDToFAPair>();
        if (key.m_fileAssoc && m_private->DeleteExtension(key.m_uid))
            delete item;
    }

    UpdateScreen();
}

void FileAssocDialog::OnNewExtensionPressed() const
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Enter the new extension:");

    auto *newextdialog = new MythTextInputDialog(popupStack, message);

    if (newextdialog->Create())
        popupStack->AddScreen(newextdialog);

    connect(newextdialog, &MythTextInputDialog::haveResult,
            this, &FileAssocDialog::OnNewExtensionComplete);
}

void FileAssocDialog::OnNewExtensionComplete(const QString& newExtension)
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
            auto key = current_item->GetData().value<UIDToFAPair>();
            if (key.m_fileAssoc)
            {
                selected_id = key.m_uid;
            }
        }

        if (useSelectionOverride)
            selected_id = m_private->GetSelectionOverride();

        m_extensionList->SetVisible(true);
        m_extensionList->Reset();

        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
        for (auto & fad : tmp_list)
        {
            if (fad.m_fileAssoc)
            {
                // No memory leak. MythUIButtonListItem adds the new
                // item into m_extensionList.
                auto *new_item = new MythUIButtonListItem(m_extensionList,
                                fad.m_fileAssoc->GetExtension(),
                                QVariant::fromValue(fad));
                if (selected_id && fad.m_uid == selected_id)
                    m_extensionList->SetItemCurrent(new_item);
            }
        }

        current_item = m_extensionList->GetItemCurrent();
        if (current_item)
        {
            auto key = current_item->GetData().value<UIDToFAPair>();
            if (key.m_fileAssoc)
            {
                m_commandEdit->SetVisible(true);
                m_commandEdit->SetText(key.m_fileAssoc->GetCommand());

                m_defaultCheck->SetVisible(true);
                m_defaultCheck->SetCheckState(key.m_fileAssoc->GetDefault() ?
                        MythUIStateType::Full : MythUIStateType::Off);

                m_ignoreCheck->SetVisible(true);
                m_ignoreCheck->SetCheckState(key.m_fileAssoc->GetIgnore() ?
                        MythUIStateType::Full : MythUIStateType::Off);

                m_deleteButton->SetVisible(true);
            }
        }
    }

    BuildFocusList();
}

Q_DECLARE_METATYPE(UIDToFAPair)
