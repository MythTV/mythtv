import { Component, HostListener, OnInit, ViewEncapsulation } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { RecProfile, RecProfileGroup } from 'src/app/services/interfaces/recprofile.interface';
import { SetupService } from 'src/app/services/setup.service';
import { ProfileGroupComponent } from './profile-group/profile-group.component';
import { Router } from '@angular/router';

@Component({
  selector: 'app-recording-profiles',
  templateUrl: './recording-profiles.component.html',
  styleUrls: ['./recording-profiles.component.css'],
  encapsulation: ViewEncapsulation.None,
})
export class RecordingProfilesComponent implements OnInit {

  currentTab: number = -1;
  deletedTab = -1;
  dirtyMessages: string[] = [];
  // forms: any[] = [];
  activeTab: boolean[] = [];
  readyCount = 0;
  profileGroups: ProfileGroupComponent[] = [];

  dirtyText = 'settings.common.unsaved';
  warningText = 'settings.common.warning';

  groups: RecProfileGroup[] = [];

  constructor(private captureCardService: CaptureCardService,  public router: Router,
    private translate: TranslateService, private setupService: SetupService) {
    this.setupService.setCurrentForm(null);
    this.loadGroups();
    translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
    translate.get(this.warningText).subscribe(data => this.warningText = data);
  }


  loadGroups() {
    // Get for all hosts in case they want to use delete all
    let onlyUsed = true;
    // For testing, to show all profile groups set onlyUsed to false
    // onlyUsed = false // *** Testing only ***
    this.captureCardService.GetRecProfileGroupList(0, 0, onlyUsed).subscribe(data => {
      this.groups = data.RecProfileGroupList.RecProfileGroups;
      this.readyCount++;
    })
  }

  ngOnInit(): void {
  }

  onTabOpen(e: { index: number }) {
    this.currentTab = e.index;
    this.dirtyMessages[this.currentTab] = "";
  }

  onTabClose(e: any) {
    this.showDirty();
    this.currentTab = -1;
  }

  showDirty() {
    if (this.currentTab == -1 || !this.profileGroups[this.currentTab])
      return;
    if (!(this.profileGroups[this.currentTab]).allClean())
      this.dirtyMessages[this.currentTab] = this.dirtyText;
    else
      this.dirtyMessages[this.currentTab] = "";
  }

  confirm(message?: string): Observable<boolean> {
    const confirmation = window.confirm(message);
    return of(confirmation);
  };

  canDeactivate(): Observable<boolean> | boolean {
    let allClean = true;
    this.profileGroups.forEach(entry => {
      if (!entry.allClean())
        allClean = false;
    });
    if (!allClean)
      return this.confirm(this.warningText);

    return true;
  }

  @HostListener('window:beforeunload', ['$event'])
  onWindowClose(event: any): void {
    let allClean = true;
    this.profileGroups.forEach(entry => {
      if (!entry.allClean())
        allClean = false;
    });
    if (!allClean) {
      event.preventDefault();
      event.returnValue = false;
    }
  }




}
