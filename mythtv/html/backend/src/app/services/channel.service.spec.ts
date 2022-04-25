import { TestBed } from '@angular/core/testing';

import { ChannelService } from './channel.service';

describe('ChannelService', () => {
  let service: ChannelService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(ChannelService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
