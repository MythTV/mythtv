import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecprofileComponent } from './recprofile.component';

describe('RecprofileComponent', () => {
  let component: RecprofileComponent;
  let fixture: ComponentFixture<RecprofileComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ RecprofileComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(RecprofileComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
