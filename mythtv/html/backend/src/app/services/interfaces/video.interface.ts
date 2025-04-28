import { ArtworkInfoList } from "./artwork.interface";
import { CastMemberList } from "./castmember.interface";

export interface Genre {
    Name: string;
}

export interface VideoMetadataInfo {
    Id: number;
    Title: string;
    SubTitle: string;
    Tagline: string;
    Director: string;
    Studio: string;
    Description: string;
    Certification: string;
    Inetref: string;
    Collectionref: number;
    HomePage: string;
    ReleaseDate: Date; // dateTime
    AddDate: string; // dateTime
    UserRating: number;
    ChildID: number;
    Length: number;
    PlayCount: number;
    Season: number;
    Episode: number;
    ParentalLevel: number;
    Visible: boolean;
    Watched: boolean;
    Processed: boolean;
    ContentType: string;
    FileName: string;
    Hash: string;
    HostName: string;
    Coverart: string;
    Fanart: string;
    Banner: string;
    Screenshot: string;
    Trailer: string;
    Artwork: ArtworkInfoList;
    Cast: CastMemberList;
    Genres: {
        GenreList: Genre[];
    }
}

export interface VideoMetadataInfoList {
    StartIndex: number;
    Count: number;
    TotalAvailable: number;
    AsOf: string; // dateTime
    Version: string;
    ProtoVer: string;
    VideoMetadataInfos: VideoMetadataInfo[];
}

export interface VideoCategoryList {
  VideoCategoryList: {
  VideoCategories: VideoCategories[];
  };
}

export interface VideoCategories {
  Id?: number;
  Name?: string;
}

export interface GetVideoListRequest {
    Folder?: string;
    Sort?: string;
    TitleRegEx?: string;
    Category?: number;
    Descending?: boolean;
    StartIndex?: number;
    Count?: number;
    CollapseSubDirs?: boolean;
}

export interface UpdateVideoMetadataRequest {
    Id: number;
    Title?: string;
    SubTitle?: string;
    TagLine?: string;
    Director?: string;
    Studio?: string;
    Plot?: string;
    Rating?: string;
    Inetref?: string;
    CollectionRef?: number;
    HomePage?: string;
    Year?: number;
    ReleaseDate?: Date;
    UserRating?: number;
    Length?: number;
    PlayCount?: number;
    Season?: number;
    Episode?: number;
    ShowLevel?: number;
    FileName?: string;
    Hash?: string;
    CoverFile?: string;
    ChildID?: number;
    Browse?: boolean;
    Watched?: boolean;
    Processed?: boolean;
    PlayCommand?: string;
    Category?: number;
    Trailer?: string;
    Host?: string;
    Screenshot?: string;
    Banner?: string;
    Fanart?: string;
    InsertDate?: Date;
    ContentType?: string;
    Genres?: string;
    Cast?: string;
    Countries?: string;
}
