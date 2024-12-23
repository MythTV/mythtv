import { ComponentFixture, TestBed } from '@angular/core/testing';

import { PrevrecsComponent } from './prevrecs.component';

describe('PrevrecsComponent', () => {
  let component: PrevrecsComponent;
  let fixture: ComponentFixture<PrevrecsComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [PrevrecsComponent]
    });
    fixture = TestBed.createComponent(PrevrecsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
