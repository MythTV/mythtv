export interface StorageGroup {
    Deleted:    number; // Boolean?
    Directory:  string;
    Expirable:  number; // Boolean?
    Free:       number;
    Id:         string;
    LiveTV:     number;
    Total:      number;
    Used:       number;
}

export interface StorageGroupDir {
    Id:         number;
    GroupName:  string;
    HostName:   string;
    DirName:    string;
    DirRead:    boolean;
    DirWrite:   boolean;
    KiBFree:    number;
}

export interface StorageGroupDirList {
    StorageGroupDirs: StorageGroupDir[];
}
