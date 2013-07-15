// Qt headers
#include <QHash>
#include <QCoreApplication>

#include "bdnav/meta_data.h"
#include "bluraymetadata.h"
#include "mythdirs.h"

BlurayMetadata::BlurayMetadata(const QString path) :
    m_bdnav(NULL),
    m_title(QString()),          m_alttitle(QString()),
    m_language(QString()),       m_discnumber(0),
    m_disctotal(0),              m_path(path),
    m_images(QStringList()),     m_topMenuSupported(false),
    m_firstPlaySupported(false), m_numHDMVTitles(0),
    m_numBDJTitles(0),           m_numUnsupportedTitles(0),
    m_aacsDetected(false),       m_libaacsDetected(false),
    m_aacsHandled(false),        m_bdplusDetected(false),
    m_libbdplusDetected(false),  m_bdplusHandled(false)
{
}

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

    if (!m_bdnav)
        return false;

    return true;
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
                                   .arg(m_path)
                                   .arg(metadata->thumbnails[i].path);
            m_images.append(filepath);
        }
    }

    const BLURAY_DISC_INFO *discinfo = bd_get_disc_info(m_bdnav);
    if (discinfo)
    {
        m_topMenuSupported   = discinfo->top_menu_supported;
        m_firstPlaySupported = discinfo->first_play_supported;
        m_numHDMVTitles = discinfo->num_hdmv_titles;
        m_numBDJTitles = discinfo->num_bdj_titles;
        m_numUnsupportedTitles = discinfo->num_unsupported_titles;
        m_aacsDetected = discinfo->aacs_detected;
        m_libaacsDetected = discinfo->libaacs_detected;
        m_aacsHandled = discinfo->aacs_handled;
        m_bdplusDetected = discinfo->bdplus_detected;
        m_libbdplusDetected = discinfo->libbdplus_detected;
        m_bdplusHandled = discinfo->bdplus_handled;
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

    metadataMap["numtitles"] = m_titles.count();
    metadataMap["numthumbs"] = m_images.count();
}
