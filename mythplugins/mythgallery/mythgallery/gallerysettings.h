#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QCoreApplication>

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class GallerySettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(GallerySettings)

  public:  
    GallerySettings();
};

#endif
