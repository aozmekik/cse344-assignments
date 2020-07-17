//
// Created by drh0use on 3/12/20.
//

#ifndef UNTITLED1_FFT_H
#define UNTITLED1_FFT_H

typedef float real;
typedef struct {real x; real y;} complex;

void calc_fft(complex *v, int n, complex *temp);

#endif //UNTITLED1_FFT_H
