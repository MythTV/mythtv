import { Country, Language, MythDatabaseStatus } from "./config.interface";
import { Database } from "./myth.interface";

export interface WizardData {
    Country:   Country;
    Language: Language;
    Database: Database;
    DatabaseStatus: MythDatabaseStatus;
}
