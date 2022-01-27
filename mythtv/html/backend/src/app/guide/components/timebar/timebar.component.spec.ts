import { ComponentFixture, TestBed } from '@angular/core/testing';

import { TimebarComponent } from './timebar.component';

describe('TimebarComponent', () => {
  let component: TimebarComponent;
  let fixture: ComponentFixture<TimebarComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ TimebarComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(TimebarComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
