import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router'
import { DashboardComponent } from './dashboard.component';
import { StatusComponent } from '../status/status.component';
import { ChannelEditorComponent } from '../config/settings/channel-editor/channel-editor.component';
import { GuideComponent } from '../guide/guide.component';
import { RecordingsComponent } from './recordings/recordings.component';

@NgModule({
	imports: [
		RouterModule.forChild([
			{
				path: 'dashboard', component: DashboardComponent, children: [
					{ path: 'status', component: StatusComponent },
					{ path: 'channel-editor', component: ChannelEditorComponent },
					{ path: 'program-guide', component: GuideComponent },
					{ path: 'recordings', component: RecordingsComponent },
				]
			}
		])
	],
exports: [
	RouterModule
]
})
export class DashboardRoutingModule { }
