#ifndef EXPERTSETTINGSEDITOR_H
#define EXPERTSETTINGSEDITOR_H

#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "rawsettingseditor.h"

class ExpertSettingsEditor : public RawSettingsEditor
{
    Q_OBJECT

  public:
    ExpertSettingsEditor(MythScreenStack *parent, const char *name = 0)
      : RawSettingsEditor(parent, name)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT value, data "
                        "FROM settings "
                        "WHERE hostname = :HOSTNAME");
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

        if (query.exec())
        {
            while (query.next())
            {
                m_settings[query.value(0).toString()] =
                    query.value(0).toString();
            }
        }

        m_title = tr("Expert Settings Editor");
        m_settings["EventCmdRecPending"] = tr("Recording Pending");
    }
};

#endif

