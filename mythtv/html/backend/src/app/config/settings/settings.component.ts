import { Component, OnInit } from '@angular/core';
import { Setup } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-settings',
    templateUrl: './settings.component.html',
    styleUrls: ['./settings.component.css']
})
export class SettingsComponent implements OnInit {

    m_setupData!: Setup;
    m_showHelp: boolean = false;

    constructor(private setupService: SetupService) { }

    ngOnInit(): void {
        this.m_setupData = this.setupService.getSetupData();
        console.log(this.m_setupData);
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
