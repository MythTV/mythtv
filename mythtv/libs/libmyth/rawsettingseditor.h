#ifndef RAW_SETTINGS_EDITOR_H_
#define RAW_SETTINGS_EDITOR_H_

#include <QEvent>
#include <QHash>
#include <QObject>

#include "mythexp.h"
#include "mythscreentype.h"

#include "mythuibutton.h"
#include "mythuishape.h"
#include "mythuitextedit.h"

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUITextEdit;
class MythUIButton;

/** \class RawSettingsEditor
 *  \brief An editor screen that allows manipulation of raw settings values
 *
 *  This class is a base to build on.  It is not meant to be used directly.
 *  Any inheritting class must set the title and list of settings names in
 *  its constructor.
 */
class MPUBLIC RawSettingsEditor : public MythScreenType
{
    Q_OBJECT

  public:
    // Constructor
    RawSettingsEditor(MythScreenStack *parent, const char *name = 0);

    // Destructor
   ~RawSettingsEditor();

    // MythScreenType overrides
    bool Create(void);
    void Load(void);
    void Init(void);

  private slots:
    // Saves changes
    void Save(void);

    // Helpers for keeping track of current value and updating the screen
    void selectionChanged(MythUIButtonListItem *item);
    void valueChanged(void);

  protected:
    // Protected data, directly accessed by inheritting classes
    QString                 m_title;
    QMap <QString, QString> m_settings;

  private:
    // Helper for updating prev/next values on screen
    void updatePrevNextTexts(void);

    // UI Widgets on screen
    MythUIButtonList *m_settingsList;
    MythUITextEdit   *m_settingValue;

    MythUIButton     *m_saveButton;
    MythUIButton     *m_cancelButton;

    MythUIText       *m_textLabel;

    // Prev/Next text areas on screen
    QHash <int, MythUIText*>  m_prevNextTexts;
    QHash <int, MythUIShape*> m_prevNextShapes;

    // Original and current settings values used for selective saving
    QHash <QString, QString>  m_origValues;
    QHash <QString, QString>  m_settingValues;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
