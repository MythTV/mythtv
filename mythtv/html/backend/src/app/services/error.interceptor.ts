import { Injectable } from '@angular/core';
import {
  HttpRequest,
  HttpHandler,
  HttpEvent,
  HttpInterceptor
} from '@angular/common/http';
import { catchError, Observable, throwError } from 'rxjs';

@Injectable()
export class ErrorInterceptor implements HttpInterceptor {

  constructor() { }

  intercept(request: HttpRequest<unknown>, next: HttpHandler): Observable<HttpEvent<unknown>> {
    return next.handle(request).pipe(catchError(err => {
      if (err.status == 401) {
        // auto logout if 401 response returned from api
        if (sessionStorage.getItem('accessToken')) {
          sessionStorage.removeItem('accessToken');
          sessionStorage.removeItem('loggedInUser');
          sessionStorage.removeItem('APIAuthReqd');
          location.reload();
        }
        else if (!sessionStorage.getItem('APIAuthReqd')) {
          sessionStorage.setItem('APIAuthReqd','true');
          location.reload();
        }
      }
      const error = err.error?.message || err.statusText;
      console.error(err);
      return throwError(() => error);
    }))
  }

}
