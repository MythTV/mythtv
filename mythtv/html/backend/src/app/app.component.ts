import { Component } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { PrimeNGConfig } from 'primeng/api';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent {
  title = 'MythTV Backend';

  constructor(private config: PrimeNGConfig, public translate: TranslateService) {
    // this language will be used as a fallback when a translation isn't found in the current language
    translate.setDefaultLang('en_US');

    // the lang to use, if the lang isn't available, it will use the current loader to get them
    translate.use(localStorage.getItem('Language') || 'en_US');

    // load the primeng translations
    translate.get('primeng').subscribe(res => this.config.setTranslation(res));

    this.config.ripple = true;
  }
}
