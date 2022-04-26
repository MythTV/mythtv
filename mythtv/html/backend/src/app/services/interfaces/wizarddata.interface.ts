import { MythDatabaseStatus } from "./config.interface";
import { Language } from "./language.interface";
import { Country } from "./country.interface";
import { Database } from "./myth.interface";

export interface WizardData {
    Country:   Country;
    Language: Language;
    Database: Database;
    DatabaseStatus: MythDatabaseStatus;
}
