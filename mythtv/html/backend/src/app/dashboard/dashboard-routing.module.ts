import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router'
import { DashboardComponent } from './dashboard.component';
import { StatusComponent } from '../status/status.component';
import { ChannelEditorComponent } from '../config/settings/channel-editor/channel-editor.component';
import { GuideComponent } from '../guide/guide.component';
import { RecordingsComponent } from './recordings/recordings.component';
import { UpcomingComponent } from './upcoming/upcoming.component';
import { RecrulesComponent } from './recrules/recrules.component';
import { VideosComponent } from './videos/videos.component';
import { DashboardSettingsComponent } from './dashboard-settings/dashboard-settings.component';
import { CanDeactivateGuardService } from '../can-deactivate-guard.service';
import { PrevrecsComponent } from './prevrecs/prevrecs.component';

@NgModule({
	imports: [
		RouterModule.forChild([
			{
				path: 'dashboard', component: DashboardComponent, children: [
					{ path: 'status', component: StatusComponent },
					{ path: 'channel-editor', component: ChannelEditorComponent },
					{ path: 'program-guide', component: GuideComponent },
					{ path: 'recordings', component: RecordingsComponent },
					{ path: 'prev-recorded', component: PrevrecsComponent },
					{ path: 'upcoming', component: UpcomingComponent },
					{ path: 'recrules', component: RecrulesComponent },
					{ path: 'videos', component: VideosComponent },
					{ path: 'settings', component: DashboardSettingsComponent,
						canDeactivate: [CanDeactivateGuardService]
					 },
				]
			}
		])
	],
exports: [
	RouterModule
]
})
export class DashboardRoutingModule { }
