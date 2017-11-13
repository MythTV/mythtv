#ifndef MYTHSETTINGS_H_
#define MYTHSETTINGS_H_

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "settings.h"

#include "standardsettings.h"

class VideoGeneralSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(VideoGeneralSettings);

  public:
    VideoGeneralSettings();
};

#endif
