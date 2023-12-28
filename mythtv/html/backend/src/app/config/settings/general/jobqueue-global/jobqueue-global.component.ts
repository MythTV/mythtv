import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-jobqueue-global',
    templateUrl: './jobqueue-global.component.html',
    styleUrls: ['./jobqueue-global.component.css']
})
export class JobqueueGlobalComponent implements OnInit, AfterViewInit {

    @ViewChild("jobqglobal")
    currentForm!: NgForm;

    successCount = 0;
    errorCount = 0;
    JobsRunOnRecordHost = false;
    AutoCommflagWhileRecording = false;
    JobQueueCommFlagCommand = "mythcommflag";
    JobQueueTranscodeCommand = "mythtranscode";
    AutoTranscodeBeforeAutoCommflag = false;
    SaveTranscoding = false;

    constructor(public setupService: SetupService, private mythService: MythService) {
        this.getJobQGlobal();
    }

    ngOnInit(): void {
    }

    ngAfterViewInit() {
        this.setupService.setCurrentForm(this.currentForm);
    }

    getJobQGlobal() {

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobsRunOnRecordHost", Default: "0" })
            .subscribe({
                next: data => this.JobsRunOnRecordHost = (data.String == '1'),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoCommflagWhileRecording", Default: "0" })
            .subscribe({
                next: data => this.AutoCommflagWhileRecording = (data.String == '1'),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobQueueCommFlagCommand", Default: "mythcommflag" })
            .subscribe({
                next: data => this.JobQueueCommFlagCommand = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobQueueTranscodeCommand", Default: "mythtranscode" })
            .subscribe({
                next: data => this.JobQueueTranscodeCommand = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoTranscodeBeforeAutoCommflag", Default: "0" })
            .subscribe({
                next: data => this.AutoTranscodeBeforeAutoCommflag = (data.String == '1'),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SaveTranscoding", Default: "0" })
            .subscribe({
                next: data => this.SaveTranscoding = (data.String == '1'),
                error: () => this.errorCount++
            });
    }

    JobQGlobalObs = {
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
            this.errorCount++;
            if (this.currentForm)
                this.currentForm.form.markAsDirty();
        },
    };

    saveForm() {
        this.successCount = 0;
        this.errorCount = 0;

        this.successCount = 0;
        this.errorCount = 0;
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "JobsRunOnRecordHost",
            Value: this.JobsRunOnRecordHost ? "1" : "0"
        }).subscribe(this.JobQGlobalObs);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "AutoCommflagWhileRecording",
            Value: this.AutoCommflagWhileRecording ? "1" : "0"
        }).subscribe(this.JobQGlobalObs);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "JobQueueCommFlagCommand",
            Value: this.JobQueueCommFlagCommand
        }).subscribe(this.JobQGlobalObs);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "JobQueueTranscodeCommand",
            Value: this.JobQueueTranscodeCommand
        }).subscribe(this.JobQGlobalObs);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "AutoTranscodeBeforeAutoCommflag",
            Value: this.AutoTranscodeBeforeAutoCommflag ? "1" : "0"
        }).subscribe(this.JobQGlobalObs);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "SaveTranscoding",
            Value: this.SaveTranscoding ? "1" : "0"
        }).subscribe(this.JobQGlobalObs);
    }

}
