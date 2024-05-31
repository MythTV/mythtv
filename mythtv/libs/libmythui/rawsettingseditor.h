#ifndef RAW_SETTINGS_EDITOR_H_
#define RAW_SETTINGS_EDITOR_H_

#include <QEvent>
#include <QHash>
#include <QObject>

#include "mythuiexp.h"
#include "libmythui/mythscreentype.h"

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUIShape;

/** \class RawSettingsEditor
 *  \brief An editor screen that allows manipulation of raw settings values
 *
 *  This class is a base to build on.  It is not meant to be used directly.
 *  Any inheritting class must set the title and list of settings names in
 *  its constructor.
 */
class MUI_PUBLIC RawSettingsEditor : public MythScreenType
{
    Q_OBJECT

  public:
    // Constructor
    explicit RawSettingsEditor(MythScreenStack *parent, const char *name = nullptr);

    // Destructor
   ~RawSettingsEditor() override = default;

    // MythScreenType overrides
    bool Create(void) override; // MythScreenType
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

  private slots:
    // Saves changes
    void Save(void);

    // Helpers for keeping track of current value and updating the screen
    void selectionChanged(MythUIButtonListItem *item);
    void valueChanged(void);

  protected:
    // Protected data, directly accessed by inheritting classes
    QString                   m_title;
    QMap <QString, QString>   m_settings;

  private:
    // Helper for updating prev/next values on screen
    void updatePrevNextTexts(void);

    // UI Widgets on screen
    MythUIButtonList         *m_settingsList {nullptr};
    MythUITextEdit           *m_settingValue {nullptr};

    MythUIButton             *m_saveButton   {nullptr};
    MythUIButton             *m_cancelButton {nullptr};

    MythUIText               *m_textLabel    {nullptr};

    // Prev/Next text areas on screen
    QHash <int, MythUIText*>  m_prevNextTexts;
    QHash <int, MythUIShape*> m_prevNextShapes;

    // Original and current settings values used for selective saving
    QHash <QString, QString>  m_origValues;
    QHash <QString, QString>  m_settingValues;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
