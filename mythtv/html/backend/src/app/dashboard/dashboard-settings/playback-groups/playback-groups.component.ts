import { Component, HostListener, OnInit, ViewChild } from "@angular/core";
import { NgForm } from "@angular/forms";
import { TranslateService } from "@ngx-translate/core";
import { Observable, of, PartialObserver } from "rxjs";
import { DvrService } from "src/app/services/dvr.service";
import { PlayGroup } from "src/app/services/interfaces/dvr.interface";
import { SetupService } from "src/app/services/setup.service";

@Component({
  selector: 'app-playback-groups',
  templateUrl: './playback-groups.component.html',
  styleUrls: ['./playback-groups.component.css']
})
export class PlaybackGroupsComponent implements OnInit {

  @ViewChild("groupform") currentForm!: NgForm;

  playGroups: PlayGroup[] = [];
  groupNames: string[] = [];

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    warningText: 'settings.common.warning',
    headingNew: "dashboard.playgroup.new",
    headingEdit: "dashboard.playgroup.edit",
  }

  dialogHeader = "";
  displayGroupDlg = false;
  displayUnsaved = false;
  displayDelete = false;
  loaded = false;
  // operation: 0 = update, 1 = new, -1 = delete
  operation = 0;
  successCount = 0;
  errorCount = 0;

  group: PlayGroup = this.resetGroup();

  constructor(private dvrService: DvrService, private translate: TranslateService,
    private setupService: SetupService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadGroups();
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

  loadGroups () {
    this.playGroups.length = 0;
    this.loaded = false;
    this.dvrService.GetPlayGroupList().subscribe(data => {
      this.groupNames = data.PlayGroupList;
      this.groupNames.push('Default');
      this.groupNames.forEach(groupName => {
        this.dvrService.GetPlayGroup(groupName).subscribe(group => {
            this.playGroups.push(group.PlayGroup);
            if (this.playGroups.length == this.groupNames.length)
              this.playGroups = [...this.playGroups]
              this.loaded = true;
          });
      });
    });

  }

  resetGroup() : PlayGroup {
    return {Name: '', TitleMatch: '', SkipAhead:0, SkipBack:0, Jump:0, TimeStretch: 100};
  }


  openNew() {
    this.group = this.resetGroup();
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.msg.headingNew;
    this.displayGroupDlg = true;
    this.operation = 1;
  }

  editGroup(group: PlayGroup) {
    this.group = Object.assign({}, group);
    this.successCount = 0;
    this.errorCount = 0;
    this.dialogHeader = this.msg.headingEdit;
    this.displayGroupDlg = true;
    this.operation = 0;
  }

  deleteRequest(group: PlayGroup) {
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
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        this.currentForm.form.markAsPristine();
        this.loadGroups();
        switch (this.operation) {
          case 0:
            // update
            break;
          case 1:
            // add
            this.operation = 0;
            break;
          case -1:
            // Delete
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
  }

  saveGroup() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;

    if (this.operation == 1) {
        this.dvrService.AddPlayGroup(this.group).subscribe(this.saveObserver);
    }
    else {
      this.dvrService.UpdatePlayGroup(this.group).subscribe(this.saveObserver);
    }
  }

  deleteGroup(group: PlayGroup) {
    this.successCount = 0;
    this.errorCount = 0;
    this.group = group;
    this.operation = -1;
    if (group.Name != 'Default')
      this.dvrService.removePlayGroup(group.Name).subscribe(this.saveObserver);
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
