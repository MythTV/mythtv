import { Injectable } from '@angular/core';
import {
  HttpRequest,
  HttpHandler,
  HttpEvent,
  HttpInterceptor
} from '@angular/common/http';
import { Observable } from 'rxjs';

@Injectable()
export class TokenInterceptor implements HttpInterceptor {

  constructor() { }

  intercept(request: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
    let accessToken = sessionStorage.getItem('accessToken');
    if (accessToken) {
      const modifiedReq = request.clone({
        headers: request.headers.set('Authorization', accessToken),
      });
      return next.handle(modifiedReq);
    }
    return next.handle(request);
  }
}
