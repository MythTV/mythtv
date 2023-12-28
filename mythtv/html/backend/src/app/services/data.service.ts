import { Injectable } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

interface AssociativeArray {
    [key: string]: string
}

@Injectable({
    providedIn: 'root'
})
export class DataService {

    m_showSidebar = true;

    // This associative array will hold translations for each status
    recStatusText: AssociativeArray = {
        Pending: '',
        Failing: '',
        MissedFuture: '',
        Tuning: '',
        Failed: '',
        TunerBusy: '',
        LowDiskSpace: '',
        Cancelled: '',
        Missed: '',
        Aborted: '',
        Recorded: '',
        Recording: '',
        WillRecord: '',
        Unknown: '',
        DontRecord: '',
        PreviousRecording: '',
        CurrentRecording: '',
        EarlierShowing: '',
        TooManyRecordings: '',
        NotListed: '',
        Conflict: '',
        LaterShowing: '',
        Repeat: '',
        Inactive: '',
        NeverRecord: '',
        Offline: '',
    }

    constructor(private translate: TranslateService) {
        this.getTranslations();
    }

    getTranslations() {
        // Translation keys like 'data.recstatus.Pending'
        for (const [key, value] of Object.entries(this.recStatusText)) {
            this.translate.get('data.recstatus.' + key).subscribe(data => {
                Object.defineProperty(this.recStatusText, key, { value: data });
            });
        }
    }

    toggleShowSidebar() {
        this.m_showSidebar = !this.m_showSidebar;
    }

    showSidebar(x: boolean) {
        this.m_showSidebar = x;
    }

}
