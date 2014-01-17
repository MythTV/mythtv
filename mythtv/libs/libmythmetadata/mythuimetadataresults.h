#ifndef MYTHUIMETADATARESULTS_H_
#define MYTHUIMETADATARESULTS_H_

#include "mythcorecontext.h"
#include "mythuibuttonlist.h"

#include "metadataimagedownload.h"
#include "metadatadownload.h"
#include "metadatacommon.h"

#include "mythmetaexp.h"

class META_PUBLIC MetadataResultsDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MetadataResultsDialog(MythScreenStack *lparent,
                          const MetadataLookupList &results);
    ~MetadataResultsDialog();

    bool Create();

  signals:
    void haveResult(RefCountHandler<MetadataLookup>);

  private:
    MetadataLookupList m_results;
    MythUIButtonList  *m_resultsList;
    MetadataImageDownload *m_imageDownload;

  private slots:
    void customEvent(QEvent *event);

    void cleanCacheDir();
    void sendResult(MythUIButtonListItem* item);
};

#endif
