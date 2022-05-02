import { HttpErrorResponse } from '@angular/common/http';
import { Injectable, OnInit } from '@angular/core';
import { ConfigService } from './config.service';
import { Country, MythCountryList } from './interfaces/country.interface';
import { Language, MythLanguageList } from './interfaces/language.interface';
import { GetSettingResponse, MythHostName } from './interfaces/myth.interface';
import { WizardData } from './interfaces/wizarddata.interface';
import { MythService } from './myth.service';

@Injectable({
    providedIn: 'root'
})
export class SetupWizardService implements OnInit {

    m_wizardData: WizardData = {
        Country: {
            Code: '', Country:'', NativeCountry: '', Image: ''
        },
        Language: {
            Code: '', Language:'', NativeLanguage: '', Image: ''
        },
        Database:{
            Host: 'localhost', Port: 3306, UserName: 'mythtv', Password: 'mythtv', Ping: false,
            Name: 'mythconverg', Type: 'QMYSQL', LocalHostName: 'my-unique-identifier-goes-here',
            LocalEnabled: false, DoTest: true
        },
        DatabaseStatus:{
            DatabaseStatus: {
                Host: '', Port: 0, UserName: '', Password: '', Ping: false,
                Name: '', Type: '', LocalHostName: '',
                LocalEnabled: false, Connected: false, HaveDatabase: false, SchemaVersion: 0
            }
        }
    };

    m_hostName: string;

    m_languageSetting: string;
    m_languages: Language[];

    m_countrySetting: string;
    m_countries: Country[];

    constructor(private configService: ConfigService,
                private mythService: MythService) {

        console.log("**** SetupWizard ctor called ****");

        this.m_hostName = '';
        this.m_languages = [];
        this.m_languageSetting = '';
        this.m_countries = [];
        this.m_countrySetting = '';

        this.mythService.GetHostName().subscribe((value: MythHostName) => {
            this.m_hostName = value.String;

            console.log('hostname is: ', this.m_hostName);
            // now  we can get any settings that require an hostname
         });

        this.initDatabaseStatus();

        this.configService.GetCountries().subscribe((data: MythCountryList) => {
            this.m_countries = data.CountryList.Countries;
            this.initCountry();
        });

        this.configService.GetLanguages().subscribe((data: MythLanguageList) => {
            this.m_languages = data.LanguageList.Languages;
            this.initLanguage();
        });
    }

    ngOnInit(): void {
        console.log("**** SetupWizard ngOnInit called ****");

    }

    getWizardData() {
        return this.m_wizardData;
    }

    getCountryList() {
        return this.m_countries;
    }

    getLanguageList() {
        return this.m_languages;
    }

    initDatabaseStatus() {
        console.log("SetupWizardService: initDatabase called");
        this.configService.GetDatabaseStatus().subscribe(
            result => {
                console.log("SetupWizardService: initDatabaseStatus() called", result);
                this.m_wizardData.DatabaseStatus = result;
                this.m_wizardData.Database.Host = result.DatabaseStatus.Host;
                this.m_wizardData.Database.Port = result.DatabaseStatus.Port;
                this.m_wizardData.Database.UserName = result.DatabaseStatus.UserName;
                this.m_wizardData.Database.Password = result.DatabaseStatus.Password;
                this.m_wizardData.Database.Name = result.DatabaseStatus.Name;
            },
            (err: HttpErrorResponse) => {console.log("Failed to get database status", err.statusText); }
        );
    }

    initCountry() {
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: 'Country', Default: 'US' }).subscribe((result: GetSettingResponse) => {
            this.m_wizardData.Country = this.findCountryByCode(result.String);
            console.log("Country is set to: ", this.m_wizardData.Country.Code);
        })
    }

    findCountryByCode(code: string) : Country {
        for (var x = 0; x < this.m_countries.length; x++) {
            if (this.m_countries[x].Code === code)
                return this.m_countries[x];

        }

        return this.m_countries[0];
    }

    initLanguage() {
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: 'Language', Default: 'en_US' }).subscribe((result: GetSettingResponse) => {
            this.m_wizardData.Language = this.findLanguageByCode(result.String);
            console.log("Language is set to: ", this.m_wizardData.Language.Code);
        })
    }

    findLanguageByCode(code: string) : Language {
        for (var x = 0; x < this.m_languages.length; x++) {
            if (this.m_languages[x].Code === code)
                return this.m_languages[x];

        }

        return this.m_languages[0];
    }

    updateDatabaseStatus() {
        console.log("SetupWizardService: updateDatabase called");
        this.configService.GetDatabaseStatus().subscribe(
            result => { this.m_wizardData.DatabaseStatus = result; },
            (err: HttpErrorResponse) => {console.log("Failed to get database status", err.statusText); }
        );
    }
}
