import { AfterViewInit, Component, ElementRef, HostListener, OnInit, QueryList, ViewChild, ViewChildren, ViewEncapsulation } from '@angular/core';
import { Router } from '@angular/router';
import { MythCountryList, Country } from '../../../services/interfaces/country.interface';
import { MythLanguageList, Language } from "../../../services/interfaces/language.interface";
import { ConfigService } from '../../../services/config.service';
import { WizardData } from '../../../services/interfaces/wizarddata.interface';
import { SetupWizardService } from 'src/app/services/setupwizard.service';
import { NgForm } from '@angular/forms';
import { SetupService } from 'src/app/services/setup.service';
import { Observable, of } from 'rxjs';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';

@Component({
    selector: 'app-selectlanguage',
    templateUrl: './selectlanguage.component.html',
    styleUrls: ['./selectlanguage.component.css'],
    encapsulation: ViewEncapsulation.None,
})

export class SelectLanguageComponent implements OnInit, AfterViewInit {

    @ViewChildren("countryrow", { read: ElementRef }) countryRowElement!: QueryList<ElementRef>;
    @ViewChildren("languagerow", { read: ElementRef }) languageRowElement!: QueryList<ElementRef>;
    @ViewChild("langform") currentForm!: NgForm;

    m_wizardData!: WizardData;

    m_countries: Country[] = [];
    m_languages: Language[] = [];

    // isReady = false;

    successCount = 0;
    errorCount = 0;
    expectedCount = 0;

    warningText = 'settings.common.warning';


    constructor(public router: Router, private translate: TranslateService,
        public setupService: SetupService, private configService: ConfigService,
        private wizardService: SetupWizardService, private mythService: MythService) {
        this.translate.get(this.warningText).subscribe(data => {
            this.warningText = data
        });
    }

    ngOnInit(): void {
        this.configService.GetLanguages().subscribe((data: MythLanguageList) => this.m_languages = data.LanguageList.Languages);
        this.configService.GetCountries().subscribe((data: MythCountryList) => this.m_countries = data.CountryList.Countries);
        this.m_wizardData = this.wizardService.getWizardData();
        setTimeout(() =>
            this.scrollIntoView()
            , 1000);
    }

    ngAfterViewInit(): void {
    }

    scrollIntoView() {

        if (this.m_wizardData.Country.Code != '') {
            const el = this.countryRowElement.find(r => r.nativeElement.getAttribute("id") === this.m_wizardData.Country.Code);
            if (el)
                el.nativeElement.scrollIntoView({ behavior: "instant", inline: "start", block: "center" });
            else
                console.log("Failed to find element by ID")
        }

        if (this.m_wizardData.Language.Code != '') {
            const el = this.languageRowElement.find(r => r.nativeElement.getAttribute("id") === this.m_wizardData.Language.Code);
            if (el)
                el.nativeElement.scrollIntoView({ behavior: "instant", inline: "start", block: "center" });
        }

        if (this.wizardService.m_topElement != null)
            this.wizardService.m_topElement.nativeElement.scrollIntoView({ behavior: "instant", block: "start" });

    }

    saveObserver = {
        next: (x: any) => {
            if (x.bool) {
                this.successCount++;
                if (this.successCount >= this.expectedCount)
                    localStorage.setItem('Language', this.m_wizardData.Language.Code);
                this.translate.use(this.m_wizardData.Language.Code);
            }
            else {
                this.errorCount++;
                if (this.currentForm)
                    this.currentForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.errorCount++;
            if (this.currentForm)
                this.currentForm.form.markAsDirty();
        },
    };

    saveForm() {
        this.successCount = 0;
        this.errorCount = 0;
        this.expectedCount = 2;

        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: 'Country',
            Value: this.m_wizardData.Country.Code
        }).subscribe(this.saveObserver);

        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: 'Language',
            Value: this.m_wizardData.Language.Code
        }).subscribe(this.saveObserver);
    }

    confirm(message?: string): Observable<boolean> {
        const confirmation = window.confirm(message);
        return of(confirmation);
    };

    canDeactivate(): Observable<boolean> | boolean {
        if (this.currentForm && this.currentForm.dirty)
            return this.confirm(this.warningText);
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
