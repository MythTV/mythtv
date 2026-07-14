import { ComponentFixture, TestBed } from '@angular/core/testing';

import { DbBackupsComponent } from './db-backups.component';

describe('DbBackupsComponent', () => {
  let component: DbBackupsComponent;
  let fixture: ComponentFixture<DbBackupsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [DbBackupsComponent]
    })
    .compileComponents();

    fixture = TestBed.createComponent(DbBackupsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
