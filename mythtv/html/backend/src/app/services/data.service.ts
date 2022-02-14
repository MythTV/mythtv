import { Injectable } from '@angular/core';
import { BehaviorSubject } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class DataService {

    private m_showSidebar = new BehaviorSubject(true);
    showSideBar = this.m_showSidebar.asObservable();

    constructor() { }

    toggleShowSidebar() {
        console.log("toggleShowSidebar called: " + this.m_showSidebar.value);
        this.m_showSidebar.next(!this.m_showSidebar.value);
    }
}
