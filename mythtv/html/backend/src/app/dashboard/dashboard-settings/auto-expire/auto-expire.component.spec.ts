import { ComponentFixture, TestBed } from '@angular/core/testing';

import { AutoExpireComponent } from './auto-expire.component';

describe('AutoExpireComponent', () => {
  let component: AutoExpireComponent;
  let fixture: ComponentFixture<AutoExpireComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [AutoExpireComponent]
    });
    fixture = TestBed.createComponent(AutoExpireComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
