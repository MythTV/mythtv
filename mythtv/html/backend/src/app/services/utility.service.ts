import { Injectable } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

interface StringAssociativeArray {
  [key: string]: string
}

@Injectable({
  providedIn: 'root'
})
export class UtilityService {

  allSlashes = new RegExp(/\//g);
  deSpacer = new RegExp(/ /g);

  recTypeTrans: StringAssociativeArray = {
    "Single Record": "",
    "Record All": "",
    "Record One": "",
    "Record Daily": "",
    "Record Weekly": "",
    "Override Recording": "",
    "Do not Record": "",
    "Recording Template": "",
    "Not Recording": ""
  };

  dayFormatter: Intl.DateTimeFormat  = new Intl.DateTimeFormat(undefined, {weekday: "short"});

  // sessionStorage or localStorage
  sortStorage: Storage = sessionStorage;

  constructor(private translate: TranslateService) {
    // translations
    for (const [key, value] of Object.entries(this.recTypeTrans)) {
      const label = 'recrule.' + key.replace(this.deSpacer, '');
      this.translate.get(label).subscribe(data => {
        Object.defineProperty(this.recTypeTrans, key, { value: data });
      });
    }
    let persistSort = localStorage.getItem('RememberSort');
    if (persistSort && persistSort == 'Y')
      this.sortStorage = localStorage;
    else
      localStorage.setItem('RememberSort','N')
  }

  formatDate(dateStr: string, innerHtml?: boolean, withDay?:boolean): string {
    if (!dateStr)
      return '';
    if (dateStr.length == 10)
      dateStr = dateStr + ' 00:00';
    const date = new Date(dateStr);
    let resp = '';
    if (withDay)
      resp = this.dayFormatter.format(date) + ' ';
    resp += date.toLocaleDateString();
    if (innerHtml)
      return resp.replace(this.allSlashes, '/<wbr>');
    return resp;
  }

  formatTime(date: string): string {
    if (!date)
      return '';
    // Get the locale specific time and remove the seconds
    const t = new Date(date);
    const tWithSecs = t.toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, '');
  }

  formatDateTime(date: string, innerHtml?: boolean) {
    return this.formatDate(date, innerHtml) + ' ' + this.formatTime(date);
  }

}
