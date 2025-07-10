import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { PartialObserver, Subject } from 'rxjs';
import { startWith } from 'rxjs/operators';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { ChannelService } from 'src/app/services/channel.service';
import { CardAndInput, CaptureCardList, InputGroup, DiseqcTreeList, DiseqcTree, DiseqcConfig, CaptureDeviceList, CaptureDevice } from 'src/app/services/interfaces/capture-card.interface';
import { Channel, FetchChannelsFromSourceRequest, GetChannelInfoListRequest } from 'src/app/services/interfaces/channel.interface';
import { VideoSource, VideoSourceList } from 'src/app/services/interfaces/videosource.interface';
import { SetupService } from 'src/app/services/setup.service';
import { InputConnectionsComponent } from '../input-connections.component';
import { ChannelscanComponent } from '../channelscan/channelscan.component';

@Component({
  selector: 'app-iconnection',
  templateUrl: './iconnection.component.html',
  styleUrls: ['./iconnection.component.css']
})
export class IconnectionComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;
  // Video Sources Indexed by id
  @Input() videoSourceLookup!: VideoSource[];
  @Input() videoSourceList!: VideoSourceList;
  // parent
  @Input() parentComponent!: InputConnectionsComponent;

  @ViewChild("connform") currentForm!: NgForm;
  // @ViewChild("top") topElement!: ElementRef;

  // List of channels
  allChannels: Channel[] = [];
  sourceChannels: Channel[] = [];
  // List of User Groups filtered
  inputGroups: InputGroup[] = [];
  selectGroups: InputGroup[] = [];

  diseqcTreeList!: DiseqcTreeList;

  diseqcTree!: DiseqcTree;

  diseqcConfig!: DiseqcConfig;

  captureDeviceList: CaptureDeviceList = {
    CaptureDeviceList: {
      CaptureDevices: [],
    }
  };

  currentDevice: CaptureDevice = <CaptureDevice>{ FrontendName: "Unknown", InputNames: ['MPEG2TS'] };

  scanComponent?: ChannelscanComponent;

  work = {
    successCount: 0,
    errorCount: 0,
    expectedCount: 0,
    recLimitUpd: false,
    reloadGroups: false,
    // These are flled with the value that would be obtained from CardUtil::IsEncoder
    // and CardUtil::IsUnscanable
    isEncoder: false,
    isUnscanable: false,
    hasTuner: false,
    isTunerSharable: false,
    showPresetTuner: false,
    // For input group
    inputGroupName: "",
    orgInputGroupName: "",
    fetchChannelsDialog: false,
    // 0 = not fetch, 1 = in progress, 2 = success, 3 = fail
    fetchStatus: 0,
    fetchCount: 0,
    switchPort: 0,
    rotorDegrees: 0,
    scrPort: "",
    // Eastern = 1, Western = -1
    hemisphere: 1,
    isReady: false,
    startScan: false,
  };

  deviceFree = new Subject<boolean>();

  orgInputGroupIds: number[] = [];

  fetchMessages = [
    "",
    "settings.iconnection.fetch.inprog",
    "settings.iconnection.fetch.complete",
    "settings.iconnection.fetch.failed",
    // "settings.iconnection.fetch.incompatible"
  ]

  messages = {
    devNotExist: 'settings.capture.dvb.devNotExist',
  }

  preEncodedTypes = [
    "DVB", "FIREWIRE", "HDHOMERUN", "FREEBOX", "IMPORT", "DEMO",
    "ASI", "CETON", "VBOX", "SATIP"
  ];

  unscanableTypes = [
    "FIREWIRE", "HDPVR", "IMPORT", "DEMO", "GO7007", "MJPEG"
  ];

  hasTunerTypes = [
    "DVB", "HDHOMERUN", "FREEBOX", "CETON", "VBOX", "SATIP"
  ];

  tunerSharableTypes = [
    "DVB", "HDHOMERUN", "ASI", "FREEBOX", "CETON", "EXTERNAL",
    "VBOX", "V4L2ENC", "SATIP"
  ];

  quickTuneValues = [
    { prompt: "settings.iconnection.quicktune.never", value: 0 },
    { prompt: "settings.iconnection.quicktune.livetv", value: 1 },
    { prompt: "settings.iconnection.quicktune.always", value: 2 }
  ];

  constructor(private translate: TranslateService, private channelService: ChannelService,
    private captureCardService: CaptureCardService, public setupService: SetupService) {

    this.quickTuneValues.forEach(
      entry => translate.get(entry.prompt).subscribe(data => entry.prompt = data));

    this.loadChannels();
    this.loadInputGroups();
  }

  loadChannels() {
    const channelRequest: GetChannelInfoListRequest = {
      Details: true
    };
    this.channelService.GetChannelInfoList(channelRequest).subscribe(data => {
      this.allChannels = data.ChannelInfoList.ChannelInfos;
      this.fillChannelList();
    });
  }

  fillChannelList(): void {
    this.sourceChannels = this.allChannels.filter(data => data.SourceId == this.card.SourceId);
    if (!this.sourceChannels.find(data => data.ChanNum == this.card.StartChannel))
      this.card.StartChannel = "";
  }

  loadInputGroups() {
    this.captureCardService.GetInputGroupList().subscribe(data => {
      this.inputGroups = data.InputGroupList.InputGroups;
      this.inputGroups.forEach(x => {
        if (x.InputGroupName, startWith("user:")) {
          const name = x.InputGroupName.substring(5);
          if (this.selectGroups.findIndex(x => name == x.InputGroupName) == -1)
            this.selectGroups.push({
              CardInputId: 0,
              InputGroupId: x.InputGroupId,
              InputGroupName: name
            });
          if (x.CardInputId == this.card.CardId) {
            if (!this.work.inputGroupName) {
              this.work.inputGroupName = name;
              this.work.orgInputGroupName = name;
            }
            this.orgInputGroupIds.push(x.InputGroupId);
          }
        }
      });
    });

  }

  ngOnInit(): void {
    this.work.isEncoder = (this.preEncodedTypes.indexOf(this.card.CardType) < 0);
    this.work.isUnscanable = (this.unscanableTypes.indexOf(this.card.CardType) >= 0);
    this.work.hasTuner = (this.hasTunerTypes.indexOf(this.card.CardType) >= 0);
    this.work.isTunerSharable = (this.tunerSharableTypes.indexOf(this.card.CardType) >= 0);
    if (this.work.isEncoder || this.work.isUnscanable)
      if (this.work.hasTuner || this.card.CardType == "EXTERNAL")
        this.work.showPresetTuner = true;
    if (this.card.CardType == "DVB") {
      this.loadDiseqc();
    }
    if (!this.card.DisplayName)
      this.card.DisplayName = "Input " + this.card.CardId;
    if (!this.work.isTunerSharable) {
      this.card.SchedGroup = false;
      this.card.RecLimit = 1;
    }
    this.captureCardService.GetCaptureDeviceList(this.card.CardType)
      .subscribe({
        next: data => {
          this.captureDeviceList = data;
          this.setupDevice();
          this.deviceFree.next(true);
        },
        error: (err: any) => {
          console.log("GetCaptureDeviceList", err);
          this.work.errorCount++;
        }
      });
  }

  // After load of devices, make sure the current record is selected in list
  setupDevice(): void {
    if (this.card.VideoDevice) {
      let device = this.captureDeviceList.CaptureDeviceList.CaptureDevices.find(x => x.VideoDevice == this.card.VideoDevice);
      if (device)
        this.currentDevice = device;
      else {
        this.currentDevice = <CaptureDevice>{
          VideoDevice: this.card.VideoDevice,
          FrontendName: this.messages.devNotExist,
          InputNames: ['MPEG2TS']
        };
        this.captureDeviceList.CaptureDeviceList.CaptureDevices.push(this.currentDevice);
      }
    }
    if (this.currentDevice && this.card.InputName) {
      if (!this.currentDevice.InputNames.includes(this.card.InputName)) {
        this.currentDevice.InputNames.push(this.card.InputName);
      }
      if (!this.currentDevice.InputNames.includes('MPEG2TS')) {
        this.currentDevice.InputNames.push('MPEG2TS');
      }
    }
    this.work.isReady = true;
  }



  loadDiseqc() {
    // Get DiseqcTree list
    this.captureCardService.GetDiseqcTreeList()
      .subscribe({
        next: data => {
          this.diseqcTreeList = data;
          this.setupDiseqc();
        },
        error: (err: any) => {
          console.log("GetDiseqcTreeList", err);
          this.work.errorCount++;
        }
      });
  }

  setupDiseqc(): void {
    let tree = this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.find
      (x => x.DiSEqCId == this.card.DiSEqCId);
    if (tree) {
      this.diseqcTree = tree;
      switch (this.diseqcTree.Type) {
        case 'switch':
        case 'rotor':
        case 'scr':
          this.captureCardService.GetDiseqcConfigList()
            .subscribe({
              next: data => {
                let config = data.DiseqcConfigList.DiseqcConfigs
                  .find(entry => entry.CardId == this.card.CardId
                    && entry.DiSEqCId == this.card.DiSEqCId);
                if (config)
                  this.diseqcConfig = config;
                else
                  this.diseqcConfig = {
                    CardId: this.card.CardId,
                    DiSEqCId: this.card.DiSEqCId,
                    Value: ''
                  };
                if (this.diseqcTree.Type == "switch") {
                  this.work.switchPort = Number.parseInt(this.diseqcConfig.Value) - 1;
                  if (Number.isNaN(this.work.switchPort))
                    this.work.switchPort = 0;
                }
                else if (this.diseqcTree.Type == "rotor") {
                  this.work.rotorDegrees = Number.parseFloat(this.diseqcConfig.Value);
                  if (Number.isNaN(this.work.rotorDegrees))
                    this.work.rotorDegrees = 0;
                  if (this.work.rotorDegrees < 0) {
                    this.work.hemisphere = -1;
                    this.work.rotorDegrees = - this.work.rotorDegrees;
                  }
                  else
                    this.work.hemisphere = 1;
                }
                else if (this.diseqcTree.Type == "scr") {
                  switch (this.diseqcConfig.Value) {
                    case '0':
                      this.work.scrPort = 'A';
                      break;
                    case '1':
                      this.work.scrPort = 'B';
                      break;
                    default:
                      this.work.scrPort = '';
                  }
                }
              },
              error: (err: any) => {
                console.log("GetDiseqcTreeList", err);
                this.work.errorCount++;
              }
            });
      }
    }
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    // This is needed tp prevent ExpressionChangedAfterItHasBeenCheckedError
    // Previous value for 'ng-pristine': 'true'. Current value: 'false'.
    this.currentForm.form.markAsDirty();
    setTimeout(() => {
      this.currentForm.form.markAsPristine();
    }, 0);
  }

  fetchChannels(): void {
    this.work.fetchChannelsDialog = false;
    let parm: FetchChannelsFromSourceRequest = {
      SourceId: this.card.SourceId,
      CardId: this.card.CardId,
      WaitForFinish: true
    };
    this.work.fetchStatus = 1;
    this.channelService.FetchChannelsFromSource(parm).subscribe({
      next: (x: any) => {
        if (x.int > 0)
          this.work.fetchStatus = 2;
        else
          this.work.fetchStatus = 3;
        this.work.fetchCount = x.int;
        this.loadChannels();
      },
      error: (err: any) => {
        console.log("fetchChannels", err);
        this.work.fetchStatus = 3;
        this.work.fetchCount = 0;
      }
    });
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.work.successCount++;
        if (this.work.recLimitUpd) {
          if (this.work.successCount == this.work.expectedCount) {
            // Update max recordings after all other updates
            this.captureCardService.SetInputMaxRecordings(this.card.CardId, this.card.RecLimit)
              .subscribe(this.saveObserver);
          }
          else if (this.work.successCount == this.work.expectedCount + 1) {
            // reload cards to get updated list from SetInputMaxRecordings
            this.parentComponent.loadCards(false);
            this.work.recLimitUpd = false;
          }
        }
        if (this.work.successCount == this.work.expectedCount && this.work.reloadGroups) {
          this.loadInputGroups();
          this.work.reloadGroups = false;
        }
        if (this.work.successCount == this.work.expectedCount && this.work.startScan && this.scanComponent) {
          this.work.startScan = false;
          this.currentForm.form.markAsPristine();
          this.scanComponent.startScan();
        }
      }
      else {
        console.log("saveObserver error", x);
        this.work.startScan = false;
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.work.startScan = false;
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    }
  };

  saveForm() {
    this.work.successCount = 0;
    this.work.errorCount = 0;
    this.work.expectedCount = 0;
    let inputGroupId = 0;
    // Changing input group - get the new group id
    if (this.work.inputGroupName != this.work.orgInputGroupName) {
      if (this.work.inputGroupName) {
        this.captureCardService.AddUserInputGroup(this.work.inputGroupName)
          .subscribe({
            next: (x: any) => {
              this.saveCard(x.int);
            },
            error: (err: any) => {
              console.log("saveForm error", err);
              this.work.errorCount++;
              this.currentForm.form.markAsDirty();
            }
          });
      }
      else
        // Special value -1 to indicate existing group is being removed
        this.saveCard(-1);
    }
    else
      this.saveCard(0);
  }

  saveCard(inputGroupId: number) {
    if (inputGroupId != 0)
      this.work.reloadGroups = true;
    // Update device and child devices
    let counter = 0;
    this.work.recLimitUpd = false;
    this.cardList.CaptureCardList.CaptureCards.forEach(entry => {
      if (entry.CardId == this.card.CardId || entry.ParentId == this.card.CardId) {
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'inputname',
          entry.InputName = this.card.InputName)
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'displayname',
          entry.DisplayName = this.card.DisplayName)
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'sourceid',
          String(entry.SourceId = this.card.SourceId))
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'quicktune',
          String(entry.Quicktune = this.card.Quicktune))
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'dishnet_eit',
          (entry.DishnetEIT = this.card.DishnetEIT) ? '1' : '0')
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'externalcommand',
          entry.ExternalCommand = this.card.ExternalCommand)
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'tunechan',
          entry.TuneChannel = this.card.TuneChannel)
          .subscribe(this.saveObserver);
        if (this.card.StartChannel) {
          this.captureCardService.UpdateCaptureCard(entry.CardId, 'startchan',
            entry.StartChannel = this.card.StartChannel)
            .subscribe(this.saveObserver);
          this.work.expectedCount++
        }
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'recpriority',
          String(entry.RecPriority = this.card.RecPriority))
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'livetvorder',
          String(entry.LiveTVOrder = this.card.LiveTVOrder))
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'reclimit',
          String(entry.RecLimit = this.card.RecLimit))
          .subscribe(this.saveObserver);
        // Handle schedgroup and schedorder specially.  If schedgroup is
        // set, schedgroup and schedorder should be false and 0,
        // respectively, for all children.
        if (this.card.SchedGroup && this.card.CardId != entry.CardId) {
          entry.SchedGroup = false;
          entry.SchedOrder = 0;
        }
        else {
          entry.SchedGroup = this.card.SchedGroup;
          entry.SchedOrder = this.card.SchedOrder;
        }
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'schedgroup',
          entry.SchedGroup ? '1' : '0')
          .subscribe(this.saveObserver);
        this.captureCardService.UpdateCaptureCard(entry.CardId, 'schedorder',
          String(entry.SchedOrder))
          .subscribe(this.saveObserver);

        this.work.expectedCount += 12;

        if (inputGroupId != 0) {
          this.orgInputGroupIds.forEach(x => {
            this.captureCardService.UnlinkInputGroup(entry.CardId, x)
              .subscribe(this.saveObserver);
            this.work.expectedCount++;
          });
        }
        if (inputGroupId > 0) {
          this.captureCardService.LinkInputGroup(entry.CardId, inputGroupId)
            .subscribe(this.saveObserver);
          this.work.expectedCount++;
        }
        counter++;
      }
    });
    if (counter != this.card.RecLimit) {
      this.work.recLimitUpd = true;
    }
    // Save diseqc config
    if (this.diseqcConfig) {
      let newValue = "";

      if (this.diseqcTree.Type == "switch")
        newValue = (this.work.switchPort - 1).toString();
      else if (this.diseqcTree.Type == "rotor")
        newValue = (this.work.rotorDegrees * this.work.hemisphere).toString();
      else if (this.diseqcTree.Type == "scr") {
        switch (this.work.scrPort) {
          case 'A':
            newValue = '0';
            break;
          case 'B':
            newValue = '1';
            break;
          default:
            newValue = '0';
        }
      }
      if (newValue != this.diseqcConfig.Value) {
        this.diseqcConfig.Value = newValue;
        this.captureCardService.DeleteDiseqcConfig(this.card.CardId)
          .subscribe(resp => {
            this.captureCardService.AddDiseqcConfig(this.diseqcConfig)
              .subscribe(this.saveObserver);
          })
      }
    }
  }

}
