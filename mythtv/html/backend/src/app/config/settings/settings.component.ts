import { Component, OnInit } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';

import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-settings',
    templateUrl: './settings.component.html',
    styleUrls: ['./settings.component.css']
})
export class SettingsComponent implements OnInit {

    m_showHelp: boolean = false;
    currentTab: number= -1;
    // This allows for up to 16 tabs
    dirtyMessages : string[] = ["","","","","","","","","","","","","","","",""];
    forms : any [] = [,,,,,,,,,,,,,,,,];
    dirtyText = 'settings.unsaved';

    constructor(private setupService: SetupService,  private translate: TranslateService) {
        translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
    }

    ngOnInit(): void {
    }

    onTabOpen(e: { index: number}) {
        this.showDirty();
        if ( typeof this.forms[e.index] == 'undefined')
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
        console.log("onTabClose");
        console.log(e);
    }

    showDirty() {
        if ( this.currentTab == -1)
            return;
        if ((<NgForm>this.forms[this.currentTab]).dirty)
            this.dirtyMessages[this.currentTab] = this.dirtyText;
        else
            this.dirtyMessages[this.currentTab] = "";
    }

    showHelp() {
        this.m_showHelp = true;
    }

    saveForm() {
        console.log("save form clicked");
    }
    testConnection() {

    }
}
