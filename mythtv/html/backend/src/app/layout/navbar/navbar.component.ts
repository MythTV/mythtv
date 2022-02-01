import { Component, OnInit } from '@angular/core';
import { Theme } from 'src/app/services/interfaces/theme.interface';
import { ThemeService } from '../../services/theme.service';

@Component({
    selector: 'app-navbar',
    templateUrl: './navbar.component.html',
    styleUrls: ['./navbar.component.css']
})
export class NavbarComponent implements OnInit {

    m_themes$!: Theme[];
    m_selectedTheme!: Theme;

    constructor(private themeService: ThemeService) {
        this.themeService.getThemes().then((themes: Theme[]) => {
            this.m_themes$ = themes;
            this.m_selectedTheme = this.findThemeByName(localStorage.getItem('Theme') || 'Indigo Light');
            this.themeService.switchTheme(this.m_selectedTheme.CSS);
        });
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
}
