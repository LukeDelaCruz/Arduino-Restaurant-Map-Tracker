#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include "lcd_image.h"
#include "mapconvert.h"
#include "sorting.h"

// #define NUM_RESTAURANTS 1066

struct RestDist {
  uint16_t index; // index of restaurant from 0 to NUM_RESTAURANTS-1
  uint16_t dist;  // Manhatten distance to cursor position
};

// Swap two restaurants of RestDist struct; class code
void swap_rest(RestDist *ptr_rest1, RestDist *ptr_rest2) {
  RestDist tmp = *ptr_rest1;
  *ptr_rest1 = *ptr_rest2;
  *ptr_rest2 = tmp;
}

int pivot(RestDist *rest_dist, int start, int end) {
  int pivot = rest_dist[end].dist; // pivot
  int i = start-1;  // index of the smaller elements

  for (int j = start; j <= end-1; j++) {
    if (rest_dist[j].dist <= pivot) {
      i++;
      swap_rest(&rest_dist[i],&rest_dist[j]);
    }
  }
  swap_rest(&rest_dist[i+1],&rest_dist[end]);
  return (i+1);
}

void qsort(RestDist *rest_dist, int start, int end) {
  if (start < end) { // base case
    int pi = pivot(rest_dist, start, end);
    qsort(rest_dist, start, pi-1);
    qsort(rest_dist, pi+1, end);
  }
}
