import { Component, Input, OnInit } from '@angular/core';
import { ConfigService } from '../../services/config.service';
import { MenuItem } from 'primeng/api';
import { Router } from '@angular/router';

@Component({
    selector: 'app-settings',
    templateUrl: './setupwizard.component.html',
    styleUrls: ['./setupwizard.component.css']
})
export class SetupWizardComponent implements OnInit {

    constructor(private router: Router,
                private configService: ConfigService) { }

    wizardItems: MenuItem[] = [];
    
    ngOnInit(): void {
        this.wizardItems = [{
            label: 'Choose Language',
            routerLink: 'selectlanguage'
        },
        {
            label: 'Database Setup',
            routerLink: 'dbsetup'
        },
        {
            label: 'Backend Network',
            routerLink: 'backendnetwork'
        },
        {
            label: 'Storage Groups',
            routerLink: 'sgsetup'
        },
        {
            label: 'Done',
            routerLink: 'restart'
        }
    ];
        
    this.router.navigate(['settings/selectlanguage']);

    }
}
