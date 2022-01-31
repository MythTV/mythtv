import { Inject, Injectable } from '@angular/core';
import { DOCUMENT } from '@angular/common';
import { Theme } from './interfaces/theme.interface';
import { HttpClient } from '@angular/common/http';

@Injectable({
    providedIn: 'root',
})
export class ThemeService {
    
    constructor(private http: HttpClient, @Inject(DOCUMENT) private document: Document) { }

    switchTheme(theme: string) {
        let themeLink = this.document.getElementById('app-theme') as HTMLLinkElement;

        if (themeLink) {
            themeLink.href = "assets/themes/" + theme;
        }
    }

    getThemes() {
        return this.http.get<any>('assets/themes/themes.json')
        .toPromise()
        .then(res => <Theme[]>res.data)
        .then(data => { return data; });
    }
}