import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EitScannerComponent } from './eit-scanner.component';

describe('EitScannerComponent', () => {
  let component: EitScannerComponent;
  let fixture: ComponentFixture<EitScannerComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ EitScannerComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(EitScannerComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
