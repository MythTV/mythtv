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
            ArtworkList list, const VideoArtworkType type);

    ~ImageSearchResultsDialog();

    bool Create() override; // MythScreenType
    static void cleanCacheDir();
    void customEvent(QEvent *event) override; // MythUIType

  signals:
    void haveResult(ArtworkInfo, VideoArtworkType);

  private:
    ArtworkList            m_list;
    VideoArtworkType       m_type;
    MythUIButtonList      *m_resultsList   {nullptr};
    MetadataImageDownload *m_imageDownload {nullptr};

  private slots:
    void sendResult(MythUIButtonListItem* item);
};

