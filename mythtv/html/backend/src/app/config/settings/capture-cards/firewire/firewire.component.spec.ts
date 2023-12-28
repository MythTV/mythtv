import { ComponentFixture, TestBed } from '@angular/core/testing';

import { FirewireComponent } from './firewire.component';

describe('FirewireComponent', () => {
  let component: FirewireComponent;
  let fixture: ComponentFixture<FirewireComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ FirewireComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(FirewireComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
