import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router'
import { SetupWizardComponent } from './setupwizard.component';
import { SelectLanguageComponent } from './selectlanguage/selectlanguage.component';
import { DbsetupComponent } from './dbsetup/dbsetup.component';
import { BackendnetworkComponent } from './backendnetwork/backendnetwork.component';
import { SgsetupComponent } from './sgsetup/sgsetup.component';
import { RestartComponent } from "./restart/restart.component"
import { CanDeactivateGuardService } from 'src/app/can-deactivate-guard.service';

@NgModule({
	imports: [
		RouterModule.forChild([
			{
				path: 'setupwizard', component: SetupWizardComponent, children: [
					{ path: '', redirectTo: 'setupwizard/selectlanguage', pathMatch: 'full' },
					{
						path: 'selectlanguage', component: SelectLanguageComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'dbsetup', component: DbsetupComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{ path: 'backendnetwork', component: BackendnetworkComponent },
					{ path: 'sgsetup', component: SgsetupComponent },
					{ path: 'restart', component: RestartComponent }
				]
			}
		])
	],
	exports: [
		RouterModule
	]
})
export class SetupWizardRoutingModule { }