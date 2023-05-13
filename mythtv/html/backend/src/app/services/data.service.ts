import { Injectable } from '@angular/core';

@Injectable({
    providedIn: 'root'
})
export class DataService {

    m_showSidebar = true;

    constructor() { }

    toggleShowSidebar() {
        this.m_showSidebar = !this.m_showSidebar;
    }
}
