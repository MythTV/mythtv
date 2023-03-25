import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-locale',
    templateUrl: './locale.component.html',
    styleUrls: ['./locale.component.css']
})

export class LocaleComponent implements OnInit, AfterViewInit {

    successCount = 0;
    errorCount = 0;
    TVFormat = 'PAL';
    VbiFormat = 'None';
    FreqTable = 'us-bcast';

    @ViewChild("locale")
    currentForm!: NgForm;

    m_vbiFormats: string[];

    // from frequencies.cpp line 2215
    m_FreqTables: string[];

    // from
    m_TVFormats: string[];

    constructor(public setupService: SetupService, private mythService: MythService) {

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

        this.getLocaleData();
    }

    getLocaleData() {
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "TVFormat" })
            .subscribe({
                next: data => this.TVFormat = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "VbiFormat" })
            .subscribe({
                next: data => this.VbiFormat = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "FreqTable" })
            .subscribe({
                next: data => this.FreqTable = data.String,
                error: () => this.errorCount++
            });
    }


    ngOnInit(): void {
    }

    ngAfterViewInit() {
        this.setupService.setCurrentForm(this.currentForm);
    }

    LocaleObs = {
        next: (x: any) => {
            if (x.bool)
                this.successCount++;
            else {
                this.errorCount++;
                if (this.currentForm)
                    this.currentForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.errorCount++
            if (this.currentForm)
                this.currentForm.form.markAsDirty();
        }
    };

    saveForm() {
        this.successCount = 0;
        this.errorCount = 0;

        this.mythService.PutSetting({
            HostName: "_GLOBAL_", Key: "TVFormat",
            Value: this.TVFormat
        }).subscribe(this.LocaleObs);
        this.mythService.PutSetting({
            HostName: "_GLOBAL_", Key: "VbiFormat",
            Value: this.VbiFormat
        }).subscribe(this.LocaleObs);
        this.mythService.PutSetting({
            HostName: "_GLOBAL_", Key: "FreqTable",
            Value: this.FreqTable
        }).subscribe(this.LocaleObs);
    }
}
