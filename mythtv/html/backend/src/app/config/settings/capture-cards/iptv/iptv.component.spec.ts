import { ComponentFixture, TestBed } from '@angular/core/testing';

import { IptvComponent } from './iptv.component';

describe('IptvComponent', () => {
  let component: IptvComponent;
  let fixture: ComponentFixture<IptvComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ IptvComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(IptvComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
