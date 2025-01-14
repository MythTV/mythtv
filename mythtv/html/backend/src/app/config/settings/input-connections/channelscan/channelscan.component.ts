import { AfterViewInit, Component, Input, OnInit, ViewChild } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { Fieldset } from 'primeng/fieldset';
import { ScrollPanel } from 'primeng/scrollpanel';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { ChannelService } from 'src/app/services/channel.service';
import { CaptureCardList, CardAndInput, CardSubType } from 'src/app/services/interfaces/capture-card.interface';
import { ChannelScanRequest, ChannelScanStatus, Scan, ScanDialogResponse } from 'src/app/services/interfaces/channel.interface';
import { VideoMultiplex } from 'src/app/services/interfaces/multiplex.interface';
import { SetupService } from 'src/app/services/setup.service';
import { IconnectionComponent } from '../iconnection/iconnection.component';
import { VideoSource } from 'src/app/services/interfaces/videosource.interface';


interface ScanExt extends Scan {
  label?: string;
}

interface VideoMultiplexExt extends VideoMultiplex {
  label?: string;
}

class Sel {
  label: string;
  value: string;
  constructor(label: string, value: string) {
    this.label = label;
    this.value = value;
  }
}

class SatTuning {
  label: string;
  Frequency: number;
  Polarity: string;
  SymbolRate: string;
  Modulation: string;
  ModSys: string;
  FEC: string;
  constructor(label: string, Frequency: number, Polarity: string, SymbolRate: string, Modulation: string, ModSys: string, FEC: string) {
    this.label = label;
    this.Frequency = Frequency;
    this.Polarity = Polarity;
    this.SymbolRate = SymbolRate;
    this.Modulation = Modulation;
    this.ModSys = ModSys;
    this.FEC = FEC;
  }
}

@Component({
  selector: 'app-channelscan',
  templateUrl: './channelscan.component.html',
  styleUrls: ['./channelscan.component.css']
})
export class ChannelscanComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;
  @Input() iconnection!: IconnectionComponent;
  @Input() videoSourceLookup!: VideoSource[];
  @ViewChild("scroll") scrollpanel!: ScrollPanel;
  // @ViewChild("statusPanel") statusPanel!: Fieldset;

  serviceValues = [
    new Sel("settings.channelscan.tv", "tv"),
    new Sel("settings.channelscan.tvradio", "audio"),
    new Sel("settings.channelscan.all", "all"),
  ];

  cardSubType!: CardSubType;
  scanSubType = '';

  scanTypes: Sel[] = [];

  freqTableSelect = [
    new Sel('settings.channelscan.freq.usa.broadcast', 'us'),
    new Sel('settings.channelscan.freq.usa.cablehi', 'uscablehigh'),
    new Sel('settings.channelscan.freq.usa.cablehrchi', 'ushrchigh'),
    new Sel('settings.channelscan.freq.usa.cableirchi', 'usirchigh'),
    new Sel('settings.channelscan.freq.usa.cable', 'uscable'),
    new Sel('settings.channelscan.freq.usa.cablehrc', 'ushrc'),
    new Sel('settings.channelscan.freq.usa.cableirc', 'usirc'),
  ];

  countryTable = [
    new Sel('settings.channelscan.country.au', 'au'),
    new Sel('settings.channelscan.country.cl', 'cl'),
    new Sel('settings.channelscan.country.cz', 'cz'),
    new Sel('settings.channelscan.country.dk', 'dk'),
    new Sel('settings.channelscan.country.fi', 'fi'),
    new Sel('settings.channelscan.country.fr', 'fr'),
    new Sel('settings.channelscan.country.de', 'de'),
    new Sel('settings.channelscan.country.gr', 'gr'),
    new Sel('settings.channelscan.country.il', 'il'),
    new Sel('settings.channelscan.country.it', 'it'),
    new Sel('settings.channelscan.country.nl', 'nl'),
    new Sel('settings.channelscan.country.nz', 'nz'),
    new Sel('settings.channelscan.country.es', 'es'),
    new Sel('settings.channelscan.country.se', 'se'),
    new Sel('settings.channelscan.country.gb', 'gb'),
  ];

  networkTable = [
    new Sel('settings.channelscan.country.de', 'de'),
    new Sel('settings.channelscan.country.nl', 'nl'),
    new Sel('settings.channelscan.country.gb', 'gb'),
  ];

  satTuningTable = [
    new SatTuning("(Select Satellite)", 0, "h", "27500000", "qpsk", "DVB-S2", "auto"),
    new SatTuning("Thor 5/6/7 0.8W", 10872000, "h", "25000000", "8psk", "DVB-S2", "3/4"),
    new SatTuning("Eutelsat   7.0E", 10721000, "h", "22000000", "qpsk", "DVB-S", "3/4"),
    new SatTuning("Hotbird   13.0E", 12015000, "h", "27500000", "8psk", "DVB-S2", "3/4"),
    new SatTuning("Astra-1   19.2E", 11229000, "v", "22000000", "8psk", "DVB-S2", "2/3"),
    new SatTuning("Astra-3   23.5E", 12031500, "h", "27500000", "qpsk", "DVB-S2", "auto"),
    new SatTuning("Astra-2   28.2E", 10773000, "h", "23000000", "8psk", "DVB-S2", "3/4"),
    new SatTuning("Turksat-3A 42.0E", 12610000, "h", "20830000", "qpsk", "DVB-S", "3/4"),
    new SatTuning("Turksat-4A 42.0E", 11916000, "v", "30000000", "qpsk", "DVB-S", "3/4"),
    new SatTuning("Turksat-8K 42.0E", 12605000, "v", "34285000", "16apsk", "DVB-S2", "2/3"),
  ]

  atscModulationTable = [
    new Sel('settings.channelscan.modulation.atsc.vsb8', 'vsb8'),
    new Sel('settings.channelscan.modulation.atsc.qam256', 'qam256'),
    new Sel('settings.channelscan.modulation.atsc.qam128', 'qam128'),
    new Sel('settings.channelscan.modulation.atsc.qam64', 'qam64'),
  ];

  bandwidthTable = [
    new Sel('settings.channelscan.auto', 'a'),
    new Sel('6 MHz', '6'),
    new Sel('7 MHz', '7'),
    new Sel('8 MHz', '8'),
  ];

  inversionTable = [
    new Sel('settings.channelscan.auto', 'a'),
    new Sel('settings.channelscan.on', '1'),
    new Sel('settings.channelscan.off', '0'),
  ];

  modulationTable = [
    new Sel('settings.channelscan.auto', 'auto'),
    new Sel('QPSK', 'qpsk'),
    new Sel('QAM-16', 'qam_16'),
    new Sel('QAM-32', 'qam_32'),
    new Sel('QAM-64', 'qam_64'),
    new Sel('QAM-128', 'qam_128'),
    new Sel('QAM-256', 'qam_256'),
  ];

  dvbsModulationTable = [
    new Sel('QPSK', 'qpsk'),
    new Sel('8PSK', '8psk'),
    new Sel('QAM-16', 'qam_16'),
    new Sel('16PSK', '16psk'),
    new Sel('32PSK', '32psk'),
    new Sel('16APSK', '16apsk'),
  ];

  dvbtModSysTable = [
    new Sel('DVB-T', 'DVB-T'),
    new Sel('DVB-T2', 'DVB-T2'),
  ];

  dvbcModSysTable = [
    new Sel('DVB-C/A', 'DVB-C/A'),
    new Sel('DVB-C/B', 'DVB-C/B'),
    new Sel('DVB-C/C', 'DVB-C/C'),
  ];

  dvbsModSysTable = [
    new Sel('DVB-S', 'DVB-S'),
    new Sel('DVB-S2', 'DVB-S2'),
  ];

  polarityTable = [
    new Sel('settings.channelscan.polarity.horizontal', 'h'),
    new Sel('settings.channelscan.polarity.vertical', 'v'),
    new Sel('settings.channelscan.polarity.right', 'r'),
    new Sel('settings.channelscan.polarity.left', 'l'),
  ];

  fecTable = [
    new Sel('settings.channelscan.auto', 'auto'),
    new Sel('settings.channelscan.none', 'none'),
    new Sel('1/2', '1/2'),
    new Sel('2/3', '2/3'),
    new Sel('3/4', '3/4'),
    new Sel('4/5', '4/5'),
    new Sel('5/6', '5/6'),
    new Sel('6/2', '6/2'),
    new Sel('7/8', '7/8'),
    new Sel('8/9', '8/9'),
    new Sel('3/5', '3/5'),
    new Sel('9/10', '9/10'),
  ];

  transmissionModeTable = [
    new Sel('settings.channelscan.auto', 'a'),
    new Sel('2K', '2'),
    new Sel('8K', '8'),
  ];

  guardIntervalTable = [
    new Sel('settings.channelscan.auto', 'auto'),
    new Sel('1/4', '1/4'),
    new Sel('1/8', '1/8'),
    new Sel('1/16', '1/16'),
    new Sel('1/32', '1/32'),
  ];

  hierarchyTable = [
    new Sel('settings.channelscan.auto', 'a'),
    new Sel('settings.channelscan.none', 'n'),
    new Sel('1', '1'),
    new Sel('2', '2'),
    new Sel('4', '4'),
  ];

  dvbcSymbolRateTable = [
    new Sel('3450000', '3450000'),
    new Sel('5000000', '5000000'),
    new Sel('5900000', '5900000'),
    new Sel('6875000', '6875000'),
    new Sel('6900000', '6900000'),
    new Sel('6950000', '6950000'),
  ];

  dvbsSymbolRateTable = [
    new Sel('3333000', '3333000'),
    new Sel('22000000', '22000000'),
    new Sel('22500000', '22500000'),
    new Sel('23000000', '23000000'),
    new Sel('27500000', '27500000'),
    new Sel('28000000', '28000000'),
    new Sel('28500000', '28500000'),
    new Sel('29500000', '29500000'),
    new Sel('29700000', '29700000'),
    new Sel('29900000', '29900000'),
  ];

  rollOffTable = [
    new Sel('0.35', '0.35'),
    new Sel('0.20', '0.20'),
    new Sel('0.25', '0.25'),
    new Sel('Auto', 'auto'),
  ];

  lockDesc = 'settings.channelscan.lock_value';
  nolockDesc = 'settings.channelscan.nolock_value';
  procDesc = 'processed';
  unprocDesc = 'unprocessed';
  otherInputStatusTitle = 'settings.channelscan.other_title';
  otherInputStatusText = 'settings.channelscan.other_text';

  satTuning: SatTuning = this.satTuningTable[0];
  scanLog = '';
  scanRequest: ChannelScanRequest = {
    CardId: 0,
    DesiredServices: 'tv',
    FreeToAirOnly: true,
    ChannelNumbersOnly: false,
    CompleteChannelsOnly: true,
    FullChannelSearch: true,
    RemoveDuplicates: true,
    AddFullTS: false,
    TestDecryptable: false,
    ScanType: '',
    FreqTable: '',
    Modulation: '',
    FirstChan: '',
    LastChan: '',
    ScanId: 0,
    IgnoreSignalTimeout: false,
    FollowNITSetting: false,
    MplexId: 0,
    Frequency: 0,
    Bandwidth: '',
    Polarity: '',
    SymbolRate: '',
    Inversion: '',
    Constellation: '',
    ModSys: '',
    CodeRateLP: '',
    CodeRateHP: '',
    FEC: '',
    TransmissionMode: '',
    GuardInterval: '',
    Hierarchy: '',
    RollOff: ''
  };

  emptyScanStatus: ChannelScanStatus = {
    CardId: 0,
    Status: '',
    SignalLock: false,
    Progress: 0,
    SignalNoise: 0,
    SignalStrength: 0,
    StatusLog: '',
    StatusText: '',
    StatusTitle: '',
    DialogMsg: '',
    DialogInputReq: false,
    DialogButtons: []
  };

  dialogResponse: ScanDialogResponse = {
    CardId: 0,
    DialogString: '',
    DialogButton: 0
  };

  buttonText = '';

  scanStatus: ChannelScanStatus = Object.assign({}, this.emptyScanStatus);

  channels: string[] = [];
  channelCount = 1;
  // statusCollapsed = true;
  refreshCount = 0;
  helpText = '';

  scans: ScanExt[] = [];

  multiplex: VideoMultiplexExt[] = [];



  constructor(private translate: TranslateService, private channelService: ChannelService,
    private captureCardService: CaptureCardService, public setupService: SetupService) {
    this.tableTranslate(this.serviceValues);
    this.tableTranslate(this.freqTableSelect);
    this.tableTranslate(this.countryTable);
    this.tableTranslate(this.networkTable);
    this.tableTranslate(this.atscModulationTable);
    this.tableTranslate(this.bandwidthTable);
    this.tableTranslate(this.inversionTable);
    this.tableTranslate(this.modulationTable);
    this.tableTranslate(this.fecTable);
    this.tableTranslate(this.transmissionModeTable);
    this.tableTranslate(this.guardIntervalTable);
    this.tableTranslate(this.hierarchyTable);
    this.tableTranslate(this.polarityTable);

    translate.get(this.lockDesc).subscribe(data => this.lockDesc = data);
    translate.get(this.nolockDesc).subscribe(data => this.nolockDesc = data);
    translate.get(this.otherInputStatusTitle).subscribe(data => this.otherInputStatusTitle = data);
    translate.get(this.otherInputStatusText).subscribe(data => this.otherInputStatusText = data);
      }

  tableTranslate(table: { label: string, value: string }[]) {
    table.forEach(
      entry => {
        if (entry.label.startsWith('settings.'))
          this.translate.get(entry.label).subscribe(data => entry.label = data);
      });
  }

  ngAfterViewInit(): void {
    this.refreshStatus(false);
    this.getScanList();
    this.getmultiplexList();
    this.iconnection.scanComponent = this;
  }


  ngOnInit(): void {
    // subject.subscribe to allow DVB card to be ready. Otherwise it fails to open
    // because it is already open from the GetCaptureDeviceList call
    this.iconnection.deviceFree.subscribe((x) => {
      this.setupCard();
    });
    // In case this ran after capturedevices was already loaded (unlikely)
    if (this.iconnection.captureDeviceList.CaptureDeviceList.CaptureDevices.length > 0
      && !this.cardSubType) {
      this.iconnection.deviceFree.next(true);
    }
  }

  setupCard(): void {
    if (this.setupService.schedulingEnabled)
      return;
    this.captureCardService.GetCardSubType(this.card.CardId).subscribe(data => {
      this.cardSubType = data.CardSubType;
      this.scanRequest.ScanType = '';
      this.buildScanTypeList();
      this.onFreqTableChange(false);
      if (this.cardSubType.InputType == 'DVBT2')
        this.scanRequest.ModSys = 'DVB-T2';       // default value
      if (this.cardSubType.InputType == 'DVBS2')
        this.scanRequest.ModSys = 'DVB-S2';       // default value
      if (this.cardSubType.InputType == 'DVBC')
        this.scanRequest.SymbolRate = '6900000';       // default value
      if (['DVBS', 'DVBS2'].includes(this.cardSubType.InputType))
        this.scanRequest.SymbolRate = '27500000';       // default value
    });
  }


  buildScanTypeList() {
    let transp = false;
    this.helpText = '';
    this.scanSubType = this.cardSubType.InputType;
    this.scanTypes.length = 0;
    switch (this.cardSubType.InputType) {
      case 'V4L':
      case 'MPEG':
        this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        break;
      case 'DVBT':
      case 'DVBT2':
        this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
        this.scanTypes.push({ label: 'settings.channelscan.type.fulltuned', value: 'FULLTUNED' });
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        this.helpText = 'settings.channelscan.scantype_dvbt_desc';
        transp = true;
        break;
      case 'DVBC':  // AKA QAM
        this.scanTypes.push({ label: 'settings.channelscan.type.fulltuned', value: 'FULLTUNED' });
        this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        transp = true;
        break;
      case 'DVBS':
      case 'DVBS2':
        this.scanTypes.push({ label: 'settings.channelscan.type.fulltuned', value: 'FULLTUNED' });
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        transp = true;
        break;
      case 'ATSC':
        this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        transp = true;
        break;
      case 'HDHOMERUN':
        if (this.cardSubType.HDHRdoesDVBC) {
          this.scanSubType = 'DVBC';
          this.scanTypes.push({ label: 'settings.channelscan.type.fulltuned', value: 'FULLTUNED' });
          this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
          this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        }
        else if (this.cardSubType.HDHRdoesDVB) {
          this.scanSubType = 'DVBT';
          this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
          this.scanTypes.push({ label: 'settings.channelscan.type.fulltuned', value: 'FULLTUNED' });
          this.helpText = 'settings.channelscan.scantype_dvbt_desc';
        }
        else {
          this.scanSubType = 'ATSC';
          this.scanTypes.push({ label: 'settings.channelscan.type.full', value: 'FULL' });
        }
        this.scanTypes.push({ label: 'settings.channelscan.type.import', value: 'IMPORT' });
        this.scanTypes.push({ label: 'settings.channelscan.type.hdhrimport', value: 'HDHRIMPORT' });
        transp = true;
        break;
      case 'VBOX':
        this.scanTypes.push({ label: 'settings.channelscan.type.vboximport', value: 'VBOXIMPORT' });
        break;
      case 'FREEBOX':
        this.scanTypes.push({ label: 'settings.channelscan.type.m3umpts', value: 'MPTSIMPORT' });
        this.scanTypes.push({ label: 'settings.channelscan.type.m3u', value: 'M3UIMPORT' });
        break;
      case 'ASI':
        this.scanTypes.push({ label: 'settings.channelscan.type.asi', value: 'ASI' });
        break;
      case 'EXTERNAL':
        this.scanTypes.push({ label: 'settings.channelscan.type.mpts', value: 'MPTS' });
        this.scanTypes.push({ label: 'settings.channelscan.type.externimport', value: 'EXTIMPORT' });
        break;
      case 'ERROR_PROBE':
        this.scanTypes.push({ label: 'settings.channelscan.type.errorprobe', value: 'ERROR' });
        break;
      default:
        this.scanTypes.push({ label: 'settings.channelscan.type.erroropen', value: 'ERROR' });
        break;
    }
    if (transp) {
      this.scanTypes.push({ label: 'settings.channelscan.type.alltransport', value: 'ALLTRANSPORT' });
      this.scanTypes.push({ label: 'settings.channelscan.type.onetransport', value: 'ONETRANSPORT' });
    }
    this.scanTypes.forEach(
      entry => this.translate.get(entry.label).subscribe(data => entry.label = data));

    if (this.helpText)
      this.translate.get(this.helpText).subscribe(data => this.helpText = data);
  }

  onScanTypeChange() {
    if (this.scanRequest.ScanType == 'FULLTUNED')
      this.scanRequest.Frequency = this.videoSourceLookup[this.card.SourceId].ScanFrequency;
    else
      this.scanRequest.Frequency = 0;
    setTimeout(() => this.onFreqTableChange(false), 100);
  }

  onFreqTableChange(modchange: boolean) {
    this.channels = [];
    let mod = '';
    if (this.scanRequest.FreqTable.startsWith('us')) {

      if (!modchange) {
        if (this.scanRequest.FreqTable == 'us')
          this.scanRequest.Modulation = 'vsb8';
        else if (this.scanRequest.Modulation == 'vsb8')
          this.scanRequest.Modulation = 'qam256';
      }
      if (this.scanRequest.Modulation == 'vsb8')
        mod = 'ATSC Channel ';
      else if (this.scanRequest.Modulation == 'qam256')
        mod = 'QAM-256 Channel ';
      else if (this.scanRequest.Modulation == 'qam128')
        mod = 'QAM-128 Channel ';
      else if (this.scanRequest.Modulation == 'qam64')
        mod = 'QAM-64 Channel ';
      if (this.scanRequest.FreqTable == 'us') {
        if (this.scanRequest.Modulation == 'vsb8') {
          for (let x = 2; x <= 36; x++)
            this.channels.push(mod + x);
        }
      }
      else if (this.scanRequest.FreqTable.match(/us.*high/)) {
        for (let x = 78; x <= 158; x++)
          this.channels.push(mod + x);
      }
      else if (this.scanRequest.FreqTable.match(/^us/)) {
        this.channels.push(mod + 'T-13');
        this.channels.push(mod + 'T-14');
        for (let x = 2; x <= 158; x++)
          this.channels.push(mod + x);
      }
      if (this.channels.length > 0) {
        setTimeout(() => {
          this.scanRequest.FirstChan = this.channels[0];
          this.scanRequest.LastChan = this.channels[this.channels.length - 1];
        }, 100);
      }
      else {
        this.scanRequest.FirstChan = '';
        this.scanRequest.LastChan = '';
      }
    }
  }

  onSatTuningChange() {
    Object.assign(this.scanRequest, this.satTuning);
  }

  calcCount(): number {
    return this.channelCount = this.channels.findIndex(entry => entry == this.scanRequest.LastChan)
      - this.channels.findIndex(entry => entry == this.scanRequest.FirstChan) + 1;
  }

  getScanList() {
    this.channelService.GetScanList(this.card.SourceId).subscribe(data => {
      this.scans = data.ScanList.Scans;
      this.scans.forEach(entry => {
        var date = new Date(entry.ScanDate);
        entry.label = date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
          + ' ' + (entry.Processed ? this.procDesc : this.unprocDesc);
      })
    })
  }

  getmultiplexList() {
    this.channelService.GetVideoMultiplexList({ SourceID: this.card.SourceId }).subscribe(data => {
      this.multiplex = data.VideoMultiplexList.VideoMultiplexes;
    });
  }

  startScan() {
    if (this.iconnection.currentForm && this.iconnection.currentForm.form.dirty) {
      this.iconnection.work.startScan = true;
      this.iconnection.saveForm();
      return;
    }
    this.scanRequest.CardId = this.card.CardId;
    this.channelService.StartScan(this.scanRequest).subscribe(data => {
      setTimeout(() => this.refreshStatus(true), 500);
    });

  }

  stopScan() {
    this.channelService.StopScan(this.card.CardId).subscribe();
  }

  respondDialog() {
    this.dialogResponse.CardId = this.card.CardId;
    this.dialogResponse.DialogButton = this.scanStatus.DialogButtons.indexOf(this.buttonText);
    this.channelService.SendScanDialogResponse(this.dialogResponse)
      .subscribe(x => this.iconnection.loadChannels());
  }

  refreshStatus(doChannels: boolean ) {
    this.channelService.GetScanStatus().subscribe(data => {
      if (data.ScanStatus.CardId == this.card.CardId) {
        this.scanStatus = data.ScanStatus;
        this.scanLog = data.ScanStatus.StatusLog.split('\n').join('<br>');
        this.scrollpanel.scrollTop(100000);
      }
      else {
        this.scanStatus = Object.assign({}, this.emptyScanStatus);
        this.scanStatus.Status = data.ScanStatus.Status;
        this.scanStatus.CardId = data.ScanStatus.CardId;
        this.scanLog = '';
        if (this.scanStatus.Status == 'RUNNING') {
          this.scanStatus.StatusTitle = this.otherInputStatusTitle;
          this.scanStatus.StatusText = this.otherInputStatusText;
        }
      }
      if (this.scanStatus.Status == 'RUNNING')
        this.refreshCount = 5;
      else
        this.refreshCount--;
      if (this.refreshCount > 0)
        setTimeout(() => this.refreshStatus(true), 500);
      else {
        if (doChannels)
          this.iconnection.loadChannels();
      }

    });
  }

}
