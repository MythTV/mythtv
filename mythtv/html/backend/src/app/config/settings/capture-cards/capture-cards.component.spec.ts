import { ComponentFixture, TestBed } from '@angular/core/testing';

import { CaptureCardsComponent } from './capture-cards.component';

describe('CaptureCardsComponent', () => {
  let component: CaptureCardsComponent;
  let fixture: ComponentFixture<CaptureCardsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ CaptureCardsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(CaptureCardsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
