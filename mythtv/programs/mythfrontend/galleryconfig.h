//! \file
//! \brief Provides Gallery configuration screens

#ifndef GALLERYCONFIG_H
#define GALLERYCONFIG_H

#include "mythconfigdialogs.h"
#include "mythconfiggroups.h"


//! Settings page 1
class GallerySettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    GallerySettings();
};


//! Settings page 2
class DatabaseSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    explicit DatabaseSettings(bool enable);

signals:
    void ClearDbPressed();
};


//! Gallery Settings pages
class GalleryConfig : public ConfigurationWizard
{
public:
    explicit GalleryConfig(bool editMode)
    {
        addChild(new GallerySettings());
        m_dbGroup = new DatabaseSettings(editMode);
        addChild(m_dbGroup);
    }

    DatabaseSettings *GetClearPage() { return m_dbGroup; }

private:
    DatabaseSettings *m_dbGroup;
};


#endif // GALLERYCONFIG_H
