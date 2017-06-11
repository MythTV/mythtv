#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QCoreApplication>

#include "mythtv/standardsettings.h"
#include "mythtv/mythcontext.h"

class GallerySettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(GallerySettings)

  public:  
    GallerySettings();
};

#endif
