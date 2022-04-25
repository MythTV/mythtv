export interface Country {
    Code:           string;
    Country:        string;
    NativeCountry:  string;
    Image:          string;
}

export interface CountryList {
    Countries:      Country[];
}
