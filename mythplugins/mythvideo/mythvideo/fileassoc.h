#ifndef FILEASSOC_H_
#define FILEASSOC_H_

#include <mythtv/libmythui/mythscreentype.h>

class MythScreenStack;
class MythListButtonItem;
class MythUITextEdit;
class MythListButton;
class MythUICheckBox;
class MythUIButton;

class FileAssocDialog : public MythScreenType
{
  Q_OBJECT

  public:
    FileAssocDialog(MythScreenStack *screeParent, const QString &lname);
    ~FileAssocDialog();

    bool Create();

  public slots:
    void OnFASelected(MythListButtonItem *item);

    void OnUseDefaltChanged();
    void OnIgnoreChanged();
    void OnPlayerCommandChanged();

    void OnDonePressed();
    void OnDeletePressed();
    void OnNewExtensionPressed();

    void OnNewExtensionComplete(QString newExtension);

  private:
    void UpdateScreen(bool useSelectionOverride = false);

  private:
    MythUITextEdit *m_commandEdit;
    MythListButton *m_extensionList;
    MythUICheckBox *m_defaultCheck;
    MythUICheckBox *m_ignoreCheck;
    MythUIButton *m_doneButton;
    MythUIButton *m_newButton;
    MythUIButton *m_deleteButton;

    class FileAssocDialogPrivate *m_private;
};

#endif
