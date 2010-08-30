#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <mythscreentype.h>
#include <metadata/metadatacommon.h>
#include <metadata/metadatadownload.h>
#include <metadata/metadataimagedownload.h>

class VideoMetadata;
class VideoMetadataListManager;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUISpinBox;
class MythUICheckBox;

class EditMetadataDialog : public MythScreenType
{
    Q_OBJECT

  public:
     EditMetadataDialog(MythScreenStack *lparent,
                       QString lname,
                       VideoMetadata *source_metadata,
                       const VideoMetadataListManager &cache);
    ~EditMetadataDialog();

    bool Create();
    void customEvent(QEvent *levent);

    void fillWidgets();

  protected:
    void createBusyDialog(QString title);

  signals:
    void Finished();

  public slots:
    void SaveAndExit();
    void SetTitle();
    void SetSubtitle();
    void SetTagline();
    void SetRating();
    void SetDirector();
    void SetInetRef();
    void SetHomepage();
    void SetPlot();
    void SetYear();
    void SetUserRating();
    void SetLength();
    void SetCategory(MythUIButtonListItem*);
    void SetPlayer();
    void SetSeason();
    void SetEpisode();
    void SetLevel(MythUIButtonListItem*);
    void SetChild(MythUIButtonListItem*);
    void ToggleBrowse();
    void ToggleWatched();
    void FindCoverArt();
    void FindBanner();
    void FindFanart();
    void FindScreenshot();
    void FindTrailer();
    void NewCategoryPopup();
    void AddCategory(QString category);
    void SetCoverArt(QString file);
    void SetBanner(QString file);
    void SetFanart(QString file);
    void SetScreenshot(QString file);
    void SetTrailer(QString file);
    void FindNetArt(ArtworkType type);
    void FindNetCoverArt();
    void FindNetBanner();
    void FindNetFanart();
    void FindNetScreenshot();
    void OnSearchListSelection(ArtworkInfo info,
                               ArtworkType type);

  private:
    void OnArtworkSearchDone(MetadataLookup *lookup);
    void handleDownloadedImages(MetadataLookup *lookup);

    VideoMetadata            *m_workingMetadata;
    VideoMetadata            *m_origMetadata;

    //
    //  GUI stuff
    //

    MythUITextEdit      *m_titleEdit;
    MythUITextEdit      *m_subtitleEdit;
    MythUITextEdit      *m_taglineEdit;
    MythUITextEdit      *m_playerEdit;
    MythUITextEdit      *m_ratingEdit;
    MythUITextEdit      *m_directorEdit;
    MythUITextEdit      *m_inetrefEdit;
    MythUITextEdit      *m_homepageEdit;
    MythUITextEdit      *m_plotEdit;

    MythUISpinBox       *m_seasonSpin;
    MythUISpinBox       *m_episodeSpin;
    MythUISpinBox       *m_yearSpin;
    MythUISpinBox       *m_userRatingSpin;
    MythUISpinBox       *m_lengthSpin;
    MythUIButtonList      *m_categoryList;
    MythUIButtonList      *m_levelList;
    MythUIButtonList      *m_childList;
    MythUICheckBox      *m_browseCheck;
    MythUICheckBox      *m_watchedCheck;
    MythUIButton        *m_coverartButton;
    MythUIText          *m_coverartText;
    MythUIButton        *m_screenshotButton;
    MythUIText          *m_screenshotText;
    MythUIButton        *m_bannerButton;
    MythUIText          *m_bannerText;
    MythUIButton        *m_fanartButton;
    MythUIText          *m_fanartText;
    MythUIButton        *m_trailerButton;
    MythUIText          *m_trailerText;
    MythUIButton        *m_netCoverartButton;
    MythUIButton        *m_netFanartButton;
    MythUIButton        *m_netBannerButton;
    MythUIButton        *m_netScreenshotButton;
    MythUIImage         *m_coverart;
    MythUIImage         *m_screenshot;
    MythUIImage         *m_banner;
    MythUIImage         *m_fanart;
    MythUIButton        *m_doneButton;

    //
    //  Remember video-to-play-next index number when the user is toggling
    //  child videos on and off
    //

    int cachedChildSelection;

    const VideoMetadataListManager &m_metaCache;
    MetadataDownload               *m_query;
    MetadataImageDownload          *m_imageDownload;

    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;
};

#endif
