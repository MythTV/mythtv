import { Component, HostListener, OnInit } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-capture-cards',
  templateUrl: './capture-cards.component.html',
  styleUrls: ['./capture-cards.component.css']
})
export class CaptureCardsComponent implements OnInit {

  currentTab: number = -1;
  // This allows for up to 16 tabs
  dirtyMessages: string[] = [];
  forms: any[] = [];
  dirtyText = 'settings.unsaved';
  warningText = 'settings.warning';

  m_hostName: string = ""; // hostname of the backend server
  m_CaptureCardList!: CaptureCardList;
  m_CaptureCardsFiltered!: CardAndInput[];
  m_CaptureCardList$!: Observable<CaptureCardList>;

  constructor(private mythService: MythService,
    private captureCardService: CaptureCardService, private setupService: SetupService,
    private translate: TranslateService) {
    this.mythService.GetHostName().subscribe(data => {
      this.m_hostName = data.String;
      this.m_CaptureCardList$ = captureCardService.GetCaptureCardList(this.m_hostName, '')
      this.m_CaptureCardList$.subscribe(data => {
        this.m_CaptureCardList = data;
        this.m_CaptureCardsFiltered
          = this.m_CaptureCardList.CaptureCardList.CaptureCards.filter(x => x.ParentId == 0);
        for (let x = 0; x < this.m_CaptureCardsFiltered.length; x++) {
          this.dirtyMessages.push('');
          this.forms.push();
        }
      })
      translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
      translate.get(this.warningText).subscribe(data => this.warningText = data);
    });
  }

  ngOnInit(): void {
  }

  onTabOpen(e: { index: number }) {
    this.showDirty();
    if (typeof this.forms[e.index] == 'undefined')
      this.forms[e.index] = this.setupService.getCurrentForm();
    this.setupService.setCurrentForm(null);
    this.currentTab = e.index;
    console.log("onTabOpen");
    console.log(e);
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
    if (this.currentTab == -1 || ! this.forms[this.currentTab])
      return;
    if ((<NgForm>this.forms[this.currentTab]).dirty)
      this.dirtyMessages[this.currentTab] = this.dirtyText;
    else
      this.dirtyMessages[this.currentTab] = "";
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
