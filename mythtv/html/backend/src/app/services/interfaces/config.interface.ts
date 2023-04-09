export interface MythDatabaseStatus {
    DatabaseStatus: {
        Host: string;  // can be hostname or ip address
        Port: number;  // default is 3306
        UserName: string;
        Password: string;
        Name: string;  // default is mythconverg
        Ping: boolean;
        Type: string;
        LocalEnabled: boolean;
        LocalHostName: string;

        Connected: boolean;
        HaveDatabase: boolean;
        SchemaVersion: number;
    }
}

export interface IPAddressList {
    IPAddresses: string[];
}

export interface SystemEvent {
    Key: string;
    LocalizedName: string;
    Value: string;
}

export interface SystemEventList {
    SystemEventList: {
        SystemEvents: SystemEvent[];
    }
}
