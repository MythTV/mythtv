import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router'
import { SetupWizardComponent } from './setupwizard.component';
import { SelectLanguageComponent } from './selectlanguage/selectlanguage.component';
import { DbsetupComponent } from './dbsetup/dbsetup.component';
import { CanDeactivateGuardService } from 'src/app/can-deactivate-guard.service';
import { SettingsComponent } from '../settings/general/general-settings.component';
import { CaptureCardsComponent } from '../settings/capture-cards/capture-cards.component';
import { RecordingProfilesComponent } from '../settings/recording-profiles/recording-profiles.component';
import { VideoSourcesComponent } from '../settings/video-sources/video-sources.component';
import { InputConnectionsComponent } from '../settings/input-connections/input-connections.component';
import { ChannelEditorComponent } from '../settings/channel-editor/channel-editor.component';
import { StorageGroupsComponent } from '../settings/storage-groups/storage-groups.component';
import { SystemEventsComponent } from '../settings/system-events/system-events.component';
import { WizChanneleditComponent } from './wiz-channeledit/wiz-channeledit.component';

@NgModule({
	imports: [
		RouterModule.forChild([
			{
				path: 'setupwizard', component: SetupWizardComponent, children: [
					{
						path: 'selectlanguage', component: SelectLanguageComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'dbsetup', component: DbsetupComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'general', component: SettingsComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'capture-cards', component: CaptureCardsComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'recording-profiles', component: RecordingProfilesComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'video-sources', component: VideoSourcesComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'input-connections', component: InputConnectionsComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'channel-editor', component: WizChanneleditComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'storage-groups', component: StorageGroupsComponent,
						canDeactivate: [CanDeactivateGuardService]
					},
					{
						path: 'system-events', component: SystemEventsComponent,
						canDeactivate: [CanDeactivateGuardService]
					}
				]
			}
		])
	],
	exports: [
		RouterModule
	]
})
export class SetupWizardRoutingModule { }