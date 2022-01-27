import { AfterContentInit, AfterViewInit, Component, ContentChild, ElementRef, Input, OnInit, QueryList, ViewChildren } from '@angular/core';
import { Router } from '@angular/router';
import { MythCountryList, MythLanguageList, Country, Language } from '../../../services/interfaces/config.interface';
import { ConfigService } from '../../../services/config.service';
import { WizardData } from '../../../services/interfaces/wizarddata.interface';
import { SetupWizardService } from 'src/app/services/setupwizard.service';

@Component({
    selector: 'app-selectlanguage',
    templateUrl: './selectlanguage.component.html',
    styleUrls: ['./selectlanguage.component.css']
})

export class SelectLanguageComponent implements OnInit, AfterViewInit {

    @ViewChildren("countryrow", { read: ElementRef }) countryRowElement!: QueryList<ElementRef>;
    @ViewChildren("languagerow", { read: ElementRef }) languageRowElement!: QueryList<ElementRef>;
 
    //@ContentChild("row", { read: ElementRef }) rowElement!: QueryList<ElementRef>;
    
    m_wizardData!: WizardData;

    m_countries: Country[] = [];   
    m_languages: Language[] = [];
    
    m_showHelp: boolean = false;
    
    constructor(private router: Router,
                private configService: ConfigService,
                private wizardService: SetupWizardService) { }

    ngOnInit(): void {
        this.configService.GetLanguages().subscribe((data: MythLanguageList) => this.m_languages = data.LanguageList.Languages);

        this.configService.GetCountries().subscribe((data: MythCountryList) => this.m_countries = data.CountryList.Countries);

        this.m_wizardData = this.wizardService.getWizardData();
    }

    ngAfterViewInit() : void {
        console.log("After view init called");
        console.log("Country Code: ", this.m_wizardData.Country.Code);
        console.log("Language Code: ", this.m_wizardData.Language.Code);
        console.log(this.countryRowElement);
        console.log(this.languageRowElement);
        
        if (this.m_wizardData.Country.Code != '') {
            const el = this.countryRowElement.find(r => r.nativeElement.getAttribute("id") === this.m_wizardData.Country.Code);
            if (el)
                el.nativeElement.scrollIntoView({behavior: "smooth", inline: "start", block: "start"});
            else
                console.log("Failed to find element by ID")
        }

        if (this.m_wizardData.Language.Code != '') {
            const el = this.languageRowElement.find(r => r.nativeElement.getAttribute("id") === this.m_wizardData.Language.Code);
            if (el)
                el.nativeElement.scrollIntoView({behavior: "smooth", inline: "start", block: "start"});
        }
    }

    nextPage() {
        this.router.navigate(['settings/dbsetup']);
    }

    showHelp() {
        this.m_showHelp = true;
    }

    countrySelected(event: any) {
        console.log("event: ", event)
        console.log("Country changed event: ", event.value)
    }

    languageSelected(event: any) {
        console.log("event: ", event)
        console.log("Language changed event: ", event.value)
    }
}
