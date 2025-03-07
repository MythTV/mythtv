import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ExternalComponent } from './external.component';

describe('ExternalComponent', () => {
  let component: ExternalComponent;
  let fixture: ComponentFixture<ExternalComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
    imports: [ExternalComponent]
})
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ExternalComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
