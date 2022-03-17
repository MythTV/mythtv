// Qt headers
#include <QHash>
#include <QCoreApplication>
#include <QStringList>

#include "libmythbase/mythdirs.h"

#ifdef HAVE_LIBBLURAY
#include <libbluray/meta_data.h>
#else
#include "libbluray/bdnav/meta_data.h"
#endif
#include "bluraymetadata.h"

BlurayMetadata::~BlurayMetadata()
{
    if (m_bdnav)
       bd_close(m_bdnav);
}

bool BlurayMetadata::OpenDisc(void)
{
    if (IsOpen())
        return true;

    QString keyfile = QString("%1/KEYDB.cfg").arg(GetConfDir());
    QByteArray keyarray = keyfile.toLatin1();
    const char *keyfilepath = keyarray.data();

    m_bdnav = bd_open(m_path.toLatin1().data(), keyfilepath);

    return m_bdnav != nullptr;
}

bool BlurayMetadata::ParseDisc(void)
{
    if (!OpenDisc() && !m_bdnav)
        return false;

    const meta_dl *metadata = bd_get_meta(m_bdnav);

    if (metadata)
    {
        m_title = QString(metadata->di_name);
        m_alttitle = QString(metadata->di_alternative);
        m_language = QString(metadata->language_code);
        m_discnumber = metadata->di_set_number;
        m_disctotal = metadata->di_num_sets;

        for (unsigned i = 0; i < metadata->toc_count; i++)
        {
            uint num = metadata->toc_entries[i].title_number;
            QString title = QString(metadata->toc_entries[i].title_name);
            QPair<uint,QString> ret(num,title); 
            m_titles.append(ret);
        }

        for (unsigned i = 0; i < metadata->thumb_count; i++)
        {
            QString filepath = QString("%1/BDMV/META/DL/%2")
                                   .arg(m_path, metadata->thumbnails[i].path);
            m_images.append(filepath);
        }
    }

    const BLURAY_DISC_INFO *discinfo = bd_get_disc_info(m_bdnav);
    if (discinfo)
    {
        m_topMenuSupported   = (discinfo->top_menu_supported != 0U);
        m_firstPlaySupported = (discinfo->first_play_supported != 0U);
        m_numHDMVTitles = discinfo->num_hdmv_titles;
        m_numBDJTitles = discinfo->num_bdj_titles;
        m_numUnsupportedTitles = discinfo->num_unsupported_titles;
        m_aacsDetected = (discinfo->aacs_detected != 0U);
        m_libaacsDetected = (discinfo->libaacs_detected != 0U);
        m_aacsHandled = (discinfo->aacs_handled != 0U);
        m_bdplusDetected = (discinfo->bdplus_detected != 0U);
        m_libbdplusDetected = (discinfo->libbdplus_detected != 0U);
        m_bdplusHandled = (discinfo->bdplus_handled != 0U);
    }

    return true;
}

void BlurayMetadata::toMap(InfoMap &metadataMap)
{
    metadataMap["title"] = m_title;
    metadataMap["alttitle"] = m_alttitle;
    metadataMap["language"] = m_language;

    metadataMap["discnumber"] = QString::number(m_discnumber);
    metadataMap["disctotal"] = QString::number(m_disctotal);

    //: %1 and %2 are both numbers, %1 is the current position, %2 the maximum 
    metadataMap["discseries"] = QCoreApplication::translate("(Common)", 
                                                            "%1 of %2")
                                                            .arg(m_discnumber)
                                                            .arg(m_disctotal);

    metadataMap["numtitles"] = QString::number(m_titles.count());
    metadataMap["numthumbs"] = QString::number(m_images.count());
}
