import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ProgramEntryComponent } from './programentry.component';

describe('ProgramEntryComponent', () => {
  let component: ProgramEntryComponent;
  let fixture: ComponentFixture<ProgramEntryComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ ProgramEntryComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ProgramEntryComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
