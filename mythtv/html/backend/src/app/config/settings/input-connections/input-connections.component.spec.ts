import { ComponentFixture, TestBed } from '@angular/core/testing';

import { InputConnectionsComponent } from './input-connections.component';

describe('InputConnectionsComponent', () => {
  let component: InputConnectionsComponent;
  let fixture: ComponentFixture<InputConnectionsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ InputConnectionsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(InputConnectionsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
