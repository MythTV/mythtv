#ifndef GALLERYCONFIG_H
#define GALLERYCONFIG_H

// Qt headers

// MythTV headers
#include "mythscreentype.h"



class GalleryConfig : public MythScreenType
{
    Q_OBJECT
public:
    GalleryConfig(MythScreenStack *parent, const char *name);
    ~GalleryConfig();

    bool Create();
    bool keyPressEvent(QKeyEvent *);

signals:
    void configSaved();

private:
    MythUITextEdit     *m_storageGroupName;
    MythUIButtonList   *m_sortOrder;
    MythUISpinBox      *m_slideShowTime;
    MythUIButtonList   *m_transitionType;
    MythUISpinBox      *m_transitionTime;
    MythUICheckBox     *m_showHiddenFiles;

    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

private slots:
    void Save();
    void Exit();
    void Load();
};

#endif // GALLERYCONFIG_H
