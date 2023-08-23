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

  constructor(private translate: TranslateService) {
    // translations
    for (const [key, value] of Object.entries(this.recTypeTrans)) {
      const label = 'recrule.' + key.replace(this.deSpacer, '');
      this.translate.get(label).subscribe(data => {
        Object.defineProperty(this.recTypeTrans, key, { value: data });
      });
    }
  }

  formatDate(date: string, innerHtml?: boolean): string {
    if (!date)
      return '';
    if (date.length == 10)
      date = date + ' 00:00';
    let x = new Date(date).toLocaleDateString();
    if (innerHtml)
      return x.replace(this.allSlashes, '/<wbr>');
    return x;
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
