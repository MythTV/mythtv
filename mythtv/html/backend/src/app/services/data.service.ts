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
    loggedInUser: string | null = '';

    // This associative array will hold translations for each status
    recStatusText: AssociativeArray = {
        Default: '',
        All: '',
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
        Offline: ''
    }

    recStatusList: { Status: string, Name: string, Sequence: number }[] = [];

    constructor(private translate: TranslateService) {
        this.getTranslations();
    }

    getTranslations() {
        // Translation keys like 'data.recstatus.Pending'
        for (const [key, value] of Object.entries(this.recStatusText)) {
            this.translate.get('data.recstatus.' + key).subscribe(data => {
                Object.defineProperty(this.recStatusText, key, { value: data });
                let seq = 3;
                if (key == 'Default') seq=1;
                if (key == 'All') seq=2;
                if (key == 'Unknown') return;
                this.recStatusList.push({ Status: key, Name: data, Sequence: seq });
                if (key == 'Offline')
                    // Last entry = all done, so sort
                    this.sortStatusList();
            });
        }
    }

    sortStatusList() {
        this.recStatusList.sort((a, b) => {
            let prio = a.Sequence - b.Sequence;
            if (prio != 0)
                return prio;
            const nameA = a.Name.toUpperCase(); // ignore upper and lowercase
            const nameB = b.Name.toUpperCase(); // ignore upper and lowercase
            if (nameA < nameB) {
                return -1;
            }
            if (nameA > nameB) {
                return 1;
            }
            // names must be equal
            return 0;
        });
    }

    toggleShowSidebar() {
        this.m_showSidebar = !this.m_showSidebar;
    }

    showSidebar(x: boolean) {
        this.m_showSidebar = x;
    }

}
