import { Component, HostListener, OnInit } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService, TranslatePipe } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { CanComponentDeactivate } from 'src/app/can-deactivate-guard.service';
import { SetupService } from 'src/app/services/setup.service';
import { Card } from 'primeng/card';
import { Accordion, AccordionTab } from 'primeng/accordion';
import { PrimeTemplate } from 'primeng/api';
import { AutoExpireComponent } from './auto-expire/auto-expire.component';
import { JobsComponent } from './jobs/jobs.component';
import { RecQualityComponent } from './rec-quality/rec-quality.component';
import { RecPrioritiesComponent } from './rec-priorities/rec-priorities.component';
import { CustomPrioritiesComponent } from './custom-priorities/custom-priorities.component';
import { ChannelGroupsComponent } from './channel-groups/channel-groups.component';
import { PlaybackGroupsComponent } from './playback-groups/playback-groups.component';
import { DataSourcesComponent } from './data-sources/data-sources.component';

@Component({
    selector: 'app-dashboard-settings',
    templateUrl: './dashboard-settings.component.html',
    styleUrls: ['./dashboard-settings.component.css'],
    standalone: true,
    imports: [Card, Accordion, AccordionTab, PrimeTemplate, AutoExpireComponent, JobsComponent, RecQualityComponent, RecPrioritiesComponent, CustomPrioritiesComponent, ChannelGroupsComponent, PlaybackGroupsComponent, DataSourcesComponent, TranslatePipe]
})
export class DashboardSettingsComponent implements OnInit, CanComponentDeactivate {

  m_showHelp: boolean = false;
  currentTab: number = -1;
  // This allows for up to 16 tabs
  dirtyMessages: string[] = ["", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""];
  forms: any[] = [, , , , , , , , , , , , , , , ,];
  dirtyText = 'settings.common.unsaved';
  warningText = 'settings.common.warning';

  constructor(private setupService: SetupService, private translate: TranslateService, public router: Router) {
      this.setupService.setCurrentForm(null);
      translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
      translate.get(this.warningText).subscribe(data => this.warningText = data);
  }

  ngOnInit(): void {
  }

  onTabOpen(e: { index: number }) {
      this.showDirty();
      if (typeof this.forms[e.index] == 'undefined')
          this.forms[e.index] = this.setupService.getCurrentForm();
      this.currentTab = e.index;
      // This line removes "Unsaved Changes" from current tab header.
      this.dirtyMessages[this.currentTab] = "";
      // This line supports showing "Unsaved Changes" on current tab header,
      // and you must comment the above line,
      // but the "Unsaved Changes" text does not go away after save, so it
      // is no good until we solve that problem.
      // (<NgForm>this.forms[e.index]).valueChanges!.subscribe(() => this.showDirty())
  }

  onTabClose(e: any) {
      this.showDirty();
  }

  showDirty() {
      if (this.currentTab == -1)
          return;
      if ((<NgForm>this.forms[this.currentTab]).dirty)
          this.dirtyMessages[this.currentTab] = this.dirtyText;
      else
          this.dirtyMessages[this.currentTab] = "";
  }

  showHelp() {
      this.m_showHelp = true;
  }

  confirm(message?: string): Observable<boolean> {
      const confirmation = window.confirm(message);
      return of(confirmation);
  };

  canDeactivate(): Observable<boolean> | boolean {
      if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
          || this.dirtyMessages.find(element => element.length > 0)) {
          return this.confirm(this.warningText);
      }
      return true;
  }

  @HostListener('window:beforeunload', ['$event'])
  onWindowClose(event: any): void {
      if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
          || this.dirtyMessages.find(element => element.length > 0)) {
          event.preventDefault();
          event.returnValue = false;
      }
  }

}
