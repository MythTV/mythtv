import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SystemEventsComponent } from './system-events.component';

describe('SystemEventsComponent', () => {
  let component: SystemEventsComponent;
  let fixture: ComponentFixture<SystemEventsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SystemEventsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SystemEventsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
