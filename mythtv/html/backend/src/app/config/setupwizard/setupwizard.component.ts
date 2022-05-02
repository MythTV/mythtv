import { Component, Input, OnInit } from '@angular/core';
import { ConfigService } from '../../services/config.service';
import { MenuItem } from 'primeng/api';
import { Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';


@Component({
    selector: 'app-settings',
    templateUrl: './setupwizard.component.html',
    styleUrls: ['./setupwizard.component.css']
})
export class SetupWizardComponent implements OnInit {

    constructor(private router: Router,
                private configService: ConfigService,
                private translate: TranslateService) { }
    wizardItems: MenuItem[] = [];
    
    ngOnInit(): void {
        this.translate.get('setupwizard.chooseLanguage').subscribe(
            (translated: string) => {
                this.wizardItems = [{
                    label: this.translate.instant('setupwizard.chooseLanguage'),
                    routerLink: 'selectlanguage'
                },
                {
                    label: this.translate.instant('setupwizard.setupDatabase'),
                    routerLink: 'dbsetup'
                },
                {
                    label: this.translate.instant('setupwizard.setupNetwork'),
                    routerLink: 'backendnetwork'
                },
                {
                    label: this.translate.instant('setupwizard.setupStorageGroups'),
                    routerLink: 'sgsetup'
                },
                {
                    label: this.translate.instant('setupwizard.done'),
                    routerLink: 'restart'
                }
                ]
            })
        
        this.router.navigate(['settings/selectlanguage']);
    }
}
