import { AfterViewInit, Component, HostListener, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of, PartialObserver } from 'rxjs';
import { ChannelService } from 'src/app/services/channel.service';
import { GuideService } from 'src/app/services/guide.service';
import { Channel } from 'src/app/services/interfaces/channel.interface';
import { ChannelGroup } from 'src/app/services/interfaces/channelgroup.interface';
import { VideoSource } from 'src/app/services/interfaces/videosource.interface';
import { SetupService } from 'src/app/services/setup.service';


interface MyChannel extends Channel {
  ChanSeq?: number;
  selected?: boolean;
  inDBGroup?: boolean;
}

interface MyChannelGroup extends ChannelGroup {
  canUpdate?: boolean;
  canDelete?: boolean;
  canRename?: boolean;
}

@Component({
  selector: 'app-channel-groups',
  templateUrl: './channel-groups.component.html',
  styleUrls: ['./channel-groups.component.css']
})
export class ChannelGroupsComponent implements OnInit, AfterViewInit {

  @ViewChild("changrpform") currentForm!: NgForm;


  chanGroups: MyChannelGroup[] = [];
  allChannels: MyChannel[] = [];
  // groupChannels: MyChannel[] = [];
  videoSources: VideoSource[] = [];

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    warningText: 'settings.common.warning',
    headingNew: "dashboard.channelgroup.new",
    headingEdit: "dashboard.channelgroup.edit",
  }

  dialogHeader = "";
  oldName = '';
  displayGroupDlg = false;
  displayUnsaved = false;
  displayDelete = false;
  // showChannels = false;
  loaded = false;
  // operation: 0 = update, 1 = new, -1 = delete, 2 = add/remove channel
  operation = 0;
  successCount = 0;
  errorCount = 0;
  chanResponses = 0;
  dupName = false;
  authorization = '';

  group: MyChannelGroup = this.resetGroup();

  constructor(private channelService: ChannelService, private guideService: GuideService,
    private translate: TranslateService, private setupService: SetupService ) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadGroups();
    this.loadAllChannels();
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  loadTranslations(): void {
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }
  }


  loadGroups() {
    this.guideService.GetChannelGroupList(true).subscribe(data => {
      this.chanGroups = data.ChannelGroupList.ChannelGroups;
      this.loadSources();
    });
  }

  loadSources() {
    this.channelService.GetVideoSourceList()
      .subscribe(data => {
        this.videoSources = data.VideoSourceList.VideoSources;
        this.chanGroups.forEach((entry) => {
          if (entry.GroupId == 1) {
            entry.canDelete = false;
            entry.canUpdate = true;
            entry.canRename = false;
          }
          else if (entry.Name == 'Priority'
            || this.videoSources.find((vs) => vs.SourceName == entry.Name)) {
            entry.canDelete = false;
            entry.canUpdate = false;
            entry.canRename = false;
          }
          else {
            entry.canDelete = true;
            entry.canUpdate = true;
            entry.canRename = true;
          }
        });
        this.loaded = true;
      });
  }

  loadAllChannels() {
    let accessToken = sessionStorage.getItem('accessToken');
    if (accessToken == null)
      this.authorization = ''
    else
      this.authorization = '&authorization=' + accessToken;
    this.channelService.GetChannelInfoList(
      { OnlyVisible: true }).subscribe((data) => {
        this.allChannels = data.ChannelInfoList.ChannelInfos;
      });
  }

  resetGroup(): MyChannelGroup {
    return { Name: '', GroupId: 0, canUpdate: true, canRename: true };
  }

  resetChannels() {
    this.allChannels.forEach((entry, index) => {
      entry.ChanSeq = index;
      entry.selected = false;
      entry.inDBGroup = false;
    });
  }


  openNew() {
    this.group = this.resetGroup();
    this.resetChannels();
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.msg.headingNew;
    this.displayGroupDlg = true;
    this.operation = 1;
    this.chanResponses = 0;
  }

  editGroup(group: ChannelGroup) {
    this.group = Object.assign({}, group);
    this.oldName = group.Name;
    this.dialogHeader = this.msg.headingEdit;
    this.operation = 0;
    this.chanResponses = 0;
    this.dupName = false;

    this.resetChannels();
    let ix = 0;
    this.displayGroupDlg = true;
    this.channelService.GetChannelInfoList(
      { ChannelGroupID: group.GroupId }).subscribe((data) => {
        data.ChannelInfoList.ChannelInfos.forEach((chan) => {
          for (; this.allChannels[ix].ChanId != chan.ChanId; ix++) {
            if (ix >= this.allChannels.length) {
              console.log("ERROR: Channel list mismatch");
              return;
            }
          }
          this.allChannels[ix].inDBGroup = true;
          this.allChannels[ix].selected = true;
        });
        this.displayGroupDlg = true;
      });
  }

  checkName() {
    this.dupName = false;
    this.group.Name.trim();
    if (this.oldName.localeCompare(this.group.Name) == 0)
      return;
    let match = this.chanGroups.find((ent) =>
      this.group.Name.localeCompare(ent.Name) == 0);
    if (match)
      this.dupName = true;
  }

  deleteRequest(group: ChannelGroup) {
    this.group = group;
    this.displayDelete = true;
  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayGroupDlg && !this.displayUnsaved) {
        // Close on the edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
        return;
      }
    }
    // Close on the edit dialog
    this.currentForm.form.markAsPristine();
    this.displayGroupDlg = false;
    this.displayUnsaved = false;
    this.successCount = 0;
    this.errorCount = 0;
    this.operation = 0;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool || x.int) {
        this.successCount++;
        switch (this.operation) {
          case 0:
            // update
            this.updateChannels(true);
            break;
          case 1:
            // add
            this.group.GroupId = x.int;
            this.updateChannels(true);
            break;
          case -1:
            // Delete
            this.displayDelete = false;
            break;
          case 2:
            // Channel Response
            this.chanResponses--;
            break;
        }
        if (this.chanResponses == 0) {
          this.currentForm.form.markAsPristine();
          this.loadGroups();
          if (this.operation == 2)
            this.editGroup(this.group);
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
  }

  saveGroup() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;
    this.group.Name = this.group.Name.trim();
    if (this.operation == 1) {
      this.guideService.AddChannelGroup(this.group.Name).subscribe(this.saveObserver);
    }
    else if (this.oldName.localeCompare(this.group.Name) != 0) {
      this.guideService.UpdateChannelGroup(this.oldName, this.group.Name).subscribe(this.saveObserver);
    }
    else {
      this.updateChannels(false);
    }
  }

  deleteGroup(group: ChannelGroup) {
    this.successCount = 0;
    this.errorCount = 0;
    this.group = group;
    this.operation = -1;
    this.guideService.RemoveChannelGroup(this.group.Name).subscribe(this.saveObserver);
  }

  // Update the channels in a group
  updateChannels(updated: boolean) {
    this.operation = 2;
    this.allChannels.forEach((channel) => {
      if (channel.selected && !channel.inDBGroup) {
        this.guideService.AddToChannelGroup({
          ChannelGroupId: this.group.GroupId,
          ChanId: channel.ChanId
        }).subscribe(this.saveObserver);
        this.chanResponses++;
      } else if (!channel.selected && channel.inDBGroup) {
        this.guideService.RemoveFromChannelGroup({
          ChannelGroupId: this.group.GroupId,
          ChanId: channel.ChanId
        }).subscribe(this.saveObserver);
        this.chanResponses++
      }
    });
    if (!updated && !this.chanResponses) {
      // This is after no changes were made to anything, we
      // need to set this to inform user that all is ok
      this.currentForm.form.markAsPristine();
      this.successCount++;
    }
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
      return this.confirm(this.msg.warningText);
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