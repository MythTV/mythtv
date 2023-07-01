import { Component, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { ScheduleComponent, ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DvrService } from 'src/app/services/dvr.service';
import { RecRule } from 'src/app/services/interfaces/recording.interface';
import { UtilityService } from 'src/app/services/utility.service';

interface StringAssociativeArray {
  [key: string]: string
}


@Component({
  selector: 'app-recrules',
  templateUrl: './recrules.component.html',
  styleUrls: ['./recrules.component.css']
})
export class RecrulesComponent implements OnInit, SchedulerSummary {

  recRules: RecRule[] = [];

  inter: ScheduleLink = { summaryComponent: this };


  typeValue: StringAssociativeArray = {
    "Single Record": "",
    "Record All": "",
    "Record One": "",
    "Record Daily": "",
    "Record Weekly": "",
    "Override Recording": "",
    "Recording Template": "",
    "Not Recording": ""
  };

  rulesLoaded = false;
  errorCount = 0;

  constructor(private dvrService: DvrService, private translate: TranslateService,
    public utility: UtilityService) {
    // translations
    for (const [key, value] of Object.entries(this.typeValue)) {
      const label = 'recrule.' + key.replace(' ', '');
      this.translate.get(label).subscribe(data => {
        Object.defineProperty(this.typeValue, key, { value: data });
      });
    }
  }

  refresh(): void {
    this.loadLists();
  }

  ngOnInit(): void {
    this.loadLists();
  }

  loadLists() {
    this.recRules = [];
    this.errorCount = 0;

    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.recRules = data.RecRuleList.RecRules;
        this.rulesLoaded = true;
      },
      error: (err: any) => {
        this.errorCount++
      }
    });

  }

  updateRecRule(recRule: RecRule) {
    if (this.inter.sched)
      this.inter.sched.open(undefined, undefined, recRule);
  }

}
