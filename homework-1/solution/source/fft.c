#include <math.h>
#include "fft.h"


#ifndef PI
# define PI	3.14159265358979323846264338327950288
#endif

void calc_fft(complex *v, int n, complex *temp){
    if(n > 1){			/* otherwise, do nothing and return */
        int k, m;
        complex z, w, *vo, *ve;
        ve = temp;
        vo = temp + n / 2;
        for(k=0; k<n/2; k++) {
            ve[k] = v[2*k];
            vo[k] = v[2*k+1];
        }

        calc_fft(ve, n / 2, v);
        calc_fft(vo, n / 2, v);
        for(m=0; m<n/2; m++){
            w.x = cos(2 * PI * m / (double) n);
            w.y = -sin(2 * PI * m / (double) n);
            z.x = w.x * vo[m].x - w.y * vo[m].y;
            z.y = w.x * vo[m].y + w.y * vo[m].x;
            v[m].x = ve[m].x + z.x;
            v[m].y = ve[m].y + z.y;
            v[m+n/2].x = ve[m].x - z.x;
            v[m+n/2].y = ve[m].y - z.y;
        }
  }


}


