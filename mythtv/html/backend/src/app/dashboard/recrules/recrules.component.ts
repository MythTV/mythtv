import { Component, OnInit } from '@angular/core';
import { SortMeta } from 'primeng/api';
import { ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DvrService } from 'src/app/services/dvr.service';
import { RecRule } from 'src/app/services/interfaces/recording.interface';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-recrules',
  templateUrl: './recrules.component.html',
  styleUrls: ['./recrules.component.css']
})
export class RecrulesComponent implements OnInit, SchedulerSummary {

  recRules: RecRule[] = [];
  recRule?: RecRule;

  inter: ScheduleLink = { summaryComponent: this };


  deSpacer = new RegExp(/ /g);

  rulesLoaded = false;
  errorCount = 0;
  successCount = 0;
  displayDelete = false;
  sortField = 'Title';
  sortOrder = 1;

  constructor(private dvrService: DvrService,
    public utility: UtilityService) {

    let sortField = this.utility.sortStorage.getItem('recrules.sortField');
    if (sortField)
      this.sortField = sortField;

    let sortOrder = this.utility.sortStorage.getItem('recrules.sortOrder');
    if (sortOrder)
      this.sortOrder = Number(sortOrder);
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

  onSort(sortMeta: SortMeta) {
    this.sortField = sortMeta.field;
    this.sortOrder = sortMeta.order;
    this.utility.sortStorage.setItem("recrules.sortField", sortMeta.field);
    this.utility.sortStorage.setItem('recrules.sortOrder', sortMeta.order.toString());
  }

  newRecRule() {
    this.updateRecRule();
  }

  updateRecRule(recRule?: RecRule) {
    if (this.inter.sched)
      this.inter.sched.open(undefined, undefined, recRule);
  }

  newTemplate() {
    this.updateRecRule( <RecRule>{ Type: 'Recording Template' });
  }

  saveObserver = {
    next: (x: any) => {
      if (this.recRule) {
        if (this.recRule.Id && x.bool) {
          // Delete
          this.successCount++;
          this.displayDelete = false;
          setTimeout(() => this.refresh(), 1000);
        }
        else if (!this.recRule.Id && x.uint) {
          // Add
          this.successCount++;
          setTimeout(() => this.inter.summaryComponent.refresh(), 1000);
          this.recRule.Id = x.uint;
        }
        else {
          this.errorCount++;
        }
      }
      else {
        // Should not happen
        console.log("ERROR: recRule is undefined")
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.error(err);
      // if (err.status == 400) {
      //   let parts = (<string>err.error).split(this.htmlRegex);
      //   if (parts.length > 1)
      //     this.errortext = parts[1];
      // }
      this.errorCount++;
    },
  };


  deleteRequest(recRule: RecRule) {
    this.recRule = recRule;
    this.displayDelete = true;
  }

  deleteRule(recRule: RecRule) {
    this.dvrService.RemoveRecordSchedule(recRule.Id)
      .subscribe(this.saveObserver);
  }


}
