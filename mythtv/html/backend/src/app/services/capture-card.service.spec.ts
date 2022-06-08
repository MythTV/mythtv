import { TestBed } from '@angular/core/testing';

import { CaptureCardService } from './capture-card.service';

describe('CaptureCardService', () => {
  let service: CaptureCardService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(CaptureCardService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
