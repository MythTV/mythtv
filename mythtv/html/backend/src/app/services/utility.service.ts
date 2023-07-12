import { Injectable } from '@angular/core';

@Injectable({
  providedIn: 'root'
})
export class UtilityService {

  allSlashes = new RegExp(/\//g);

  constructor() { }

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
    return tWithSecs.replace(/:.. /, ' ');
  }


}
