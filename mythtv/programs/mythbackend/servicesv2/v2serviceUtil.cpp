#include "v2serviceUtil.h"
#include "videoutils.h"


void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
                      bool          bDetails)
{
    pVideoMetadataInfo->setId(pMetadata->GetID());
    pVideoMetadataInfo->setTitle(pMetadata->GetTitle());
    pVideoMetadataInfo->setSubTitle(pMetadata->GetSubtitle());
    pVideoMetadataInfo->setTagline(pMetadata->GetTagline());
    pVideoMetadataInfo->setDirector(pMetadata->GetDirector());
    pVideoMetadataInfo->setStudio(pMetadata->GetStudio());
    pVideoMetadataInfo->setDescription(pMetadata->GetPlot());
    pVideoMetadataInfo->setCertification(pMetadata->GetRating());
    pVideoMetadataInfo->setInetref(pMetadata->GetInetRef());
    pVideoMetadataInfo->setCollectionref(pMetadata->GetCollectionRef());
    pVideoMetadataInfo->setHomePage(pMetadata->GetHomepage());
    pVideoMetadataInfo->setReleaseDate(
        QDateTime(pMetadata->GetReleaseDate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
    pVideoMetadataInfo->setAddDate(
        QDateTime(pMetadata->GetInsertdate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
    pVideoMetadataInfo->setUserRating(pMetadata->GetUserRating());
    pVideoMetadataInfo->setChildID(pMetadata->GetChildID());
    pVideoMetadataInfo->setLength(pMetadata->GetLength().count());
    pVideoMetadataInfo->setPlayCount(pMetadata->GetPlayCount());
    pVideoMetadataInfo->setSeason(pMetadata->GetSeason());
    pVideoMetadataInfo->setEpisode(pMetadata->GetEpisode());
    pVideoMetadataInfo->setParentalLevel(pMetadata->GetShowLevel());
    pVideoMetadataInfo->setVisible(pMetadata->GetBrowse());
    pVideoMetadataInfo->setWatched(pMetadata->GetWatched());
    pVideoMetadataInfo->setProcessed(pMetadata->GetProcessed());
    pVideoMetadataInfo->setContentType(ContentTypeToString(
                                       pMetadata->GetContentType()));
    pVideoMetadataInfo->setFileName(pMetadata->GetFilename());
    pVideoMetadataInfo->setHash(pMetadata->GetHash());
    pVideoMetadataInfo->setHostName(pMetadata->GetHost());
    pVideoMetadataInfo->setCoverart(pMetadata->GetCoverFile());
    pVideoMetadataInfo->setFanart(pMetadata->GetFanart());
    pVideoMetadataInfo->setBanner(pMetadata->GetBanner());
    pVideoMetadataInfo->setScreenshot(pMetadata->GetScreenshot());
    pVideoMetadataInfo->setTrailer(pMetadata->GetTrailer());

    if (bDetails)
    {
        if (!pMetadata->GetFanart().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Fanart");
            pArtInfo->setType("fanart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Fanart",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetFanart()))));
        }
        if (!pMetadata->GetCoverFile().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Coverart");
            pArtInfo->setType("coverart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Coverart",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetCoverFile()))));
        }
        if (!pMetadata->GetBanner().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Banners");
            pArtInfo->setType("banner");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Banners",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetBanner()))));
        }
        if (!pMetadata->GetScreenshot().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Screenshots");
            pArtInfo->setType("screenshot");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Screenshots",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetScreenshot()))));
        }
    }

    V2FillGenreList(pVideoMetadataInfo->Genres(), pVideoMetadataInfo->GetId());
}


void V2FillGenreList(V2GenreList* pGenreList, int videoID)
{
    if (!pGenreList)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT genre from videogenre "
                  "LEFT JOIN videometadatagenre ON videometadatagenre.idgenre = videogenre.intid "
                  "WHERE idvideo = :ID "
                  "ORDER BY genre;");
    query.bindValue(":ID",    videoID);

    if (query.exec() && query.size() > 0)
    {
        while (query.next())
        {
            V2Genre *pGenre = pGenreList->AddNewGenre();
            QString genre = query.value(0).toString();
            pGenre->setName(genre);
        }
    }
}
