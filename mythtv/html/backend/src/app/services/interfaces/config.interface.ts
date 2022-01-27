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

export interface Language {
        Code: string;
        Language: string;
        NativeLanguage?: string;
        Image?: string;
}

export interface MythLanguageList {
    LanguageList: {
        Languages: Language[]
    }
}

export interface Country {
    Code: string;
    Country: string;
    NativeCountry?: string;
    Image?: string;
}

export interface MythCountryList {
    CountryList: {
        Countries: Country[]
    }
}

