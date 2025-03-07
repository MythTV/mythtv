import { ComponentFixture, TestBed } from '@angular/core/testing';

import { JobqueueCommandsComponent } from './jobqueue-commands.component';

describe('JobqueueCommandsComponent', () => {
  let component: JobqueueCommandsComponent;
  let fixture: ComponentFixture<JobqueueCommandsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
    imports: [JobqueueCommandsComponent]
})
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(JobqueueCommandsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
