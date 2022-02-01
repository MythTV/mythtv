import { Component, OnInit } from '@angular/core';
import { Language, MythLanguageList } from 'src/app/services/interfaces/config.interface';
import { Theme } from 'src/app/services/interfaces/theme.interface';
import { ThemeService } from '../../services/theme.service';
import { ConfigService } from '../../services/config.service';

@Component({
    selector: 'app-navbar',
    templateUrl: './navbar.component.html',
    styleUrls: ['./navbar.component.css']
})
export class NavbarComponent implements OnInit {

    m_themes$!: Theme[];
    m_selectedTheme!: Theme;

    m_languages!: Language[];
    m_selectedLanguage!: Language;

    constructor(private themeService: ThemeService,
                private configService: ConfigService) {
        this.themeService.getThemes().then((themes: Theme[]) => {
            this.m_themes$ = themes;
            this.m_selectedTheme = this.findThemeByName(localStorage.getItem('Theme') || 'Indigo Light');
            this.themeService.switchTheme(this.m_selectedTheme.CSS);
        });

        this.configService.GetLanguages().subscribe((data: MythLanguageList) => {
            this.m_languages = data.LanguageList.Languages;
            this.m_selectedLanguage = this.findLanguageByCode(localStorage.getItem('Language') || 'en_US');
        })
    }

    ngOnInit(): void {
    }

    findThemeByName(name: string) : Theme {
        for (var x = 0; x < this.m_themes$.length; x++) {
            if (this.m_themes$[x].Name === name)
                return this.m_themes$[x];

        }

        return this.m_themes$[0];
    }

    changeTheme(theme: Theme) {
        this.m_selectedTheme = theme;
        this.themeService.switchTheme(theme.CSS);
        localStorage.setItem('Theme', this.m_selectedTheme.Name);
    }

    findLanguageByCode(code: string) : Language {
        for (var x = 0; x < this.m_languages.length; x++) {
            if (this.m_languages[x].Code === code)
                return this.m_languages[x];

        }

        return this.m_languages[0];
    }

    changeLanguage(language: Language) {
        console.log("Language changed to ", language.NativeLanguage)
        this.m_selectedLanguage = language;
        localStorage.setItem('Language', this.m_selectedLanguage.Code);
    }
}
