#ifndef MYTH_SYSTEM_EVENT_H_
#define MYTH_SYSTEM_EVENT_H_

#include <QEvent>
#include <QObject>

#include "programinfo.h"
#include "recordinginfo.h"
#include "rawsettingseditor.h"

// Helper commands for formatting and sending a MythSystemLegacyEvent
MTV_PUBLIC void SendMythSystemLegacyRecEvent(const QString msg,
                                    const RecordingInfo *pginfo);
MTV_PUBLIC void SendMythSystemLegacyPlayEvent(const QString msg,
                                     const ProgramInfo *pginfo);

/** \class MythSystemLegacyEventHandler
 *  \brief Handles incoming MythSystemLegacyEvent messages
 *
 *  MythSystemLegacyEventHandler handles incoming MythSystemLegacyEvent messages and runs
 *  the appropriate event handler command on the local system if one is
 *  configured.
 */
class MTV_PUBLIC MythSystemLegacyEventHandler : public QObject
{
    Q_OBJECT

  public:
    // Constructor
    MythSystemLegacyEventHandler();

    // Destructor
   ~MythSystemLegacyEventHandler();

  private:
    // Helpers for converting incoming events to command lines
    void SubstituteMatches(const QStringList &tokens, QString &command);
    QString EventNameToSetting(const QString &name);

    // Custom Event Handler
    void customEvent(QEvent *e);
};

/** \class MythSystemLegacyEventEditor
 *  \brief An editor for MythSystemLegacyEvent handler commands
 *
 *  This class extends RawSettingsEditor and automatically populates the
 *  settings list with the MythSystemLegacyEvent handler command settings names.
 */
class MTV_PUBLIC MythSystemLegacyEventEditor : public RawSettingsEditor
{
    Q_OBJECT

  public:
    MythSystemLegacyEventEditor(MythScreenStack *parent, const char *name = 0);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
