import { Component, OnInit } from '@angular/core';
import { Theme } from 'src/app/services/interfaces/theme.interface';
import { ThemeService } from '../../services/theme.service';

@Component({
    selector: 'app-navbar',
    templateUrl: './navbar.component.html',
    styleUrls: ['./navbar.component.css']
})
export class NavbarComponent implements OnInit {

    themes$!: Theme[];
    selectedTheme!: Theme;

    constructor(private themeService: ThemeService) {
        this.themeService.getThemes().then((themes: Theme[]) => this.themes$ = themes);
    }

    ngOnInit(): void {
    }

    changeTheme(theme: string) {
        this.themeService.switchTheme(theme);
    }
}
