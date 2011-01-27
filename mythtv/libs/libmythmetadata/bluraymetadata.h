#ifndef BLURAYMETADATA_H_
#define BLURAYMETADATA_H_

#include <QList>
#include <QPair>
#include <QString>

#include "mythimage.h"
#include "mythexp.h"
#include "bluray.h"

typedef QList< QPair < uint,QString > > BlurayTitles;

typedef QHash<QString,QString> MetadataMap;

struct meta_dl;
class MPUBLIC BlurayMetadata : public QObject
{
  public:
    BlurayMetadata(const QString path);
    ~BlurayMetadata();

    void toMap(MetadataMap &metadataMap);

    bool OpenDisc(void);
    bool IsOpen() { return m_bdnav; };
    bool ParseDisc(void);

    QString      GetTitle(void) { return m_title; };
    QString      GetAlternateTitle(void) { return m_alttitle; };
    QString      GetDiscLanguage(void) { return m_language; };

    uint         GetCurrentDiscNumber(void) { return m_discnumber; };
    uint         GetTotalDiscNumber(void) { return m_disctotal; };

    uint         GetTitleCount(void) { return m_titles.count(); };
    BlurayTitles GetTitles(void) { return m_titles; };

    uint         GetThumbnailCount(void) { return m_images.count(); };
    QStringList  GetThumbnails(void) {return m_images; };

  private:
    BLURAY              *m_bdnav;
    meta_dl             *m_metadata;

    QString              m_title;
    QString              m_alttitle;
    QString              m_language;

    uint                 m_discnumber;
    uint                 m_disctotal;

    QString              m_path;

    BlurayTitles         m_titles;
    QStringList          m_images;
};

#endif
