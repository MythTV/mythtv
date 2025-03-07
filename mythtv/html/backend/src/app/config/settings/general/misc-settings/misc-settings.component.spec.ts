import { ComponentFixture, TestBed } from '@angular/core/testing';

import { MiscSettingsComponent } from './misc-settings.component';

describe('MiscSettingsComponent', () => {
  let component: MiscSettingsComponent;
  let fixture: ComponentFixture<MiscSettingsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
    imports: [MiscSettingsComponent]
})
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(MiscSettingsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
