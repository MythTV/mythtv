import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { ChannelService } from 'src/app/services/channel.service';
import { FreqTableList, Grabber, GrabberList, VideoSource, VideoSourceList }
  from 'src/app/services/interfaces/videosource.interface';
import { UpdateVideoSourceRequest } from 'src/app/services/interfaces/channel.interface';
import { PartialObserver } from 'rxjs';
import { TranslateService } from '@ngx-translate/core';
import { SetupService } from 'src/app/services/setup.service';
import { Clipboard } from '@angular/cdk/clipboard';
import { BackendInfo } from 'src/app/services/interfaces/backend.interface';
import { MythService } from 'src/app/services/myth.service';

@Component({
  selector: 'app-vsource',
  templateUrl: './vsource.component.html',
  styleUrls: ['./vsource.component.css']
})
export class VsourceComponent implements OnInit, AfterViewInit {

  @Input() videoSource!: VideoSource;
  @Input() videoSourceList!: VideoSourceList;
  @ViewChild("vsourceform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  grabberList: GrabberList = {
    GrabberList: {
      Grabbers: []
    }
  }

  freqTableList: FreqTableList = {
    FreqTableList: []
  };

  backendInfo!: BackendInfo;

  work = {
    successCount: 0,
    errorCount: 0,
    errorMessage: '',
    validateError: false
  };

  messages = {
    nameInUse: 'settings.vsource.nameInUse',
    nameRequired: "settings.vsource.nameRequired",
  }

  configCommand = '';

  constructor(private channelService: ChannelService, private translate: TranslateService,
    public setupService: SetupService, private clipboard: Clipboard, private mythService: MythService) {
    translate.get(this.messages.nameInUse).subscribe(data => this.messages.nameInUse = data);
    translate.get(this.messages.nameRequired).subscribe(data => this.messages.nameRequired = data);
  }

  ngOnInit(): void {
    this.channelService.GetGrabberList()
      .subscribe(data => {
        this.grabberList = data;
      });
    this.channelService.GetFreqTableList()
      .subscribe(data => {
        this.freqTableList = data;
      });
    this.mythService.GetBackendInfo()
      .subscribe(data => {
        this.backendInfo = data;
        this.setupConf()
      });
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  copyConfigure(): void {
    let ret = this.clipboard.copy(this.configCommand);
  }


  checkName(): void {
    this.work.errorMessage = "";
    this.work.validateError = false;
    this.videoSource.SourceName = this.videoSource.SourceName.trim();
    // Check if already in use
    let match = this.videoSourceList.VideoSourceList.VideoSources.find
      (x => x.SourceName == this.videoSource.SourceName
        && x.Id != this.videoSource.Id);
    if (match) {
      this.work.errorMessage = this.messages.nameInUse;
      this.work.validateError = true;
    }
    // Check if blank
    if (this.videoSource.SourceName == "") {
      this.work.errorMessage = this.messages.nameRequired;
      this.work.validateError = true;
    }
    this.setupConf();
  }

  // Setup the configure command
  setupConf(): void {
    if (this.videoSource.Grabber == 'eitonly' || this.videoSource.Grabber == '/bin/true'
      || this.videoSource.Grabber == '' || this.videoSource.SourceName == ''
      || this.work.validateError)
      this.configCommand = '';
    else {
      let confDir = this.backendInfo.BackendInfo.Env.MYTHCONFDIR;
      if (!confDir)
        confDir = this.backendInfo.BackendInfo.Env.HOME + "/.mythtv";
      this.configCommand = "sudo -u " + this.backendInfo.BackendInfo.Env.USER + " "
        + this.videoSource.Grabber + ' --configure --config-file "'
        + confDir + '/' + this.videoSource.SourceName + '.xmltv"';
    }
  }

  // good response to add: {"int": 19}
  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.work.successCount++;
      }
      else if (!this.videoSource.Id && x.int) {
        this.work.successCount++;
        if (!this.videoSource.Id) {
          this.videoSource.Id = x.int;
        }
      }
      else {
        console.log("saveObserver error", x);
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    }
  };

  saveForm() {
    this.work.successCount = 0;
    this.work.errorCount = 0;
    if (this.videoSource.Id) {
      let req: UpdateVideoSourceRequest = <any>this.videoSource;
      req.SourceID = this.videoSource.Id;
      this.channelService.UpdateVideoSource(req)
        .subscribe(this.saveObserver);
    }
    else {
      this.channelService.AddVideoSource(this.videoSource)
        .subscribe(this.saveObserver);
    }
  }

}
