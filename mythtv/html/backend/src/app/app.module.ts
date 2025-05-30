import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { HttpClientModule, HttpClient, HTTP_INTERCEPTORS } from '@angular/common/http';
import { TranslateModule, TranslateLoader } from '@ngx-translate/core';
import { TranslateHttpLoader } from '@ngx-translate/http-loader';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

import { PrimeNGModule } from './primeng.module';
import { MenuModule } from 'primeng/menu';

import { AppRoutingModule } from './app-routing.module';

import { AppComponent } from './app.component';
import { NavbarComponent } from './layout/navbar/navbar.component';
import { SidenavComponent } from './layout/sidenav/sidenav.component';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { SetupWizardRoutingModule } from './config/setupwizard/setupwizard-routing.module';
import { DashboardComponent } from './dashboard/dashboard.component';
import { StatusComponent } from './status/status.component';
import { EncodersComponent } from './status/components/encoders/encoders.component';
import { BackendsComponent } from './status/components/backends/backends.component';
import { FrontendsComponent } from './status/components/frontends/frontends.component';
import { ScheduledComponent } from './status/components/scheduled/scheduled.component';
import { JobqueueComponent } from './status/components/jobqueue/jobqueue.component';
import { MachineinfoComponent } from './status/components/machineinfo/machineinfo.component';
import { SelectLanguageComponent } from './config/setupwizard/selectlanguage/selectlanguage.component';
import { DbsetupComponent } from './config/setupwizard/dbsetup/dbsetup.component';
import { TestbedComponent } from './testbed/testbed.component';
import { GuideComponent } from './guide/guide.component';
import { ChannelIconComponent } from './guide/components/channelicon/channelicon.component';
import { ProgramEntryComponent } from './guide/components/programentry/programentry.component';
import { TimebarComponent } from './guide/components/timebar/timebar.component';
import { SettingsComponent } from './config/settings/general/general-settings.component';
import { HostAddressComponent } from './config/settings/general/host-address/host-address.component';
import { LocaleComponent } from './config/settings/general/locale/locale.component';
import { MiscSettingsComponent } from './config/settings/general/misc-settings/misc-settings.component';
import { EitScannerComponent } from './config/settings/general/eit-scanner/eit-scanner.component';
import { ShutdownWakeupComponent } from './config/settings/general/shutdown-wakeup/shutdown-wakeup.component';
import { BackendWakeupComponent } from './config/settings/general/backend-wakeup/backend-wakeup.component';
import { BackendControlComponent } from './config/settings/general/backend-control/backend-control.component';
import { JobqueueBackendComponent } from './config/settings/general/jobqueue-backend/jobqueue-backend.component';
import { JobqueueGlobalComponent } from './config/settings/general/jobqueue-global/jobqueue-global.component';
import { JobqueueCommandsComponent } from './config/settings/general/jobqueue-commands/jobqueue-commands.component';
import { EpgDownloadingComponent } from './config/settings/general/epg-downloading/epg-downloading.component';
import { CaptureCardsComponent } from './config/settings/capture-cards/capture-cards.component';
import { CetonComponent } from './config/settings/capture-cards/ceton/ceton.component';
import { DvbComponent } from './config/settings/capture-cards/dvb/dvb.component';
import { LnbComponent } from './config/settings/capture-cards/dvb/lnb/lnb.component';
import { SwitchComponent } from './config/settings/capture-cards/dvb/switch/switch.component';
import { RotorComponent } from './config/settings/capture-cards/dvb/rotor/rotor.component';
import { UnicableComponent } from './config/settings/capture-cards/dvb/unicable/unicable.component';
import { ExternalComponent } from './config/settings/capture-cards/external/external.component';
import { HdhomerunComponent } from './config/settings/capture-cards/hdhomerun/hdhomerun.component';
import { IptvComponent } from './config/settings/capture-cards/iptv/iptv.component';
import { ImportComponent } from './config/settings/capture-cards/import/import.component';
import { DemoComponent } from './config/settings/capture-cards/demo/demo.component';
import { VideoSourcesComponent } from './config/settings/video-sources/video-sources.component';
import { VsourceComponent } from './config/settings/video-sources/vsource/vsource.component';
import { InputConnectionsComponent } from './config/settings/input-connections/input-connections.component';
import { IconnectionComponent } from './config/settings/input-connections/iconnection/iconnection.component';
import { StorageGroupsComponent } from './config/settings/storage-groups/storage-groups.component';
import { SgroupComponent } from './config/settings/storage-groups/sgroup/sgroup.component';
import { ChannelEditorComponent } from './config/settings/channel-editor/channel-editor.component';
import { RecordingProfilesComponent } from './config/settings/recording-profiles/recording-profiles.component';
import { ProfileGroupComponent } from './config/settings/recording-profiles/profile-group/profile-group.component';
import { RecprofileComponent } from './config/settings/recording-profiles/profile-group/recprofile/recprofile.component';
import { SystemEventsComponent } from './config/settings/system-events/system-events.component';
import { BackendWarningComponent } from './config/backend-warning/backend-warning.component';
import { V4l2Component } from './config/settings/capture-cards/v4l2/v4l2.component';
import { HdpvrComponent } from './config/settings/capture-cards/hdpvr/hdpvr.component';
import { SatipComponent } from './config/settings/capture-cards/satip/satip.component';
import { VboxComponent } from './config/settings/capture-cards/vbox/vbox.component';
import { FirewireComponent } from './config/settings/capture-cards/firewire/firewire.component';
import { ChannelscanComponent } from './config/settings/input-connections/channelscan/channelscan.component';
import { DashboardRoutingModule } from './dashboard/dashboard-routing.module';
import { WizChanneleditComponent } from './config/setupwizard/wiz-channeledit/wiz-channeledit.component';
import { RecordingsComponent } from './dashboard/recordings/recordings.component';
import { ScheduleComponent } from './schedule/schedule.component';
import { UpcomingComponent } from './dashboard/upcoming/upcoming.component';
import { RecrulesComponent } from './dashboard/recrules/recrules.component';
import { VideosComponent } from './dashboard/videos/videos.component';
import { ProgramsComponent } from './dashboard/programs/programs.component';
import { LegendComponent } from './guide/components/legend/legend.component';
import { DashboardSettingsComponent } from './dashboard/dashboard-settings/dashboard-settings.component';
import { PlaybackGroupsComponent } from './dashboard/dashboard-settings/playback-groups/playback-groups.component';
import { ChannelGroupsComponent } from './dashboard/dashboard-settings/channel-groups/channel-groups.component';
import { AutoExpireComponent } from './dashboard/dashboard-settings/auto-expire/auto-expire.component';
import { JobsComponent } from './dashboard/dashboard-settings/jobs/jobs.component';
import { RecQualityComponent } from './dashboard/dashboard-settings/rec-quality/rec-quality.component';
import { RecPrioritiesComponent } from './dashboard/dashboard-settings/rec-priorities/rec-priorities.component';
import { DataSourcesComponent } from './dashboard/dashboard-settings/data-sources/data-sources.component';
import { CustomPrioritiesComponent } from './dashboard/dashboard-settings/custom-priorities/custom-priorities.component';
import { PrevrecsComponent } from './dashboard/prevrecs/prevrecs.component';
import { TokenInterceptor } from './services/token.interceptor';
import { ErrorInterceptor } from './services/error.interceptor';
import { UsersComponent } from './dashboard/dashboard-settings/users/users.component';

// AoT requires an exported function for factories
export function HttpLoaderFactory(http: HttpClient) {
  return new TranslateHttpLoader(http,'./assets/i18n/');
}

@NgModule({
  declarations: [
    AppComponent,
    NavbarComponent,
    SidenavComponent,
    SetupWizardComponent,
    DashboardComponent,
    StatusComponent,
    EncodersComponent,
    BackendsComponent,
    FrontendsComponent,
    ScheduledComponent,
    JobqueueComponent,
    MachineinfoComponent,
    SelectLanguageComponent,
    DbsetupComponent,
    TestbedComponent,
    GuideComponent,
    ChannelIconComponent,
    ProgramEntryComponent,
    TimebarComponent,
    SettingsComponent,
    HostAddressComponent,
    LocaleComponent,
    MiscSettingsComponent,
    EitScannerComponent,
    ShutdownWakeupComponent,
    BackendWakeupComponent,
    BackendControlComponent,
    JobqueueBackendComponent,
    JobqueueGlobalComponent,
    JobqueueCommandsComponent,
    EpgDownloadingComponent,
    CaptureCardsComponent,
    CetonComponent,
    DvbComponent,
    LnbComponent,
    SwitchComponent,
    RotorComponent,
    UnicableComponent,
    ExternalComponent,
    HdhomerunComponent,
    IptvComponent,
    ImportComponent,
    DemoComponent,
    VideoSourcesComponent,
    VsourceComponent,
    InputConnectionsComponent,
    IconnectionComponent,
    StorageGroupsComponent,
    SgroupComponent,
    ChannelEditorComponent,
    RecordingProfilesComponent,
    ProfileGroupComponent,
    RecprofileComponent,
    SystemEventsComponent,
    BackendWarningComponent,
    V4l2Component,
    HdpvrComponent,
    SatipComponent,
    VboxComponent,
    FirewireComponent,
    ChannelscanComponent,
    WizChanneleditComponent,
    RecordingsComponent,
    ScheduleComponent,
    UpcomingComponent,
    RecrulesComponent,
    VideosComponent,
    ProgramsComponent,
    LegendComponent,
    DashboardSettingsComponent,
    PlaybackGroupsComponent,
    ChannelGroupsComponent,
    AutoExpireComponent,
    JobsComponent,
    RecQualityComponent,
    RecPrioritiesComponent,
    DataSourcesComponent,
    CustomPrioritiesComponent,
    PrevrecsComponent,
    UsersComponent,
  ],
  imports: [
    BrowserModule,
    HttpClientModule,
    AppRoutingModule,
    BrowserAnimationsModule,
    FormsModule,
    ReactiveFormsModule,
    MenuModule,
    TranslateModule.forRoot({
      defaultLanguage: 'en_US',
      loader: {
        provide: TranslateLoader,
        useFactory: HttpLoaderFactory,
        deps: [HttpClient]
      }
    }),

    PrimeNGModule,
    SetupWizardRoutingModule,
    DashboardRoutingModule
  ],
  providers: [
    { provide: HTTP_INTERCEPTORS, useClass: TokenInterceptor, multi: true },
    { provide: HTTP_INTERCEPTORS, useClass: ErrorInterceptor, multi: true },
  ],
  bootstrap: [AppComponent]
})
export class AppModule { }
