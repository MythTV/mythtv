import { Component, HostListener, OnInit, ViewEncapsulation } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';
import { Observable, of } from 'rxjs';
import { CanComponentDeactivate } from 'src/app/can-deactivate-guard.service';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput, CardType, CardTypeList, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

interface CardTypeExt extends CardType {
  Inactive?: boolean;
}

@Component({
  selector: 'app-capture-cards',
  templateUrl: './capture-cards.component.html',
  styleUrls: ['./capture-cards.component.css'],
  encapsulation: ViewEncapsulation.None,
})
export class CaptureCardsComponent implements OnInit, CanComponentDeactivate {

  static supportedCardTypes = [
    'CETON',
    'DVB',
    'EXTERNAL',
    'HDHOMERUN',
    'FREEBOX',
    'IMPORT',
    'DEMO',
    'V4L2ENC',
    'HDPVR',
    'SATIP',
    'VBOX',
    'FIREWIRE'
  ];

  currentTab: number = -1;
  deletedTab = -1;
  dirtyMessages: string[] = [];
  forms: any[] = [];
  disabledTab: boolean[] = [];
  activeTab: boolean[] = [];
  displayDeleteThis: boolean[] = [];
  dirtyText = 'settings.common.unsaved';
  warningText = 'settings.common.warning';
  deletedText = 'settings.common.deleted';
  newText = 'settings.common.new';

  m_hostName: string = ""; // hostname of the backend server
  m_CaptureCardList!: CaptureCardList;
  m_CaptureCardsFiltered!: CardAndInput[];
  m_CaptureCardList$!: Observable<CaptureCardList>;
  diseqcTreeList!: DiseqcTreeList;
  displayModal: boolean = false;
  selectedCardType: CardType = { CardType: "", Description: "" };
  displayDeleteAllonHost: boolean = false;
  displayDeleteAll: boolean = false;
  successCount: number = 0;
  expectedCount = 0;
  errorCount: number = 0;
  deleteAll: boolean = false;

  cardTypes!: CardTypeExt[];

  constructor(private mythService: MythService, public router: Router,
    private captureCardService: CaptureCardService, public setupService: SetupService,
    private translate: TranslateService) {
    this.setupService.setCurrentForm(null);
    this.mythService.GetHostName().subscribe(data => {
      this.m_hostName = data.String;
      this.loadCards(true);
    });
    translate.get(this.dirtyText).subscribe(data => this.dirtyText = data);
    translate.get(this.warningText).subscribe(data => this.warningText = data);
    translate.get(this.deletedText).subscribe(data => this.deletedText = data);
    translate.get(this.newText).subscribe(data => this.newText = data);

    this.captureCardService.GetCardTypeList().subscribe(data => {
      this.cardTypes = data.CardTypeList.CardTypes
      this.cardTypes.forEach(entry => {
        if (CaptureCardsComponent.supportedCardTypes.indexOf(entry.CardType) < 0)
          entry.Inactive = true;
        else
          entry.Inactive = false;
      });
    });
  }

  loadCards(doFilter: boolean) {
    // Get for all hosts in case they want to use delete all
    this.m_CaptureCardList$ = this.captureCardService.GetCaptureCardList('', '')
    this.m_CaptureCardList$.subscribe(data => {
      this.m_CaptureCardList = data;
      if (doFilter)
        this.filterCards();
    })
  }

  filterCards() {
    this.m_CaptureCardsFiltered
      = this.m_CaptureCardList.CaptureCardList.CaptureCards.filter
        (x => x.ParentId == 0 && x.HostName == this.m_hostName);
    this.dirtyMessages = [];
    this.forms = [];
    this.disabledTab = [];
    this.activeTab = [];
    this.displayDeleteThis = [];
    for (let x = 0; x < this.m_CaptureCardsFiltered.length; x++) {
      this.dirtyMessages.push('');
      this.disabledTab.push(false);
      this.activeTab.push(false);
      this.displayDeleteThis.push(false);
    }
  }

  ngOnInit(): void {
    this.loadDiseqc();
  }

  loadDiseqc() {
    // Get DiseqcTree list
    this.captureCardService.GetDiseqcTreeList()
      .subscribe({
        next: data => {
          this.diseqcTreeList = data;
        },
        error: (err: any) => {
          console.log("GetDiseqcTreeList", err);
          this.errorCount++;
        }
      })
  }

  onTabOpen(e: { index: number }) {
    // Get rid of successful delete when opening a new tab
    if (this.successCount + this.errorCount >= this.expectedCount) {
      this.errorCount = 0;
      this.successCount = 0;
      this.expectedCount = 0;
    }
    this.showDirty();
    let form = this.setupService.getCurrentForm();
    if (form != null)
      this.forms[e.index] = form;
    this.setupService.setCurrentForm(null);
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
    this.currentTab = -1;
  }

  showDirty() {
    if (this.currentTab == -1 || !this.forms[this.currentTab]
      || this.disabledTab[this.currentTab])
      return;
    if ((<NgForm>this.forms[this.currentTab]).dirty)
      this.dirtyMessages[this.currentTab] = this.dirtyText;
    else if (!this.m_CaptureCardsFiltered[this.currentTab].CardId)
      this.dirtyMessages[this.currentTab] = this.newText;
    else
      this.dirtyMessages[this.currentTab] = "";
  }

  newCard() {
    this.displayModal = false;
    let newOne: CardAndInput = <CardAndInput>{
      CardType: this.selectedCardType.CardType,
      HostName: this.m_hostName,
      ChannelTimeout: 3000,
      SignalTimeout: 1000
    };
    // Update non-standard defaults on some card types.
    switch (newOne.CardType) {
      case "EXTERNAL":
        newOne.ChannelTimeout = 20000;
        break;
      case "FREEBOX":
        newOne.VideoDevice = "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u"
        newOne.ChannelTimeout = 30000;
        break;
      case "SATIP":
        newOne.DVBDiSEqCType = 1;
    }
    for (let i = 0; i < this.activeTab.length; i++)
      this.activeTab[i] = false;
    this.dirtyMessages.push(this.newText);
    this.disabledTab.push(false);
    this.activeTab.push(false);
    this.displayDeleteThis.push(false);
    this.m_CaptureCardsFiltered.push(newOne);
    this.selectedCardType = { CardType: "", Description: "" };
  }

  delObserver = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        if (this.successCount == this.expectedCount) {
          if (this.deleteAll) {
            this.loadCards(true);
            this.deleteAll = false;
          }
          else {
            if (this.deletedTab > -1) {
              this.dirtyMessages[this.deletedTab] = this.deletedText;
              this.disabledTab[this.deletedTab] = true;
              this.activeTab[this.deletedTab] = false;
              this.deletedTab = -1;
            }
          }
        }
      }
      else {
        this.errorCount++;
        this.deletedTab = -1;
        this.deleteAll = false;
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      this.deleteAll = false;
    },
  };

  deleteThis(index: number) {
    let cardId = this.m_CaptureCardsFiltered[index].CardId;
    if (!this.deleteAll) {
      // Check if prior is finished by checking counts
      if (this.successCount + this.errorCount < this.expectedCount)
        return;
      this.errorCount = 0;
      this.successCount = 0;
      this.expectedCount = 0;
      this.displayDeleteThis[index] = false;
      // To ensure delObserver flags correct item
      this.deletedTab = index;
    }
    // delete any child cards. This only happens for a card that
    // was added before this session and had children created by the
    // input setup or automatically during recording.
    this.m_CaptureCardList.CaptureCardList.CaptureCards.forEach(card => {
      if (card.ParentId == cardId) {
        console.log("DeleteThis (parent):", card.CardId);
        this.expectedCount++;
        this.captureCardService.DeleteCaptureCard(card.CardId)
          .subscribe(this.delObserver);
      }
    });
    // Delete any diseqc tree attached to the card
    // this.deleteDiseqc(this.m_CaptureCardsFiltered[index].DiSEqCId);
    this.m_CaptureCardsFiltered[index].DiSEqCId = 0;
    // Delete this card. Needs to be separate in case this card was added
    // during this session, then it would not be in the m_CaptureCardList.
    console.log("DeleteThis:", cardId);
    this.expectedCount++;
    this.captureCardService.DeleteCaptureCard(cardId)
      .subscribe(this.delObserver);
  }

  deleteAllOnHost() {
    // Check if prior is finished by checking counts
    if (this.successCount + this.errorCount < this.expectedCount)
      return;
    this.errorCount = 0;
    this.successCount = 0;
    this.expectedCount = 0;
    this.displayDeleteAllonHost = false;
    this.deletedTab = -1;
    this.deleteAll = true;
    for (let ix = 0; ix < this.m_CaptureCardsFiltered.length; ix++) {
      if (!this.disabledTab[ix] && this.m_CaptureCardsFiltered[ix].CardId)
        this.deleteThis(ix);
    }
  }

  deleteAllOnAllHosts() {
    // Check if prior is finished by checking counts
    if (this.successCount + this.errorCount < this.expectedCount)
      return;
    this.displayDeleteAll = false;
    this.errorCount = 0;
    this.successCount = 0;
    this.expectedCount = 0;
    this.deletedTab = -1;
    this.deleteAll = true;
    this.expectedCount++;
    this.captureCardService.DeleteAllCaptureCards()
      .subscribe(this.delObserver);
  }

  confirm(message?: string): Observable<boolean> {
    const confirmation = window.confirm(message);
    return of(confirmation);
  };

  canDeactivate(): Observable<boolean> | boolean {
    let currentForm = this.setupService.getCurrentForm();
    if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
      || this.dirtyMessages.find(element => element == this.dirtyText)
      || currentForm && currentForm.dirty) {
      return this.confirm(this.warningText);
    }
    return true;
  }

  @HostListener('window:beforeunload', ['$event'])
  onWindowClose(event: any): void {
    let currentForm = this.setupService.getCurrentForm();
    if (this.forms[this.currentTab] && (<NgForm>this.forms[this.currentTab]).dirty
      || this.dirtyMessages.find(element => element == this.dirtyText)
      || currentForm && currentForm.dirty) {
      event.preventDefault();
      event.returnValue = false;
    }
  }

}
