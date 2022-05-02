import { ComponentFixture, TestBed } from '@angular/core/testing';

import { JobqueueGlobalComponent } from './jobqueue-global.component';

describe('JobqueueGlobalComponent', () => {
  let component: JobqueueGlobalComponent;
  let fixture: ComponentFixture<JobqueueGlobalComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ JobqueueGlobalComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(JobqueueGlobalComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
