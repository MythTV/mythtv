import { HttpErrorResponse } from '@angular/common/http';
import { Injectable, OnInit } from '@angular/core';
import { ConfigService } from './config.service';
import { Country, Language } from './interfaces/config.interface';
import { WizardData } from './interfaces/wizarddata.interface';

@Injectable({
    providedIn: 'root'
})
export class SetupWizardService implements OnInit{

    m_wizardData: WizardData = {  Country: { Code: '', Country:'', NativeCountry: '', Image: '' }, 
                                Language: { Code: '', Language:'', NativeLanguage: '', Image: '' }, 
                                Database:{ Host: 'localhost', Port: 3306, UserName: 'mythtv', Password: 'mythtv', Ping: false, 
                                         Name: 'mythconverg', Type: 'QMYSQL', LocalHostName: 'my-unique-identifier-goes-here', 
                                         LocalEnabled: false, DoTest: true
                                        },
                                DatabaseStatus:{ 
                                    DatabaseStatus: { Host: '', Port: 0, UserName: '', Password: '', Ping: false, 
                                                   Name: '', Type: '', LocalHostName: '', 
                                                   LocalEnabled: false, Connected: false, HaveDatabase: false, SchemaVersion: 0 
                                    }
                                }
    };
    
    constructor(private configService: ConfigService) {console.log("SetupWizardService: constructor called"); this.initDatabaseStatus();}

    ngOnInit(): void {
        console.log("SetupWizardService: ngOnInit called");
        this.initDatabaseStatus();
    }

    setWizardData(wizardData: any) {
        this.m_wizardData = wizardData;
    }

    getWizardData() {
        return this.m_wizardData;
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

    updateDatabaseStatus() {
        console.log("SetupWizardService: updateDatabase called");
        this.configService.GetDatabaseStatus().subscribe(
            result => { this.m_wizardData.DatabaseStatus = result; },
            (err: HttpErrorResponse) => {console.log("Failed to get database status", err.statusText); }
        );
    }
}
