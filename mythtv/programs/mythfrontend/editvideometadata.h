#ifndef EDITVIDEOMETADATA_H_
#define EDITVIDEOMETADATA_H_

#include "libmythmetadata/metadatacommon.h"
#include "libmythmetadata/metadatadownload.h"
#include "libmythmetadata/metadataimagedownload.h"
#include "libmythui/mythscreentype.h"

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
                       const QString& lname,
                       VideoMetadata *source_metadata,
                       const VideoMetadataListManager &cache);
    ~EditMetadataDialog() override;

    bool Create() override; // MythScreenType
    void customEvent(QEvent *levent) override; // MythUIType

    void fillWidgets();

  protected:
    void createBusyDialog(const QString& title);

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
    void SetCategory(MythUIButtonListItem *item);
    void SetPlayer();
    void SetSeason();
    void SetEpisode();
    void SetLevel(MythUIButtonListItem *item);
    void SetChild(MythUIButtonListItem *item);
    void ToggleBrowse();
    void ToggleWatched();
    void FindCoverArt();
    void FindBanner();
    void FindFanart();
    void FindScreenshot();
    void FindTrailer();
    void NewCategoryPopup();
    void AddCategory(const QString& category);
    void SetCoverArt(QString file);
    void SetBanner(QString file);
    void SetFanart(QString file);
    void SetScreenshot(QString file);
    void SetTrailer(QString file);
    void FindNetArt(VideoArtworkType type);
    void FindNetCoverArt();
    void FindNetBanner();
    void FindNetFanart();
    void FindNetScreenshot();
    void OnSearchListSelection(const ArtworkInfo& info,
                               VideoArtworkType type);

  private:
    void OnArtworkSearchDone(MetadataLookup *lookup);
    void handleDownloadedImages(MetadataLookup *lookup);

    VideoMetadata     *m_workingMetadata;
    VideoMetadata     *m_origMetadata;

    //
    //  GUI stuff
    //

    MythUITextEdit   *m_titleEdit                   {nullptr};
    MythUITextEdit   *m_subtitleEdit                {nullptr};
    MythUITextEdit   *m_taglineEdit                 {nullptr};
    MythUITextEdit   *m_playerEdit                  {nullptr};
    MythUITextEdit   *m_ratingEdit                  {nullptr};
    MythUITextEdit   *m_directorEdit                {nullptr};
    MythUITextEdit   *m_inetrefEdit                 {nullptr};
    MythUITextEdit   *m_homepageEdit                {nullptr};
    MythUITextEdit   *m_plotEdit                    {nullptr};

    MythUISpinBox    *m_seasonSpin                  {nullptr};
    MythUISpinBox    *m_episodeSpin                 {nullptr};
    MythUISpinBox    *m_yearSpin                    {nullptr};
    MythUISpinBox    *m_userRatingSpin              {nullptr};
    MythUISpinBox    *m_lengthSpin                  {nullptr};
    MythUIButtonList *m_categoryList                {nullptr};
    MythUIButtonList *m_levelList                   {nullptr};
    MythUIButtonList *m_childList                   {nullptr};
    MythUICheckBox   *m_browseCheck                 {nullptr};
    MythUICheckBox   *m_watchedCheck                {nullptr};
    MythUIButton     *m_coverartButton              {nullptr};
    MythUIText       *m_coverartText                {nullptr};
    MythUIButton     *m_screenshotButton            {nullptr};
    MythUIText       *m_screenshotText              {nullptr};
    MythUIButton     *m_bannerButton                {nullptr};
    MythUIText       *m_bannerText                  {nullptr};
    MythUIButton     *m_fanartButton                {nullptr};
    MythUIText       *m_fanartText                  {nullptr};
    MythUIButton     *m_trailerButton               {nullptr};
    MythUIText       *m_trailerText                 {nullptr};
    MythUIButton     *m_netCoverartButton           {nullptr};
    MythUIButton     *m_netFanartButton             {nullptr};
    MythUIButton     *m_netBannerButton             {nullptr};
    MythUIButton     *m_netScreenshotButton         {nullptr};
    MythUIImage      *m_coverart                    {nullptr};
    MythUIImage      *m_screenshot                  {nullptr};
    MythUIImage      *m_banner                      {nullptr};
    MythUIImage      *m_fanart                      {nullptr};
    MythUIButton     *m_doneButton                  {nullptr};

    //
    //  Remember video-to-play-next index number when the user is toggling
    //  child videos on and off
    //

    int               m_cachedChildSelection        {0};

    const VideoMetadataListManager &m_metaCache;
    MetadataDownload               *m_query         {nullptr};
    MetadataImageDownload          *m_imageDownload {nullptr};

    MythUIBusyDialog               *m_busyPopup     {nullptr};
    MythScreenStack                *m_popupStack    {nullptr};
};

#endif
