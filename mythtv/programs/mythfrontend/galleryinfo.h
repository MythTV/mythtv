//! \file
//! \brief The info/details overlay that shows image metadata

#ifndef GALLERYINFO_H
#define GALLERYINFO_H

#include <QTimer>

#include "libmythmetadata/imagemanager.h"
#include "libmythui/mythuibuttonlist.h"


class MythScreenType;

//! Displayed info/details about an image.
enum InfoVisibleState : std::uint8_t
                      { kNoInfo,    //!< Details not displayed
                        kBasicInfo, //!< Shows just the most useful exif tags
                        kFullInfo   //!< Shows all exif tags
                      };

//! The image info/details buttonlist overlay that displays exif tags
class InfoList : public QObject
{
    Q_OBJECT
public:
    explicit InfoList(MythScreenType &screen);

    bool             Create(bool focusable);
    void             Toggle(const ImagePtrK &im);
    bool             Hide();
    void             Update(const ImagePtrK &im);
    void             Display(ImageItemK &im, const QStringList &tagStrings);
    InfoVisibleState GetState() const   { return m_infoVisible; }

private slots:
    void Clear()   { m_btnList->Reset(); }

private:
    void CreateButton(const QString &name, const QString &value);
    void CreateCount(ImageItemK &im);

    MythScreenType   &m_screen;      //!< Parent screen
    MythUIButtonList *m_btnList     {nullptr};  //!< Overlay buttonlist
    InfoVisibleState  m_infoVisible {kNoInfo};  //!< Info list state
    ImageManagerFe   &m_mgr;         //!< Image Manager
    QTimer            m_timer;       //!< Clears list if no new metadata arrives
};

#endif // GALLERYINFO_H
