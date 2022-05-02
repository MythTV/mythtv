import { Component, OnInit } from '@angular/core';
import { Setup } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-locale',
    templateUrl: './locale.component.html',
    styleUrls: ['./locale.component.css']
})

export class LocaleComponent implements OnInit {
    m_setupData!: Setup;
    m_showHelp: boolean = false;

    m_vbiFormats: string[];

    // from frequencies.cpp line 2215
    m_FreqTables: string[];

    // from 
    m_TVFormats: string[];

    constructor(private setupService: SetupService) {

        // TODO: add Service API calls to get these
        this.m_TVFormats = [
            "NTSC",
            "NTSC-JP",
            "PAL",
            "PAL-60",
            "PAL-BG",
            "PAL-DK",
            "PAL-D",
            "PAL-I",
            "PAL-M",
            "PAL-N",
            "PAL-NC",
            "SECAM",
            "SECAM-D",
            "DECAM-DK"
        ];

        this.m_vbiFormats = [
            "None",
            "PAL teletext",
            "NTSC closed caption"
        ];

        this.m_FreqTables = [
            "us-bcast",
            "us-cable",
            "us-cable-hrc",
            "us-cable-irc",
            "japan-bcast",
            "japan-cable",
            "europe-west",
            "europe-east",
            "italy",
            "newzealand",
            "australia",
            "ireland",
            "france",
            "china-bcast",
            "southafrica",
            "argentina",
            "australia-optus",
            "singapore",
            "malaysia",
            "israel-hot-matav",
            "try-all"
            ];
    }

    ngOnInit(): void {
        this.m_setupData = this.setupService.getSetupData();
    }

    showHelp() {
        this.m_showHelp = true;
    }

    saveForm() {
        console.log("save form clicked");
        this.setupService.saveLocaleSettings();
    }
}
