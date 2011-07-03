#include <QObject>

#include "mythuibuttonlist.h"
#include "metadatacommon.h"
#include "metadataimagedownload.h"
#include "mythcorecontext.h"

#include "mythmetaexp.h"

class META_PUBLIC ImageSearchResultsDialog : public MythScreenType
{
    Q_OBJECT

  public:
    ImageSearchResultsDialog(MythScreenStack *lparent,
            const ArtworkList list, const VideoArtworkType type);

    ~ImageSearchResultsDialog();

    bool Create();
    void cleanCacheDir();
    void customEvent(QEvent *event);

  signals:
    void haveResult(ArtworkInfo, VideoArtworkType);

  private:
    ArtworkList            m_list;
    VideoArtworkType            m_type;
    MythUIButtonList      *m_resultsList;
    MetadataImageDownload *m_imageDownload;

  private slots:
    void sendResult(MythUIButtonListItem* item);
};

