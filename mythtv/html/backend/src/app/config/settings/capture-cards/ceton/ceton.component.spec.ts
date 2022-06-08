import { ComponentFixture, TestBed } from '@angular/core/testing';

import { CetonComponent } from './ceton.component';

describe('CetonComponent', () => {
  let component: CetonComponent;
  let fixture: ComponentFixture<CetonComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ CetonComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(CetonComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
