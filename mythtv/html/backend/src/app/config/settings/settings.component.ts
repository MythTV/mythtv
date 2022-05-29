import { Component, OnInit } from '@angular/core';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-settings',
    templateUrl: './settings.component.html',
    styleUrls: ['./settings.component.css']
})
export class SettingsComponent implements OnInit {

    m_showHelp: boolean = false;

    constructor(private setupService: SetupService) { }

    ngOnInit(): void {
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
