import { Component, HostListener, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { ConfigService } from 'src/app/services/config.service';
import { SystemEvent, SystemEventList } from 'src/app/services/interfaces/config.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-system-events',
  templateUrl: './system-events.component.html',
  styleUrls: ['./system-events.component.css'],
  encapsulation: ViewEncapsulation.None,
})

export class SystemEventsComponent implements OnInit {

  @ViewChild("eventsform") currentForm!: NgForm;
  eventList!: SystemEventList;

  hostName = '';
  events: SystemEvent[] = [];

  successCount = 0;
  errorCount = 0;
  expectedCount = 0;

  warningText = 'settings.common.warning';

  // See mythdb.cpp. This value is used to delete a setting
  kClearSettingValue = "<clear_setting_value>";

  constructor(private configService: ConfigService, private translate: TranslateService,
    public setupService: SetupService, private mythService: MythService, public router: Router) {
    this.mythService.GetHostName().subscribe({
      next: data => {
        this.hostName = data.String;
        this.configService.GetSystemEvents().subscribe(data => {
          this.eventList = data;
          this.events = data.SystemEventList.SystemEvents;
        });
      },
      error: () => this.errorCount++
    })

    this.translate.get(this.warningText).subscribe(data => {
      this.warningText = data
    });
  }

  ngOnInit(): void {
  }

  jqbObserver = {
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
    this.expectedCount = 0;
    // const hostName = this.setupService.getHostName();

    this.events.forEach(entry => {
      let value = entry.Value.trim();
      if (value)
        // value = this.kClearSettingValue;
        this.mythService.PutSetting({
          HostName: this.hostName, Key: entry.Key,
          Value: value
        }).subscribe(this.jqbObserver);
      else
        this.mythService.DeleteSetting({
          HostName: this.hostName, Key: entry.Key
        }).subscribe(this.jqbObserver);
      this.expectedCount++;
    });

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
