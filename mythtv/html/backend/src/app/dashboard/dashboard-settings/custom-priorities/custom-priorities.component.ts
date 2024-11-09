import { Component, HostListener, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of, PartialObserver } from 'rxjs';
import { DvrService } from 'src/app/services/dvr.service';
import { PowerPriority } from 'src/app/services/interfaces/dvr.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-custom-priorities',
  templateUrl: './custom-priorities.component.html',
  styleUrls: ['./custom-priorities.component.css']
})
export class CustomPrioritiesComponent implements OnInit {

  @ViewChild("customform") currentForm!: NgForm;

  rules: PowerPriority[] = [];
  ruleNames: string[] = [];

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    warningText: 'settings.common.warning',
    headingNew: "dashboard.custom.new",
    headingEdit: "dashboard.custom.edit",
  }

  htmlRegex = new RegExp("<TITLE>|</TITLE>");

  dialogHeader = "";
  displayRuleDlg = false;
  displayUnsaved = false;
  displayDelete = false;
  loaded = false;
  // operation: 0 = update, 1 = new, -1 = delete
  operation = 0;
  successCount = 0;
  errorCount = 0;
  errorMsg = '';
  successMsg = '';

  rule: PowerPriority = this.resetRule();

  constructor(private dvrService: DvrService, private translate: TranslateService,
    private setupService: SetupService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadPowerPriorities();
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

  loadPowerPriorities() {
    this.rules.length = 0;
    this.ruleNames.length = 0;
    this.loaded = false;
    this.dvrService.GetPowerPriorityList().subscribe(data => {
      this.rules = data.PowerPriorityList.PowerPriorities;
      this.rules.forEach(entry => {
        this.ruleNames.push(entry.PriorityName);
      });
      this.loaded = true;
    });

  }

  resetRule(): PowerPriority {
    return { PriorityName: '', RecPriority: 1, SelectClause: '' };
  }


  openNew() {
    this.rule = this.resetRule();
    this.successCount = 0;
    this.errorCount = 0;
    this.errorMsg = '';
    this.successMsg = '';
    this.dialogHeader = this.msg.headingNew;
    this.displayRuleDlg = true;
    this.operation = 1;
  }

  validate(sql: string) {
    this.successCount = 0;
    this.errorCount = 0;
    this.errorMsg = '';
    this.successMsg = '';

    this.dvrService.CheckPowerQuery(sql).subscribe(data => {
      this.errorMsg = data.String;
      if (! this.errorMsg)
        this.successMsg = this.msg.Success;
    });
  }


  editRule(rule: PowerPriority) {
    this.rule = Object.assign({}, rule);
    this.successCount = 0;
    this.errorCount = 0;
    this.errorMsg = '';
    this.successMsg = '';
    this.dialogHeader = this.msg.headingEdit;
    this.displayRuleDlg = true;
    this.operation = 0;
  }

  deleteRequest(rule: PowerPriority) {
    this.rule = rule;
    this.displayDelete = true;
  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayRuleDlg && !this.displayUnsaved) {
        // Close on the edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
        return;
      }
    }
    // Close on the edit dialog
    this.currentForm.form.markAsPristine();
    this.displayRuleDlg = false;
    this.displayUnsaved = false;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        this.currentForm.form.markAsPristine();
        this.loadPowerPriorities();
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
      if (err.status == 400) {
        let parts = (<string>err.error).split(this.htmlRegex);
        if (parts.length > 1)
          this.errorMsg = parts[1];
      }

      this.errorCount++;
    }
  }

  saveRule() {
    this.successCount = 0;
    this.errorCount = 0;
    this.errorMsg = '';
    this.successMsg = '';

    this.displayUnsaved = false;

    if (this.operation == 1) {
      this.dvrService.AddPowerPriority(this.rule).subscribe(this.saveObserver);
    }
    else {
      this.dvrService.UpdatePowerPriority(this.rule).subscribe(this.saveObserver);
    }
  }

  deleteRule(rule: PowerPriority) {
    this.successCount = 0;
    this.errorCount = 0;
    this.errorMsg = '';
    this.successMsg = '';

    this.rule = rule;
    this.operation = -1;
    this.dvrService.RemovePowerPriority(rule.PriorityName).subscribe(this.saveObserver);
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
