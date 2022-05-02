import { ComponentFixture, TestBed } from '@angular/core/testing';

import { JobqueueBackendComponent } from './jobqueue-backend.component';

describe('JobqueueBackendComponent', () => {
  let component: JobqueueBackendComponent;
  let fixture: ComponentFixture<JobqueueBackendComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ JobqueueBackendComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(JobqueueBackendComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
