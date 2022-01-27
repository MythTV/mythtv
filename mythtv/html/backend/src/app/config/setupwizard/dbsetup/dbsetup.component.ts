import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';
import { FormBuilder, Validators } from '@angular/forms';
import { Database } from '../../../services/interfaces/myth.interface';
import { ConfigService } from '../../../services/config.service';
import { MythService } from '../../../services/myth.service';
import { WizardData } from '../../../services/interfaces/wizarddata.interface';
import { SetupWizardService } from '../../../services/setupwizard.service';
import { HttpErrorResponse } from '@angular/common/http';
import { MessageService } from 'primeng/api';

@Component({
    selector: 'app-dbsetup',
    templateUrl: './dbsetup.component.html',
    styleUrls: ['./dbsetup.component.css'],
    providers: [MessageService]
})
export class DbsetupComponent implements OnInit {

    m_wizardData!: WizardData;
    m_showHelp: boolean = false;
    

    //databaseSettings: any;

    // databaseSettings = this.formBuilder.group({
    //     ipaddress: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Host, Validators.required],
    //     port: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Port, [Validators.required, Validators.min(0), Validators.max(65536)]],
    //     user: [this.m_wizardData.DatabaseStatus.DatabaseStatus.UserName, Validators.required],
    //     password: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Password, Validators.required],
    //     dbname: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Name, Validators.required]
    // });  

    constructor(private router: Router,
        private configService: ConfigService,
        private mythService: MythService,
        private wizardService: SetupWizardService,
        private formBuilder: FormBuilder,
        private messageService: MessageService) { }
        
    ngOnInit(): void {
        //this.wizardService.initDatabaseStatus();
        this.m_wizardData = this.wizardService.getWizardData();
        //

        // this.databaseSettings = this.formBuilder.group({
        //     ipaddress: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Host, Validators.required],
        //     port: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Port, [Validators.required, Validators.min(0), Validators.max(65536)]],
        //     user: [this.m_wizardData.DatabaseStatus.DatabaseStatus.UserName, Validators.required],
        //     password: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Password, Validators.required],
        //     dbname: [this.m_wizardData.DatabaseStatus.DatabaseStatus.Name, Validators.required]
        // });  
    }

    previousPage() {
        this.router.navigate(['settings/selectlanguage']);

        return;
    }


    nextPage() {
        this.router.navigate(['settings/backendnetwork']);

        return;
    }

    showHelp() {
        this.m_showHelp = true;
    }

    saveForm() {
        console.log("save form clicked");
    }   

    testConnection() {
        console.log(this.m_wizardData.Database);
        this.configService.SetDatabaseCredentials(this.m_wizardData.Database).subscribe(
            result => {
                // we got a good return code
                console.log(result);
                this.messageService.add({severity:'success', life: 5000, summary:'Test Database Connection', detail:'Connection to database was successful'});
                this.wizardService.updateDatabaseStatus();
            },
            (err: HttpErrorResponse) => {
                // we got an error return code
                console.log("Failed to set creditals", err.statusText);
                
                this.m_wizardData.DatabaseStatus.DatabaseStatus.Connected = false;
                this.m_wizardData.DatabaseStatus.DatabaseStatus.HaveDatabase = false;
                this.m_wizardData.DatabaseStatus.DatabaseStatus.SchemaVersion = 0;    
                this.messageService.add({severity:'error', life: 5000, summary:'Test Database Connection', detail:'Connection to database failed'});
            }
        );
    }
}
