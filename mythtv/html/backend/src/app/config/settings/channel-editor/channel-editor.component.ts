import { Component, HostListener, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of, PartialObserver } from 'rxjs';
import { ChannelService } from 'src/app/services/channel.service';
import { Channel, CommMethod, DBChannelRequest } from 'src/app/services/interfaces/channel.interface';
import { VideoMultiplex } from 'src/app/services/interfaces/multiplex.interface';
import { VideoSource } from 'src/app/services/interfaces/videosource.interface';
import { SetupService } from 'src/app/services/setup.service';


interface MyChannel extends Channel {
  ChanSeq?: number;
  Source?: string;
}

@Component({
  selector: 'app-channel-editor',
  templateUrl: './channel-editor.component.html',
  styleUrls: ['./channel-editor.component.css']
})
export class ChannelEditorComponent implements OnInit {

  @ViewChild("chanform") currentForm!: NgForm;

  allChannels: MyChannel[] = [];
  videoSources: VideoSource[] = [];
  commMethods: CommMethod[] = [];
  sourceNames: string[] = [];
  multiplexes: VideoMultiplex[] = [];

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
  warningText = 'settings.common.warning';
  deleteText = 'settings.common.ru_sure';
  unassignedText = 'settings.chanedit.unassigned';

  transDone = 0;
  visDone = 0;
  numTranslations = 10;
  successCount = 0;
  errorCount = 0;
  selectedAdvanced = false;

  displayChannelDlg: boolean = false;
  dialogHeader = "";
  displayUnsaved = false;
  displayDelete = false;
  displayDeleteSource = false;
  working = false;
  chansLoaded = false;
  filterEvent = {
    filters: {
      Source: {
        matchMode: '',
        value: ''
      }
    }

  };

  channel: MyChannel = this.resetChannel();
  editingChannel?: MyChannel;
  // channelOperation -2 = delete source -1 = delete channel, 0 = update, 1 = add
  channelOperation = 0;

  constructor(private channelService: ChannelService, private translate: TranslateService,
    public setupService: SetupService, public router: Router) {

    this.translate.get(this.unassignedText).subscribe(data => {
      // this translation has to be done before loading lists
      this.unassignedText = data;
      this.transDone++
      this.loadLists();
    });

    this.loadTranslations();
  }

  ngOnInit(): void {
    this.markPristine();
  }

  resetChannel(): MyChannel {
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
      XMLTVID: '',
      ChanSeq: 0
    };
  }

  loadLists() {
    this.channelService.GetChannelInfoList({ Details: true }).subscribe(data => {
      this.allChannels = data.ChannelInfoList.ChannelInfos;
      // this.allChannels.forEach((entry,index) => entry.ChanSeq = index);
      this.chansLoaded = true;
      this.channelService.GetVideoSourceList().subscribe(data => {
        this.videoSources = data.VideoSourceList.VideoSources;
        this.videoSources.unshift(<VideoSource>{ Id: 0, SourceName: this.unassignedText });
        this.videoSources.forEach((entry) => this.sourceNames.push(entry.SourceName));
        this.allChannels.forEach((entry, index) => {
          entry.ChanSeq = index;
          entry.Source = this.getSource(entry);
        });
      });
    });
    this.channelService.GetCommMethodList().subscribe(data =>
      this.commMethods = data.CommMethodList.CommMethods);
  }

  loadMultiplexes(SourceID: number) {
    this.channelService.GetVideoMultiplexList({ SourceID: SourceID }).subscribe((data => {
      this.multiplexes = data.VideoMultiplexList.VideoMultiplexes;
    }))
  }

  loadTranslations(): void {
    this.visibilities.forEach(entry => {
      this.translate.get(entry.prompt).subscribe(data => {
        entry.prompt = data;
        this.transDone++;   // There will be 4 of these
        this.visDone++;
        if (this.visDone >= this.visibilities.length)
          // notify of change
          this.visibilities = [...this.visibilities]
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
      // notify of change
      this.tvFormats = [...this.tvFormats]
      this.transDone++
    });
  }

  getSource(channel: MyChannel): string {
    const ret = this.videoSources.find(element => channel.SourceId == element.Id);
    if (ret != undefined)
      return ret.SourceName;
    return this.unassignedText;
  }

  getVisibility(channel: MyChannel): string {
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
    this.markPristine();
  }

  editChannel(channel: MyChannel): void {
    this.editingChannel = channel;
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.headingEdit;
    this.channel = Object.assign({}, channel);
    this.displayChannelDlg = true;
    this.markPristine();
    this.loadMultiplexes(channel.SourceId);
    this.selectedAdvanced = false;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        console.log("saveObserver success", x);
        this.successCount++;
        this.currentForm.form.markAsPristine();
        switch (this.channelOperation) {
          case 0:
            if (this.editingChannel) {
              Object.assign(this.editingChannel, this.channel);
              this.editingChannel.Source = this.getSource(this.editingChannel);
            }
            break;
          case 1:
            this.allChannels.push(this.channel);
            break;
          case -1:
            // Delete channel request
            this.channel.ChanId = -99;
            this.displayDelete = false;
            this.displayDeleteSource = false;
            this.currentForm.form.markAsPristine();
            break;
          case -2:
            // Delete source request
            this.channel.ChanId = -99;
            // continue with next channel
            this.deleteSource();
            break;
        }
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
        this.working = false;
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
      this.working = false;
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
      MplexID: this.channel.MplexId,
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

  deleteRequest(channel: MyChannel) {
    this.channel = channel;
    this.displayDelete = true;
  }

  deleteChannel(channel: MyChannel, source?: boolean) {
    this.successCount = 0;
    this.errorCount = 0;
    this.channel = channel;
    if (source)
      // delete source
      this.channelOperation = -2;
    else
      // delete channel
      this.channelOperation = -1;
    console.log("Delete Channel", channel)
    this.channelService.RemoveDBChannel(channel.ChanId).subscribe(this.saveObserver);
  }

  deleteSourceRequest() {
    this.channel = this.resetChannel();
    this.displayDeleteSource = true;
  }

  deleteSource() {
    this.working = true;
    const next = this.allChannels.find(entry =>
      entry.ChanId > 0
      && (!this.filterEvent.filters.Source.value
        || this.filterEvent.filters.Source.value == entry.Source));
    if (next)
      this.deleteChannel(next, true);
    else {
      this.displayDelete = false;
      this.displayDeleteSource = false;
      this.currentForm.form.markAsPristine();
      this.working = false;
    }
  }

  onFilter(event: any) {
    this.filterEvent = event;
  }

  markPristine() {
    setTimeout(() => this.currentForm.form.markAsPristine(), 200);
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
