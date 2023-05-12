import { Component, HostListener, OnInit, ViewChild } from '@angular/core';
import { Router } from '@angular/router';
import { ConfigService } from '../../../services/config.service';
import { MythService } from '../../../services/myth.service';
import { WizardData } from '../../../services/interfaces/wizarddata.interface';
import { SetupWizardService } from '../../../services/setupwizard.service';
import { MessageService } from 'primeng/api';
import { TestDBSettingsRequest } from 'src/app/services/interfaces/myth.interface';
import { TranslateService } from '@ngx-translate/core';
import { NgForm } from '@angular/forms';
import { Observable, of } from 'rxjs';
import { SetupService } from 'src/app/services/setup.service';
import { Clipboard } from '@angular/cdk/clipboard';

@Component({
    selector: 'app-dbsetup',
    templateUrl: './dbsetup.component.html',
    styleUrls: ['./dbsetup.component.css'],
    providers: [MessageService]
})
export class DbsetupComponent implements OnInit {

    @ViewChild("databaseForm") currentForm!: NgForm;

    m_wizardData!: WizardData;

    successCount = 0;
    errorCount = 0;
    expectedCount = 2;
    connectionFail = false;
    commandlist = '';
    mySqlCommand = 'sudo mysql -u root < setup.sql'
    tzCommand = 'mysql_tzinfo_to_sql /usr/share/zoneinfo | sudo mysql -u root mysql'

    msg_testconnection = 'setupwizard.testConnection';
    msg_connectionsuccess = 'setupwizard.connectionsuccess';
    msg_connectionfail = 'setupwizard.connectionfail';
    warningText = 'settings.common.warning';


    constructor(public router: Router,
        private configService: ConfigService,
        private mythService: MythService,
        private wizardService: SetupWizardService,
        private translate: TranslateService,
        private messageService: MessageService,
        public setupService: SetupService,
        private clipboard: Clipboard) {
        this.translate.get(this.msg_testconnection).subscribe(data => this.msg_testconnection = data);
        this.translate.get(this.msg_connectionsuccess).subscribe(data => this.msg_connectionsuccess = data);
        this.translate.get(this.msg_connectionfail).subscribe(data => this.msg_connectionfail = data);
        this.translate.get(this.warningText).subscribe(data => this.warningText = data);
    }

    ngOnInit(): void {
        this.wizardService.initDatabaseStatus();
        this.m_wizardData = this.wizardService.getWizardData();
    }

    copyToclipboard(value: string): void {
        this.clipboard.copy(value);
    }

    saveObserver = {
        next: (x: any) => {
            if (x.bool) {
                this.successCount++;
            }
            else {
                this.errorCount++;
                if (this.currentForm)
                    this.currentForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.errorCount++;
            if (this.currentForm)
                this.currentForm.form.markAsDirty();
        },
    };

    saveForm(doSave: boolean) {
        this.successCount = 0;
        this.errorCount = 0;
        this.expectedCount = 1;
        this.connectionFail = false;
        const params: TestDBSettingsRequest = {
            HostName: this.m_wizardData.Database.Host,
            UserName: this.m_wizardData.Database.UserName,
            Password: this.m_wizardData.Database.Password,
            DBName: this.m_wizardData.Database.Name,
            dbPort: this.m_wizardData.Database.Port
        }
        this.commandlist = '';
        this.mythService.TestDBSettings(params).subscribe(result => {
            if (result.bool) {
                if (doSave) {
                    this.configService.SetDatabaseCredentials(this.m_wizardData.Database)
                        .subscribe(this.saveObserver);
                }
                else
                    this.messageService.add({ severity: 'success', life: 5000, summary: this.msg_testconnection, detail: this.msg_connectionsuccess });
            }
            else {
                this.messageService.add({ severity: 'error', life: 5000, summary: this.msg_testconnection, detail: this.msg_connectionfail });
                this.connectionFail = true;
                this.commandlist =
                `CREATE DATABASE IF NOT EXISTS ${this.m_wizardData.Database.Name};\n` +
                `ALTER DATABASE ${this.m_wizardData.Database.Name} DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;\n` +
                `CREATE USER IF NOT EXISTS '${this.m_wizardData.Database.UserName}'@'localhost' IDENTIFIED WITH mysql_native_password by '${this.m_wizardData.Database.Password}';\n` +
                `GRANT ALL ON ${this.m_wizardData.Database.Name}.* TO '${this.m_wizardData.Database.UserName}'@'localhost';`
            }
        });
    }

    confirm(message?: string): Observable<boolean> {
        const confirmation = window.confirm(message);
        return of(confirmation);
    };

    canDeactivate(): Observable<boolean> | boolean {
        if (this.currentForm && this.currentForm.dirty)
            return this.confirm(this.warningText);
        return true;
    }

    @HostListener('window:beforeunload', ['$event'])
    onWindowClose(event: any): void {
        if (this.currentForm && this.currentForm.dirty) {
            event.preventDefault();
            event.returnValue = false;
        }
    }
}
