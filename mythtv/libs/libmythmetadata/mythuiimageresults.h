#include <QObject>

#include "mythmetaexp.h"
#include "mythscreentype.h"
#include "metadataimagehelper.h"

class MythUIButtonList;
class MetadataImageDownload;

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

