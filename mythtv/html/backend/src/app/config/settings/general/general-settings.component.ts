import { Component, HostListener, OnInit, ViewEncapsulation } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService, TranslatePipe } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { CanComponentDeactivate } from 'src/app/can-deactivate-guard.service';

import { SetupService } from 'src/app/services/setup.service';
import { Card } from 'primeng/card';
import { Accordion, AccordionTab } from 'primeng/accordion';
import { PrimeTemplate } from 'primeng/api';
import { HostAddressComponent } from './host-address/host-address.component';
import { LocaleComponent } from './locale/locale.component';
import { MiscSettingsComponent } from './misc-settings/misc-settings.component';
import { EitScannerComponent } from './eit-scanner/eit-scanner.component';
import { ShutdownWakeupComponent } from './shutdown-wakeup/shutdown-wakeup.component';
import { BackendWakeupComponent } from './backend-wakeup/backend-wakeup.component';
import { BackendControlComponent } from './backend-control/backend-control.component';
import { JobqueueBackendComponent } from './jobqueue-backend/jobqueue-backend.component';
import { JobqueueGlobalComponent } from './jobqueue-global/jobqueue-global.component';
import { JobqueueCommandsComponent } from './jobqueue-commands/jobqueue-commands.component';
import { EpgDownloadingComponent } from './epg-downloading/epg-downloading.component';
import { Button } from 'primeng/button';

@Component({
    selector: 'app-general-settings',
    templateUrl: './general-settings.component.html',
    styleUrls: ['./general-settings.component.css'],
    encapsulation: ViewEncapsulation.None,
    standalone: true,
    imports: [
        Card,
        Accordion,
        AccordionTab,
        PrimeTemplate,
        HostAddressComponent,
        LocaleComponent,
        MiscSettingsComponent,
        EitScannerComponent,
        ShutdownWakeupComponent,
        BackendWakeupComponent,
        BackendControlComponent,
        JobqueueBackendComponent,
        JobqueueGlobalComponent,
        JobqueueCommandsComponent,
        EpgDownloadingComponent,
        Button,
        TranslatePipe,
    ],
})
export class SettingsComponent implements OnInit, CanComponentDeactivate {

    m_showHelp: boolean = false;
    currentTab: number = -1;
    // This allows for up to 16 tabs
    dirtyMessages: string[] = ["", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""];
    forms: any[] = [, , , , , , , , , , , , , , , ,];
    dirtyText = 'settings.common.unsaved';
    warningText = 'settings.common.warning';

    constructor(private setupService: SetupService, private translate: TranslateService, public router: Router) {
        this.setupService.setCurrentForm(null);
        translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
        translate.get(this.warningText).subscribe(data => this.warningText = data);
    }

    ngOnInit(): void {
    }

    onTabOpen(e: { index: number }) {
        this.showDirty();
        if (typeof this.forms[e.index] == 'undefined')
            this.forms[e.index] = this.setupService.getCurrentForm();
        this.currentTab = e.index;
        console.log("onTabOpen");
        console.log(e);
        // This line removes "Unsaved Changes" from current tab header.
        this.dirtyMessages[this.currentTab] = "";
        // This line supports showing "Unsaved Changes" on current tab header,
        // and you must comment the above line,
        // but the "Unsaved Changes" text does not go away after save, so it
        // is no good until we solve that problem.
        // (<NgForm>this.forms[e.index]).valueChanges!.subscribe(() => this.showDirty())
    }

    onTabClose(e: any) {
        this.showDirty();
    }

    showDirty() {
        if (this.currentTab == -1)
            return;
        if ((<NgForm>this.forms[this.currentTab]).dirty)
            this.dirtyMessages[this.currentTab] = this.dirtyText;
        else
            this.dirtyMessages[this.currentTab] = "";
    }

    showHelp() {
        this.m_showHelp = true;
    }

    confirm(message?: string): Observable<boolean> {
        const confirmation = window.confirm(message);
        return of(confirmation);
    };

    canDeactivate(): Observable<boolean> | boolean {
        if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
            || this.dirtyMessages.find(element => element.length > 0)) {
            return this.confirm(this.warningText);
        }
        return true;
    }

    @HostListener('window:beforeunload', ['$event'])
    onWindowClose(event: any): void {
        if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
            || this.dirtyMessages.find(element => element.length > 0)) {
            event.preventDefault();
            event.returnValue = false;
        }
    }

}
