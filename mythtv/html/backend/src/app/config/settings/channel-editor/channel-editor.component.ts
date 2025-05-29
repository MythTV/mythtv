import { Component, HostListener, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';
import { SortMeta } from 'primeng/api';
import { Observable, of, PartialObserver } from 'rxjs';
import { ChannelService } from 'src/app/services/channel.service';
import { Channel, ChannelRestoreData, CommMethod, DBChannelRequest } from 'src/app/services/interfaces/channel.interface';
import { VideoMultiplex } from 'src/app/services/interfaces/multiplex.interface';
import { VideoSource } from 'src/app/services/interfaces/videosource.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';
import { UtilityService } from 'src/app/services/utility.service';


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
  resSources: VideoSource[] = [];
  icons: String[] = [];
  iconDir = "";
  COMM_DETECT_COMMFREE  = -2;
  authorization = '';
  sortField = 'ChanSeq';
  sortOrder = 1;

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
  noneSelected = 'common.none';

  transDone = 0;
  visDone = 0;
  numTranslations = 11;
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
  resSourceId = 0;
  resXMLTV = false;
  resVisible = false;
  resIcon = false;
  resSearchDone = false;
  resShowDialog = false;
  icondldShowDialog = false;
  icondldType = "";
  icondldMax = 0;
  icondldCount = 0;
  icondldPos = -1;
  icondldFound = 0;
  // icondldStatus -
  // 0: No Download
  // 1: Download in Progress
  // 2: Download complete
  // 3: Download terminated
  icondldStatus = 0;
  iconsrchShowDialog = false;
  iconsrchTerm = "";
  iconsrchResult: {
    name: string,
    url: string
  }[] = [];
  iconsrchSelect?: {
    name: string,
    url: string
  };

  resResult: ChannelRestoreData = {
    NumChannels: 0,
    NumXLMTVID: 0,
    NumIcon: 0,
    NumVisible: 0
  };

  channel: MyChannel = this.resetChannel();
  editingChannel?: MyChannel;
  // channelOperation:
  // -2 = delete source, -1 = delete channel, 0 = update, 1 = add, 2 = save restore data
  channelOperation = 0;

  constructor(private channelService: ChannelService, private translate: TranslateService,
  public setupService: SetupService, public router: Router, private mythService: MythService,
  private utility: UtilityService,) {

    this.translate.get(this.unassignedText).subscribe(data => {
      // this translation has to be done before loading lists
      this.unassignedText = data;
      this.transDone++
      this.loadLists();
    });

    this.loadTranslations();

    let sortField = this.utility.sortStorage.getItem('channel-editor.sortField');
    if (sortField)
      this.sortField = sortField;

    let sortOrder = this.utility.sortStorage.getItem('channel-editor.sortOrder');
    if (sortOrder)
      this.sortOrder = Number(sortOrder);
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
      Icon: '',
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
    let accessToken = sessionStorage.getItem('accessToken');
    if (accessToken == null)
      this.authorization = ''
    else
      this.authorization = '&authorization=' + accessToken;
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

  loadIcons(seticon?: string) {
    this.mythService.GetBackendInfo().subscribe(data => {
      if (data.BackendInfo.Env.MYTHCONFDIR)
        this.iconDir = data.BackendInfo.Env.MYTHCONFDIR + "/channels";
      else
        this.iconDir = data.BackendInfo.Env.HOME + "/.mythtv/channels";
      this.mythService.GetDirListing(this.iconDir, true).subscribe(data => {
        // filter out resized images
        this.icons = data.DirListing.filter((icon) => !icon.match(/\.[0-9]*x[0-9]*\./))
        // Add 'None" to start of list
        this.icons.unshift(this.noneSelected + '\t');
        if (seticon) {
          this.editingChannel!.Icon = seticon;
          this.channel.Icon = seticon;
        }
      })
    });
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
    this.translate.get(this.noneSelected).subscribe(data => {
      this.noneSelected = data
      this.transDone++
    });
  }

  onSort(sortMeta: SortMeta) {
    this.sortField = sortMeta.field;
    this.sortOrder = sortMeta.order;
    this.utility.sortStorage.setItem("channel-editor.sortField", sortMeta.field);
    this.utility.sortStorage.setItem('channel-editor.sortOrder', sortMeta.order.toString());
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
    this.loadIcons();
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
    this.loadIcons();
    this.selectedAdvanced = false;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        this.currentForm.form.markAsPristine();
        switch (this.channelOperation) {
          case 0:
            // update channel
            if (this.editingChannel) {
              Object.assign(this.editingChannel, this.channel);
              this.editingChannel.Source = this.getSource(this.editingChannel);
            }
            break;
          case 1:
            this.channel.Source = this.getSource(this.channel);
            // add channel
            this.allChannels.push(this.channel);
            // notify of change
            this.allChannels = [...this.allChannels]

            break;
          case -1:
            // Delete channel
            this.channel.ChanId = -99;
            this.displayDelete = false;
            this.displayDeleteSource = false;
            this.currentForm.form.markAsPristine();
            break;
          case -2:
            // Delete source
            this.channel.ChanId = -99;
            // continue with next channel
            this.deleteSource();
            break;
          case 2:
            // save restore data
            this.resSearchDone = false;
            this.loadLists();
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
    let icon = this.channel.Icon;
    // Icon name endiong with tab means "No Icon"
    if (icon.endsWith('\t'))
      icon = '';
    this.channel.Icon = icon;
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
      Icon: icon,
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
        this.resShowDialog = false;
        this.editingChannel = undefined;
        this.currentForm.form.markAsPristine();
        return;
      }
      else if (this.displayChannelDlg) {
        // Close on the channel edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
        return;
      }
    }
    // Close on the channel edit or restore data dialog
    this.currentForm.form.markAsPristine();
    this.displayChannelDlg = false;
    this.resShowDialog = false;
    this.displayUnsaved = false;
    this.icondldShowDialog = false;
    this.editingChannel = undefined;
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

  restoreDataRequest() {
    this.resSources = [];
    this.resSearchDone = false;
    this.allChannels.forEach((entry) => {
      if (entry.MplexId) {
        if (!this.resSources.find((x) => x.Id == entry.SourceId)) {
          const sourceEntry = this.videoSources.find((x1) => x1.Id == entry.SourceId);
          if (sourceEntry)
            this.resSources.push(sourceEntry);
        }
      }
    });
    this.resShowDialog = true;
    this.markPristine();
  }

  restoreSearch() {
    this.successCount = 0;
    this.errorCount = 0;
    this.resSearchDone = false;
    this.channelService.GetRestoreData(this.resSourceId, this.resXMLTV,
      this.resIcon, this.resVisible).subscribe((resp) => {
        this.resResult = resp.ChannelRestore;
        this.resSearchDone = true;
      });
    this.markPristine();
  }

  restoreSave() {
    this.errorCount = 0;
    this.successCount = 0;
    this.channelOperation = 2;
    this.channelService.SaveRestoreData(this.resSourceId).subscribe(this.saveObserver);
  }

  onFilter(event: any) {
    this.filterEvent = event;
  }

  downloadIconsRequest() {
    this.icondldStatus = 0;
    this.icondldShowDialog = true;
    this.loadMultiplexes(0);
  }

  startIcondld() {
    if (this.icondldStatus == 0) {
      this.icondldCount = 0;
      this.icondldPos = -1;
      this.errorCount = 0;
      this.successCount = 0;
      this.icondldFound = 0;
      if (this.icondldType == 'all') {
        this.icondldMax = this.allChannels.length;
      }
      else {
        this.icondldMax = 0;
        this.allChannels.forEach(entry => {
          if (!entry.Icon)
            this.icondldMax++;
        });
      }
    }
    this.icondldStatus = 1;
    this.nextIconDld();
  }

  nextIconDld() {
    this.icondldPos++;
    if (this.icondldPos >= this.allChannels.length) {
      this.icondldStatus = 2;
      return;
    }
    if (this.icondldType != 'all') {
      while (this.allChannels[this.icondldPos].Icon) {
        this.icondldPos++;
        if (this.icondldPos >= this.allChannels.length) {
          this.icondldStatus = 2;
          return;
        }
      }
    }
    // Process icon for one channel
    const baseUrl = 'http://services.mythtv.org/channel-icon/findmissing';
    const chan = this.allChannels[this.icondldPos];
    const mPlex = this.multiplexes.find(entry => entry.MplexId == chan.MplexId);
    let transportId = 0;
    let networkId = 0;
    if (mPlex) {
      transportId = mPlex.TransportId;
      networkId = mPlex.NetworkId;
    }
    let url = `${baseUrl}?csv="${chan.ChanId}","${chan.ChannelName}","${chan.XMLTVID}","${chan.CallSign}",`
      + `"${transportId}","${chan.ATSCMajorChan}","${chan.ATSCMinorChan}","${networkId}",`
      + `"${chan.ServiceId}"`;
    this.mythService.Proxy(url).subscribe({
      next: data => {
        let resp = data.String;
        // resp is either # on one line or
        // chanid, call sign, icon id, channel name, url, for example:
        // "10033","callsign","47168","nbcsportsnetwork",
        //       "https://github.com/picons/picons/raw/master/build-source/logos/nbcsportsnetwork.default.svg"

        this.icondldCount++;
        if (resp && resp.length > 0 && resp.charAt(0) == '"') {
          let lines = resp.split('\n');
          let result = JSON.parse('[' + lines[0] + ']');
          this.icondldFound++;
          this.channelService.CopyIconToBackend(chan.ChanId, result[4]).subscribe({
            next: data => {
              if (data.bool)
                this.successCount++;
              else
                this.errorCount++;
            },
            error: err => {
              console.log("channelService.CopyIconToBackend error", err);
              this.errorCount++;
              if (this.icondldStatus == 1)
                this.nextIconDld();
            }
          });
        }
        if (this.icondldStatus == 1)
          this.nextIconDld();
      },
      error: (err) => {
        console.log("mythService.Proxy error", err);
        this.errorCount++;
        if (this.icondldStatus == 1)
          this.nextIconDld();
      }
    });
  }

  stopIcondld() {
    this.icondldStatus = 3;
  }

  searchIconRequest() {
    this.iconsrchShowDialog = true;
    this.iconsrchResult = [];
  }

  iconSearch() {
    this.successCount = 0;
    this.errorCount = 0;
    this.iconsrchResult = [];
    const url = 'http://services.mythtv.org/channel-icon/search?s=' + this.iconsrchTerm;
    this.mythService.Proxy(url).subscribe({
      next: data => {
        let resp = data.String;
        // resp is either # on one line or
        // icon id, channel name, url, for example:
        // "13286","pbs","http://www.pbs.org/images/stations/standard/PBS.gif"
        this.successCount++;
        let lines = resp.split('\n');
        lines.forEach(entry => {
          if (entry.length > 1) {
            let result = JSON.parse('[' + entry + ']');
            this.iconsrchResult.push({
              name: result[1],
              url: result[2]
            })
          }
        });
        this.iconsrchResult = [...this.iconsrchResult];
      },
      error: (err) => {
        console.log("mythService.Proxy error", err);
        this.errorCount++;
      }
    });
  }

  iconsrchSave() {
    if (!this.iconsrchSelect)
      return;
    const url = this.iconsrchSelect.url;
    this.channelService.CopyIconToBackend(this.channel.ChanId, url).subscribe({
      next: data => {
        if (data.bool) {
          this.successCount++;
          const parts = url.split('/');
          this.loadIcons(parts[parts.length - 1]);
          this.iconsrchShowDialog = false;
        }
        else
          this.errorCount++;
      },
      error: err => {
        console.log("channelService.CopyIconToBackend error", err);
        this.errorCount++;
      }
    });
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
