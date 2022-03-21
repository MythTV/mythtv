// Qt
#include <QObject>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythui/mythuihelper.h"

   class SettingsHelper : public QObject
    {
        Q_OBJECT

      public:

        SettingsHelper(void) = default;

        ~SettingsHelper(void) override = default;

      public slots:
        void RunProlog(const QString &settingsPage)
        {
            m_settingsPage = settingsPage;

            LOG(VB_GENERAL, LOG_DEBUG,
                QString("SettingHelper::RunProlog called: %1").arg(m_settingsPage));

            GetMythUI()->AddCurrentLocation("Setup");
            gCoreContext->ActivateSettingsCache(false);
        }

        void RunEpilog(void)
        {
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("SettingHelper::RunEpilog called: %1").arg(m_settingsPage));

            GetMythUI()->RemoveCurrentLocation();

            gCoreContext->ActivateSettingsCache(true);

            // tell the backend the settings may have changed
            gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");

            // tell the frontend the settings may have changed
            gCoreContext->dispatch(MythEvent(QString("CLEAR_SETTINGS_CACHE")));

            if (m_settingsPage == "settings general" ||
                m_settingsPage == "settings generalrecpriorities")
                ScheduledRecording::ReschedulePlace("TVMenuCallback");
        }

      private:
        QString m_settingsPage;
    };
