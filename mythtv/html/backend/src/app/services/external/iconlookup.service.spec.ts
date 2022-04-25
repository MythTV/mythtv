import { TestBed } from '@angular/core/testing';

import { IconlookupService } from './iconlookup.service';

describe('IconlookupService', () => {
  let service: IconlookupService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(IconlookupService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
