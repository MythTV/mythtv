export interface DownloadFileRequest {
    URL:            string;
    StorageGroup:   string;
}

export interface StorageGroupRequest {
    StorageGroup:   string;
}

export interface StorageGroupFileNameRequest {
    StorageGroup:   string;
    FileName:       string;
}

export interface GetAlbumArtRequest {
    Id:             number;
    Width:          number;
    Height:         number;
}

export interface GetImageFileRequest {
    StorageGroup:   string;
    FileName:       string;
    Width:          number;
    Height:         number;
}

export interface GetPreviewImageRequest {
    RecordedId:     number;
    ChanId:         number;
    StartTime:      string; // dateTime
    Width:          number;
    Height:         number;
    SecsIn:         number;
    Format:         string;
}

export interface GetProgramArtworkListRequest {
    Inetref:        string;
    Season:         number;
}

export interface GetRecordingRequest {
    RecordedId:     number;
    ChanId:         number;
    StartTime:      string; // dateTime
}

export interface GetRecordingArtworkRequest {
    Type:           string;
    Inetref:        string;
    Season:         number;
    Width:          number;
    Height:         number;
}

export interface GetRecordingArtworkListRequest {
    RecordedId:     number;
    ChanId:         number;
    StartTime:      string; // dateTime
}

export interface GetVideoArtworkRequest {
    Type:           string;
    Id:             number;
    Width:          number;
    Height:         number;
}
export interface GetDirListResponse {
    DirList:        String[];
}

export interface GetFileListResponse {
    FileList:       String[];
}

export interface GetHashResponse {
    Hash:           string;
}
