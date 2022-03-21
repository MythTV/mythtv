#ifndef VIDEOFILEASSOC_H_
#define VIDEOFILEASSOC_H_

#include "libmythui/mythscreentype.h"

class MythScreenStack;
class MythUIButtonListItem;
class MythUITextEdit;
class MythUIButtonList;
class MythUICheckBox;
class MythUIButton;

class FileAssocDialog : public MythScreenType
{
  Q_OBJECT

  public:
    FileAssocDialog(MythScreenStack *screenParent, const QString &lname);
    ~FileAssocDialog() override;

    bool Create() override; // MythScreenType

  public slots:
    void OnFASelected(MythUIButtonListItem *item);

    void OnUseDefaltChanged();
    void OnIgnoreChanged();
    void OnPlayerCommandChanged();

    void OnDonePressed();
    void OnDeletePressed();
    void OnNewExtensionPressed() const;

    void OnNewExtensionComplete(const QString& newExtension);

  private:
    void UpdateScreen(bool useSelectionOverride = false);

  private:
    MythUITextEdit *m_commandEdit     {nullptr};
    MythUIButtonList *m_extensionList {nullptr};
    MythUICheckBox *m_defaultCheck    {nullptr};
    MythUICheckBox *m_ignoreCheck     {nullptr};
    MythUIButton *m_doneButton        {nullptr};
    MythUIButton *m_newButton         {nullptr};
    MythUIButton *m_deleteButton      {nullptr};

    class FileAssocDialogPrivate *m_private;
};

#endif
