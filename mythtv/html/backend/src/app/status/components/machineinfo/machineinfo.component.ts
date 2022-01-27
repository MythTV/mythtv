import { Component, OnInit, Input } from '@angular/core';
import { MachineInfo } from 'src/app/services/interfaces/status.interface';
import { StorageGroup } from "src/app/services/interfaces/storagegroup.interface";

@Component({
  selector: 'app-status-machineinfo',
  templateUrl: './machineinfo.component.html',
  styleUrls: ['./machineinfo.component.css', '../../status.component.css']
})
export class MachineinfoComponent implements OnInit {
  @Input() machineinfo?: MachineInfo;

  constructor() { }

  ngOnInit(): void {
  }

  getStorageGroupTotal(storagegroups : StorageGroup[]) : StorageGroup {
    return storagegroups.filter(sg => sg['Id'] == 'total')[0];
  }

  getStorageGroupDetails(storagegroups : StorageGroup[]) : StorageGroup[] {
    return storagegroups.filter(sg => sg['Id'] != 'total');
  }
}
