import { Component, HostListener, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of, PartialObserver } from 'rxjs';
import { ChannelService } from 'src/app/services/channel.service';
import { Channel, CommMethod, DBChannelRequest } from 'src/app/services/interfaces/channel.interface';
import { VideoSource } from 'src/app/services/interfaces/videosource.interface';

@Component({
  selector: 'app-channel-editor',
  templateUrl: './channel-editor.component.html',
  styleUrls: ['./channel-editor.component.css']
})

export class ChannelEditorComponent implements OnInit {

  @ViewChild("chanform") currentForm!: NgForm;

  allChannels: Channel[] = [];
  videoSources: VideoSource[] = [];
  commMethods: CommMethod[] = [];

  tvFormats = [
    { value: "Default", prompt: "common.default" },
    { value: "NTSC", prompt: "NTSC" },
    { value: "NTSC-JP", prompt: "NTSC-JP" },
    { value: "PAL", prompt: "PAL" },
    { value: "PAL-60", prompt: "PAL-60" },
    { value: "PAL-BG", prompt: "PAL-BG" },
    { value: "PAL-DK", prompt: "PAL-DK" },
    { value: "PAL-D", prompt: "PAL-D" },
    { value: "PAL-I", prompt: "PAL-I" },
    { value: "PAL-M", prompt: "PAL-M" },
    { value: "PAL-N", prompt: "PAL-N" },
    { value: "PAL-NC", prompt: "PAL-NC" },
    { value: "SECAM", prompt: "SECAM" },
    { value: "SECAM-D", prompt: "SECAM-D" },
    { value: "SECAM-DK", prompt: "SECAM-DK" }
  ];

  visibilities = [
    { value: "Always Visible", prompt: "settings.chanedit.always-visible" },
    { value: "Visible", prompt: "settings.chanedit.visible" },
    { value: "Not Visible", prompt: "settings.chanedit.not-visible" },
    { value: "Never Visible", prompt: "settings.chanedit.never-visible" }
  ];
  headingNew = "settings.chanedit.new_channel";
  headingEdit = "settings.chanedit.title";
  warningText = 'settings.warning';
  deleteText = 'settings.ru_sure';

  transDone = 0;
  numTranslations = 8;
  successCount = 0;
  errorCount = 0;

  displayChannelDlg: boolean = false;
  dialogHeader = "";
  displayUnsaved = false;
  displayDelete = false;

  channel: Channel = this.resetChannel();
  editingChannel?: Channel;
  // channelOperation -1 = delete, 0 = update, 1 = add
  channelOperation = 0;

  constructor(private channelService: ChannelService, private translate: TranslateService) {
    this.loadLists();
    this.loadTranslations();
  }

  ngOnInit(): void {
    this.markPristine();
  }

  resetChannel(): Channel {
    return {
      ATSCMajorChan: 0,
      ATSCMinorChan: 0,
      CallSign: '',
      ChanFilters: '',
      ChanId: 0,
      ChanNum: '',
      ChannelGroups: '',
      ChannelName: '',
      CommFree: false,
      CommMethod: -1,
      DefaultAuth: '',
      ExtendedVisible: 'Visible',
      FineTune: 0,
      Format: 'Default',
      FrequencyId: '',
      IconURL: '',
      InputId: 0,
      Inputs: '',
      MplexId: 0,
      Programs: [],
      RecPriority: 0,
      ServiceId: 0,
      ServiceType: 0,
      SourceId: 0,
      TimeOffset: 0,
      UseEIT: false,
      Visible: true,
      XMLTVID: ''
    };
  }

  loadLists() {
    this.channelService.GetChannelInfoList({ Details: true }).subscribe(data =>
      this.allChannels = data.ChannelInfoList.ChannelInfos);
    this.channelService.GetVideoSourceList().subscribe(data =>
      this.videoSources = data.VideoSourceList.VideoSources);
    this.channelService.GetCommMethodList().subscribe(data =>
      this.commMethods = data.CommMethodList.CommMethods);
  }

  loadTranslations(): void {
    this.visibilities.forEach(entry => {
      this.translate.get(entry.prompt).subscribe(data => {
        entry.prompt = data;
        this.transDone++;   // There will be 4 of these
      });
    });
    this.translate.get(this.headingNew).subscribe(data => {
      this.headingNew = data
      this.transDone++
    });
    this.translate.get(this.headingEdit).subscribe(data => {
      this.headingEdit = data
      this.transDone++
    });
    this.translate.get(this.warningText).subscribe(data => {
      this.warningText = data
      this.transDone++
    });
    this.translate.get(this.deleteText).subscribe(data => {
      this.deleteText = data
      this.transDone++
    });
    this.translate.get(this.tvFormats[0].prompt).subscribe(data => {
      this.tvFormats[0].prompt = data;
      this.transDone++
    });
  }

  getSource(channel: Channel): string {
    const ret = this.videoSources.find(element => channel.SourceId == element.Id);
    if (ret != undefined)
      return ret.SourceName;
    return ""
  }

  getVisibility(channel: Channel): string {
    const ret = this.visibilities.find(element => channel.ExtendedVisible == element.value);
    if (ret != undefined)
      return ret.prompt;
    return ""
  }

  openNew(): void {
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.headingNew;
    this.channel = this.resetChannel();
    this.displayChannelDlg = true;
    // this.currentForm.form.markAsPristine();
    this.markPristine();
  }

  editChannel(channel: Channel): void {
    this.editingChannel = channel;
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.headingEdit;
    this.channel = Object.assign({}, channel);
    this.displayChannelDlg = true;
    // this.currentForm.form.markAsPristine();
    this.markPristine();
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        console.log("saveObserver success", x);
        this.successCount++;
        this.currentForm.form.markAsPristine();
        switch (this.channelOperation) {
          case 0:
            if (this.editingChannel)
              Object.assign(this.editingChannel, this.channel);
            break;
          case 1:
            this.allChannels.push(this.channel);
            break;
          case -1:
            this.channel.ChanId = -99;
            this.displayDelete = false;
            break;
        }
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
    }
  };


  saveChannel() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;
    let request: DBChannelRequest = {
      // Note - the fields we do not update are commented so they are omitted
      // ATSCMajorChan:      this.channel.ATSCMajorChan,
      // ATSCMinorChan:      this.channel.ATSCMinorChan,
      CallSign: this.channel.CallSign,
      ChannelID: this.channel.ChanId,
      ChannelNumber: this.channel.ChanNum,
      ChannelName: this.channel.ChannelName,
      CommMethod: this.channel.CommMethod,
      // DefaultAuthority:   this.channel.DefaultAuth,
      ExtendedVisible: this.channel.ExtendedVisible,
      Format: this.channel.Format,
      FrequencyID: this.channel.FrequencyId,
      // MplexID:            this.channel.MplexId,
      RecPriority: this.channel.RecPriority,
      ServiceID: this.channel.ServiceId,
      // ServiceType:        this.channel.ServiceType,
      SourceID: this.channel.SourceId,
      TimeOffset: this.channel.TimeOffset,
      UseEIT: this.channel.UseEIT,
      XMLTVID: this.channel.XMLTVID
    };

    // ChanId == 0 means adding a new channel
    if (this.channel.ChanId == 0) {
      this.channelOperation = 1;
      this.channelService.GetAvailableChanid().subscribe(data => {
        this.channel.ChanId = data.int;
        request.ChannelID = data.int;
        this.channelService.AddDBChannel(request).subscribe(this.saveObserver);
      });
    }
    else {
      this.channelOperation = 0;
      this.channelService.UpdateDBChannel(request).subscribe(this.saveObserver);
    }
  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayUnsaved) {
        // Close on the unsaved warning
        this.displayUnsaved = false;
        this.displayChannelDlg = false;
        this.editingChannel = undefined;
        this.currentForm.form.markAsPristine();
      }
      else
        // Close on the channel edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
    }
    else {
      // Close on the channel edit dialog
      this.displayChannelDlg = false;
      this.displayUnsaved = false;
      this.editingChannel = undefined;
    };
  }

  deleteRequest(channel: Channel) {
    this.successCount = 0;
    this.errorCount = 0;
    this.channel = channel;
    this.displayDelete = true;
  }

  deleteChannel(channel: Channel) {
    this.channel = channel;
    this.channelOperation = -1;
    this.channelService.RemoveDBChannel(channel.ChanId).subscribe(this.saveObserver);
  }

  markPristine() {
    let obs = new Observable(x => {
      setTimeout(() => {
        x.next(1);
        x.complete();
      }, 100)
    });
    obs.subscribe(x =>
      this.currentForm.form.markAsPristine()
    );
  }

  // Since the dialog is modal we may not actually need this function
  confirm(message?: string): Observable<boolean> {
    const confirmation = window.confirm(message);
    return of(confirmation);
  };

  // Since the dialog is modal we may not actually need this function
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
