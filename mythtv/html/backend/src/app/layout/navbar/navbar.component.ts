import { Component, isDevMode, OnInit } from '@angular/core';
import { Language, MythLanguageList } from "src/app/services/interfaces/language.interface";
import { Theme } from 'src/app/services/interfaces/theme.interface';
import { ThemeService } from '../../services/theme.service';
import { ConfigService } from '../../services/config.service';
import { TranslateService } from '@ngx-translate/core';
import { PrimeNGConfig } from 'primeng/api';
import { DataService } from 'src/app/services/data.service';
import { MythService } from 'src/app/services/myth.service';
import { Router } from '@angular/router';

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

    m_showNavbar: boolean = true;
    showTopBar = true;
    m_devMode: boolean = isDevMode();
    m_haveDatabase: boolean = true;
    userName: string | null = '';
    userPassword: string | null = '';
    displayLogin = false;
    errorCount = 0;
    APIAuthReqd = false;
    keepLogin = false;

    constructor(private themeService: ThemeService,
        private configService: ConfigService,
        private translateService: TranslateService,
        private primeconfigService: PrimeNGConfig,
        public dataService: DataService,
        private mythService: MythService,
        private router: Router) {
        this.themeService.getThemes().then((themes: Theme[]) => {
            this.m_themes$ = themes;
            this.m_selectedTheme = this.findThemeByName(localStorage.getItem('Theme') || 'Blue Light');
            this.themeService.switchTheme(this.m_selectedTheme.CSS);
        });

        this.configService.GetLanguages().subscribe((data: MythLanguageList) => {
            this.m_languages = data.LanguageList.Languages;
            this.m_selectedLanguage = this.findLanguageByCode(localStorage.getItem('Language') || 'en_US');
            // if this was successful without being logged in, reset Auth Reqd
            if (!sessionStorage.getItem('loggedInUser')) {
                sessionStorage.removeItem('APIAuthReqd')
                this.closeLogin();
            }
        })

        this.mythService.GetBackendInfo()
            .subscribe(
                data => {
                    var url = this.router.url;
                    if (data.BackendInfo.Env.IsDatabaseIgnored
                        || (!data.BackendInfo.Env.SchedulingEnabled && !url.startsWith('/setupwizard/')))
                        router.navigate(['setupwizard/dbsetup']);
                    else if (url == '/')
                        router.navigate(['dashboard/status']);
                });

    }

    ngOnInit(): void {
        this.dataService.loggedInUser = sessionStorage.getItem('loggedInUser');
        this.APIAuthReqd = (sessionStorage.getItem('APIAuthReqd') != null);
        if (!this.dataService.loggedInUser) {
            this.userName = localStorage.getItem('userName');
            this.userPassword = localStorage.getItem('userPassword');
            if (this.userName && this.userPassword) {
                this.login();
                return;
            }
            if (this.APIAuthReqd) {
                this.displayLogin = true;
                return;
            }
        }
    }

    findThemeByName(name: string): Theme {
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

    findLanguageByCode(code: string): Language {
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
        this.translateService.use(this.m_selectedLanguage.Code);
        this.translateService.get('primeng').subscribe(res => this.primeconfigService.setTranslation(res));
    }

    toggleShowNavbar() {
        this.m_showNavbar = !this.m_showNavbar;
    }

    toggleShowSidebar() {
        this.dataService.toggleShowSidebar();
    }

    showLogin() {
        this.APIAuthReqd = (sessionStorage.getItem('APIAuthReqd') != null);
        this.displayLogin = true;
    }

    login() {
        // login process
        this.errorCount = 0;
        this.mythService.LoginUser(this.userName!, this.userPassword!).subscribe(
            {
                next: (data: any) => {
                    if (data.String) {
                        sessionStorage.setItem('accessToken', data.String);
                        sessionStorage.setItem('loggedInUser', this.userName!);
                        if (this.keepLogin) {
                            localStorage.setItem('userName', this.userName!)
                            localStorage.setItem('userPassword', this.userPassword!)
                        }
                        location.reload();
                    }
                    else {
                        this.displayLogin = true;
                        this.errorCount++;
                    }
                },
                error: (err: any) => {
                    console.log("Login error", err);
                    this.errorCount++;
                }
            });
    }

    closeLogin() {
        this.displayLogin = false;
    }

    logout() {
        this.dataService.loggedInUser = '';
        sessionStorage.removeItem('accessToken');
        sessionStorage.removeItem('loggedInUser');
        localStorage.removeItem('userName');
        localStorage.removeItem('userPassword');
        location.reload();
    }
}
