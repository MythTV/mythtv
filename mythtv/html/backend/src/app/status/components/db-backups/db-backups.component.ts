import { Component, OnInit } from '@angular/core';
import { StatusService } from '../../../services/status.service';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
  selector: 'app-db-backups',
  imports: [TranslatePipe],
  templateUrl: './db-backups.component.html',
  styleUrls: ['./db-backups.component.css', '../../status.component.css'],
})
export class DbBackupsComponent implements OnInit {
  backupsList?: string[];
  
  constructor(private statusService: StatusService) { }

  ngOnInit(): void {
    this.statusService.GetBackupsList().subscribe((data) => {
      this.backupsList = data.BackupsList;
    });
  }
}
