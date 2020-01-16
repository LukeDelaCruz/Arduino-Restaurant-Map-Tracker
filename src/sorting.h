#ifndef _POWMOD_H_
#define _POWMOD_H_

void swap_rest(RestDist *ptr_rest1, RestDist *ptr_rest2);
int pivot(RestDist *rest_dist, int start, int end);
void qsort(RestDist *rest_dist, int start, int end);

#endif
