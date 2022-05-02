import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EpgDownloadingComponent } from './epg-downloading.component';

describe('EpgDownloadingComponent', () => {
  let component: EpgDownloadingComponent;
  let fixture: ComponentFixture<EpgDownloadingComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ EpgDownloadingComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(EpgDownloadingComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
