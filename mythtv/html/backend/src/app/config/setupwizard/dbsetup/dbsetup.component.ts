import { Component, HostListener, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';
import { Router } from '@angular/router';
import { ConfigService } from '../../../services/config.service';
import { MythService } from '../../../services/myth.service';
import { WizardData } from '../../../services/interfaces/wizarddata.interface';
import { SetupWizardService } from '../../../services/setupwizard.service';
import { MessageService } from 'primeng/api';
import { Database, TestDBSettingsRequest } from 'src/app/services/interfaces/myth.interface';
import { TranslateService } from '@ngx-translate/core';
import { NgForm } from '@angular/forms';
import { Observable, of } from 'rxjs';
import { SetupService } from 'src/app/services/setup.service';
import { Clipboard } from '@angular/cdk/clipboard';

@Component({
    selector: 'app-dbsetup',
    templateUrl: './dbsetup.component.html',
    styleUrls: ['./dbsetup.component.css'],
    providers: [MessageService],
    encapsulation: ViewEncapsulation.None,
})
export class DbsetupComponent implements OnInit {

    @ViewChild("databaseForm") currentForm!: NgForm;

    m_wizardData!: WizardData;
    database!: Database;

    successCount = 0;
    errorCount = 0;
    expectedCount = 2;
    connectionFail = false;
    commandlist = '';
    mySqlCommand = 'sudo mysql -u root < setup.sql'
    mySqlTzCommand = 'mysql_tzinfo_to_sql /usr/share/zoneinfo | sudo mysql -u root mysql'
    mariaDBtzCommand = 'mariadb-tzinfo-to-sql /usr/share/zoneinfo | sudo mysql -u root mysql'
    tzCommand = ''
    dbtype = 'MySQL';

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
        // This copies the default values so the html does not fail to find
        // anything.
        this.database = Object.assign({}, this.m_wizardData.Database);
        this.wizardService.dbPromise.then(() => {
            // Once the database is read, this copies the actualt values
            this.database = Object.assign({}, this.m_wizardData.Database);
            // this.testDbName = this.database.Name;
        },
            () => this.errorCount++,
        )
        this.setCommandList();
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
            HostName: this.database.Host,
            UserName: this.database.UserName,
            Password: this.database.Password,
            DBName: this.database.Name,
            dbPort: this.database.Port
        }
        this.mythService.TestDBSettings(params).subscribe(result => {
            if (result.bool) {
                this.m_wizardData.DatabaseStatus.DatabaseStatus.Connected = true;
                if (doSave) {
                    this.configService.SetDatabaseCredentials(this.database)
                        .subscribe(this.saveObserver);
                }
                else
                    this.messageService.add({ severity: 'success', life: 5000, summary: this.msg_testconnection, detail: this.msg_connectionsuccess });
            }
            else {
                this.messageService.add({ severity: 'error', life: 5000, summary: this.msg_testconnection, detail: this.msg_connectionfail });
                this.connectionFail = true;
                this.setCommandList();
            }
        });
    }

    setCommandList() {
        let pwType = '';
        if (this.dbtype == 'MySQL') {
            // pwType = 'WITH mysql_native_password';
            this.tzCommand = this.mySqlTzCommand;
        }
        else
            this.tzCommand = this.mariaDBtzCommand;
        this.commandlist =
            `CREATE DATABASE IF NOT EXISTS ${this.database.Name};<br>` +
            `CREATE USER IF NOT EXISTS '${this.database.UserName}'@'localhost' IDENTIFIED ${pwType} by '${this.database.Password}';<br>` +
            `CREATE USER IF NOT EXISTS '${this.database.UserName}'@'%' IDENTIFIED ${pwType} by '${this.database.Password}';<br>` +
            `GRANT ALL ON ${this.database.Name}.* TO '${this.database.UserName}'@'localhost';<br>` +
            `GRANT ALL ON ${this.database.Name}.* TO '${this.database.UserName}'@'%';`
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
