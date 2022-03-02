#ifndef MYTHUIMETADATARESULTS_H_
#define MYTHUIMETADATARESULTS_H_

#include "libmythmetadata/metadatacommon.h"
#include "libmythmetadata/mythmetaexp.h"
#include "libmythui/mythuibuttonlist.h"

class MetadataImageDownload;

class META_PUBLIC MetadataResultsDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MetadataResultsDialog(MythScreenStack *lparent,
                          const MetadataLookupList &results);
    ~MetadataResultsDialog() override;

    bool Create() override; // MythScreenType

  signals:
    void haveResult(RefCountHandler<MetadataLookup>);

  private:
    MetadataLookupList m_results;
    MythUIButtonList      *m_resultsList   {nullptr};
    MetadataImageDownload *m_imageDownload {nullptr};

  private slots:
    void customEvent(QEvent *event) override; // MythUIType

    static void cleanCacheDir();
    void sendResult(MythUIButtonListItem* item);
};

#endif
