
export interface Language {
    Code: string;
    Language: string;
    NativeLanguage?: string;
    Image?: string;
}

export interface MythLanguageList {
    LanguageList: {
        Languages: Language[];
    };
}
