export interface Country {
    Code:           string;
    Country:        string;
    NativeCountry?: string;
    Image?:         string;
}

export interface MythCountryList {
    CountryList: {
        Countries: Country[]
    }
}

export interface CountryList {
    Countries:      Country[];
}
