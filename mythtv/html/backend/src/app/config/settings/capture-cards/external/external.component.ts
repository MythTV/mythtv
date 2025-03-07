import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm, FormsModule } from '@angular/forms';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';
import { Card } from 'primeng/card';
import { PrimeTemplate } from 'primeng/api';
import { InputNumber } from 'primeng/inputnumber';
import { NgIf } from '@angular/common';
import { Message } from 'primeng/message';
import { Button } from 'primeng/button';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-external',
    templateUrl: './external.component.html',
    styleUrls: ['./external.component.css'],
    standalone: true,
    imports: [FormsModule, Card, PrimeTemplate, InputNumber, NgIf, Message, Button, TranslatePipe]
})
export class ExternalComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;

  @ViewChild("externalform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  work = {
    successCount: 0,
    errorCount: 0,
  };

  constructor(private captureCardService: CaptureCardService, public setupService: SetupService) { }

  ngOnInit(): void {
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  // good response to add: {"int": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.card.CardId && x.bool)
        this.work.successCount++;
      else if (!this.card.CardId && x.int) {
        this.work.successCount++;
        this.card.CardId = x.int;
      }
      else {
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.work.successCount = 0;
    this.work.errorCount = 0;
    if (this.card.CardId) {
      // Update device and child devices
      this.cardList.CaptureCardList.CaptureCards.forEach(card => {
        if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
          this.captureCardService.UpdateCaptureCard(card.CardId, 'videodevice', this.card.VideoDevice)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'channel_timeout', String(this.card.ChannelTimeout))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }


}
