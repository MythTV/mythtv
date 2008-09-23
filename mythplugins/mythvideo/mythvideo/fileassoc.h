#ifndef FILEASSOC_H_
#define FILEASSOC_H_

// QT headers
#include <QList>

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuicheckbox.h>
#include <mythtv/libmythui/mythuibutton.h>

class FileAssociation;

class FileAssocDialog : public MythScreenType
{

  Q_OBJECT

  public:

    FileAssocDialog(MythScreenStack *parent, const QString &name);
    ~FileAssocDialog();

    bool Create();

    void loadFileAssociations();
    void saveFileAssociations();
    void showCurrentFA();

  public slots:

    void switchToFA(MythListButtonItem*);
    void saveAndExit();
    void toggleDefault();
    void toggleIgnore();
    void setPlayerCommand();
    void deleteCurrent();
    void makeNewExtension();
    void createExtension(QString);

  private:

    QList<FileAssociation*>     m_fileAssociations;
    FileAssociation             *m_currentFileAssociation;

    MythUITextEdit      *m_commandEdit;
    MythListButton      *m_extensionList;
    MythUICheckBox      *m_defaultCheck;
    MythUICheckBox      *m_ignoreCheck;
    MythUIButton        *m_doneButton;
    MythUIButton        *m_newButton;
    MythUIButton        *m_deleteButton;
};

#endif
