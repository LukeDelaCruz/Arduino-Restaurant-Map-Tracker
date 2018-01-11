// Name:Harpreet Saini; ID: 1504108
//      Luke Patrick Dela Cruz; ID: 1504816
/*
Part 2 of the restaufinder saga which includes three entexnsion briefly mentioned:
1. Implementation of the quicksort algorithm to replace selection sort.
2. The restaurant list mad scrollable up or down.
3. Touch screen rating selector!
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>
#include "lcd_image.h"
#include "mapconvert.h"
#include "sorting.h"

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6
// touch screen pins, obtained from the documentaion
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin
// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940
// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

#define REST_START_BLOCK 4000000

#define NUM_RESTAURANTS  1066
int newtracker; // allows for sequential ordering of the newarray from the ratings
uint32_t globalblockNum; // caching variable from exercise 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

struct restaurant {
  int32_t lat;
  int32_t lon;
  uint8_t rating; // from 0 to 10
  char name[55];
};

struct RestDist {
  uint16_t index; // index of restaurant from 0 to NUM_RESTAURANTS-1
  uint16_t dist;  // Manhatten distance to cursor position
};

RestDist rest_dist[NUM_RESTAURANTS];

// now start reading restaurant data, let's do the first block now
// this is the cache that will globally store the runtime
restaurant restBlock[8]; // 512 bytes in total: a block

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_VERT  A1
#define JOY_HORIZ A0
#define JOY_SEL   2
#define JOY_CENTER   512
#define JOY_DEADZONE 64

// smaller numbers mean faster cursor movement
#define JOY_SPEED 64

#define CURSOR_SIZE 9

// the cursor position on the display
int cursorX, cursorY, yegMiddleX, yegMiddleY;

// forward declarations
void redrawCursor(uint16_t colour);
void qsort(RestDist *rest_dist, int start, int end);
bool check();

void setup() {
  init();
  Serial.begin(9600);

	pinMode(JOY_SEL, INPUT_PULLUP);

	tft.begin();

	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

  // initialize SPI (serial peripheral interface)
  // communication between the Arduino and the SD controller

  Serial.print("Initializing SPI communication for raw reads...");
  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("failed! Is the card inserted properly?");
    while (true) {}
  }
  else {
    Serial.println("OK!");
  }

	tft.setRotation(3);

  tft.fillScreen(ILI9341_BLACK);

  // draws the centre of the Edmonton map, leaving the rightmost 64 columns black
	yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
	yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;
	lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);

  // initial cursor position is the middle of the screen
  cursorX = (DISPLAY_WIDTH - 48)/2;
  cursorY = DISPLAY_HEIGHT/2;

  redrawCursor(ILI9341_RED);
}

// read the restaurant at position "restIndex" from the card
// and store at the location pointed to by restPtr
void getRestaurantFast(int restIndex, restaurant* restPtr) {
  // calculate the block containing this restaurant or not if we're in the same block
  if (globalblockNum - (REST_START_BLOCK + restIndex/8) != 0) {
    globalblockNum = REST_START_BLOCK + restIndex/8;

    while (!card.readBlock(globalblockNum, (uint8_t*) restBlock)) {
      Serial.println("Read block failed, trying again.");
    }
    *restPtr = restBlock[restIndex % 8];
  }
  // if the same block is of interest simply point to it using the global array
  else if (globalblockNum - (REST_START_BLOCK + restIndex/8) == 0) {
    *restPtr = restBlock[restIndex % 8];
  }
}

// class code
int selectedRest;
int j;
bool c5 = true; bool c4 = true; bool c3 = true; bool c2 = true; bool c1 = true;
void drawName(int index) {
	// 8 is vertical span of a size-1 character with one pixel of padding below
  // allows us to generate a new start by finding the remainder with respect to 30
  int sizefactor = index % 30;
  tft.setCursor(0, 8*sizefactor);
  restaurant r;
  getRestaurantFast(rest_dist[index].index, &r);
  if (index != selectedRest) { // not highlighted
    tft.setTextColor(0xFFFF, 0x0000); // white characters on black background
  } else { // highlighted
    tft.setTextColor(0x0000, 0xFFFF); // black characters on white background
  }
  tft.print(r.name);
  // tft.print("\n");
  // tft.print(" "); tft.print(r.rating);
}

// class code
void displayNames(bool direction, bool initiate) {

  if (initiate) {
    tft.fillScreen(0);
    tft.setCursor(0, 0); // where the characters will be displayed
    tft.setTextSize(1);
    tft.setTextWrap(false);
    for (int i = 0; i < 30; i++) {drawName(i);}
    tft.print("\n");
  }
  else if (direction) {
    if (selectedRest == (newtracker - (newtracker % 30))) { // last remainder that doesn't fit on a 30 group in the list on screen
      tft.fillScreen(0);
      tft.setCursor(0, 0); // where the characters will be displayed
      tft.setTextSize(1);
      tft.setTextWrap(false);
      for (int i = (newtracker - (newtracker % 30)); i < newtracker; i++) {drawName(i);}
      tft.print("\n");
      j+=30;
    }
    else {
    tft.fillScreen(0);
    tft.setCursor(0, 0); // where the characters will be displayed
    tft.setTextSize(1);
    tft.setTextWrap(false);
    for (int i = j; i < 30+j; i++) {drawName(i);} // next 30 please
    tft.print("\n");
    j+=30;
    }
  }
  else {
    tft.fillScreen(0);
    tft.setCursor(0, 0); // where the characters will be displayed
    tft.setTextSize(1);
    tft.setTextWrap(false);
    for (int i = j-60; i < j-30; i++) {drawName(i);} // previous 30 please
    tft.print("\n");
    j-=30;
  }
}

// initializes touchscreen button circles
void drawcircles() {
  //-------------------------------------------------//
  if (c5) {
    tft.drawCircle(296, 24, 23.5, 0x2DE2);
    tft.fillCircle(296, 24, 21.75, 0x0000);
    tft.setCursor(288,15);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(5);
  }
  if (!c5) {
    tft.drawCircle(296, 24, 23.5, 0x2DE2);
    tft.fillCircle(296, 24, 21.75, 0xFFFF);
    tft.setCursor(288,15);
    tft.setTextSize(3);
    tft.setTextColor(0x0000);
    tft.println(5);
  }
  if (c4) {
    tft.drawCircle(296, 72, 23.5, 0x2DE2);
    tft.fillCircle(296, 72, 21.75, 0x0000);
    tft.setCursor(288,15*4);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(4);
  }
  if (!c4) {
    tft.drawCircle(296, 72, 23.5, 0x2DE2);
    tft.fillCircle(296, 72, 21.75, 0xFFFF);
    tft.setCursor(288,15*4);
    tft.setTextSize(3);
    tft.setTextColor(0x0000);
    tft.println(4);
  }
  if (c3) {
    tft.drawCircle(296, 120, 23.5, 0x2DE2);
    tft.fillCircle(296, 120, 21.75, 0x0000);
    tft.setCursor(288,15*7.25);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(3);
  }
  if (!c3) {
    tft.drawCircle(296, 120, 23.5, 0x2DE2);
    tft.fillCircle(296, 120, 21.75, 0xFFFF);
    tft.setCursor(288,15*7.25);
    tft.setTextSize(3);
    tft.setTextColor(0x0000);
    tft.println(3);
  }
  if (c2) {
    tft.drawCircle(296, 168, 23.5, 0x2DE2);
    tft.fillCircle(296, 168, 21.75, 0x0000);
    tft.setCursor(288,15*10.60);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(2);
  }
  if (!c2) {
    tft.drawCircle(296, 168, 23.5, 0x2DE2);
    tft.fillCircle(296, 168, 21.75, 0xFFFF);
    tft.setCursor(288,15*10.60);
    tft.setTextSize(3);
    tft.setTextColor(0x0000);
    tft.println(2);
  }
  if (c1) {
    tft.drawCircle(296, 216, 23.5, 0x2DE2);
    tft.fillCircle(296, 216, 21.75, 0x0000);
    tft.setCursor(288,15*13.75);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(1);
  }
  if (!c1) {
    tft.drawCircle(296, 216, 23.5, 0x2DE2);
    tft.fillCircle(296, 216, 21.75, 0xFFFF);
    tft.setCursor(288,15*13.75);
    tft.setTextSize(3);
    tft.setTextColor(0x0000);
    tft.println(1);
  }
  //-------------------------------------------------//
}

// allows for the functionality of having only one rating highligted at once
void clearselect() {
  //-------------------------------------------------//
  if (c5) {
    tft.drawCircle(296, 24, 23.5, 0x2DE2);
    tft.fillCircle(296, 24, 21.75, 0x0000);
    tft.setCursor(288,15);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(5);
  }
  if (c4) {
    tft.drawCircle(296, 72, 23.5, 0x2DE2);
    tft.fillCircle(296, 72, 21.75, 0x0000);
    tft.setCursor(288,15*4);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(4);
  }
  if (c3) {
    tft.drawCircle(296, 120, 23.5, 0x2DE2);
    tft.fillCircle(296, 120, 21.75, 0x0000);
    tft.setCursor(288,15*7.25);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(3);
  }
  if (c2) {
    tft.drawCircle(296, 168, 23.5, 0x2DE2);
    tft.fillCircle(296, 168, 21.75, 0x0000);
    tft.setCursor(288,15*10.60);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(2);
  }
  if (c1) {
    tft.drawCircle(296, 216, 23.5, 0x2DE2);
    tft.fillCircle(296, 216, 21.75, 0x0000);
    tft.setCursor(288,15*13.75);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF);
    tft.println(1);
  }
  //-------------------------------------------------//
}

// what is selected?
bool checkselect() {
  TSPoint touch = ts.getPoint();
  if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE) {return false;}
  int touchY = map(touch.x, TS_MINX, TS_MAXX, 0, DISPLAY_HEIGHT - 1);
  int touchX = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH - 1, 0);
  if (touchX > 272.5 && touchX < 319.5) {
    if (touchY > 0.5 && touchY < 47.5 && c5) { // select
      tft.drawCircle(296, 24, 23.5, 0x2DE2);
      tft.fillCircle(296, 24, 21.75, 0xFFFF);
      tft.setCursor(288,15);
      tft.setTextSize(3);
      tft.setTextColor(0x0000);
      tft.println(5);
      c5 = false;
      c4 = c3 = c2 = c1 = true;
      delay(150); // finger release delay
      return true;
    }
    else if (touchY > 0.5 && touchY < 47.5 && !c5) { // deselect
      tft.drawCircle(296, 24, 23.5, 0x2DE2);
      tft.fillCircle(296, 24, 21.75, 0x0000);
      tft.setCursor(288,15);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF);
      tft.println(5);
      c5 = true;
      delay(150);
      return true;
    }
    else if (touchY > 48.5 && touchY < 95.5 && c4) {
      tft.drawCircle(296, 72, 23.5, 0x2DE2);
      tft.fillCircle(296, 72, 21.75, 0xFFFF);
      tft.setCursor(288,15*4);
      tft.setTextSize(3);
      tft.setTextColor(0x0000);
      tft.println(4);
      c4 = false;
      c5 = c3 = c2 = c1 = true;
      delay(150);
      return true;
    }
    else if (touchY > 48.5 && touchY < 95.5 && !c4) {
      tft.drawCircle(296, 72, 23.5, 0x2DE2);
      tft.fillCircle(296, 72, 21.75, 0x0000);
      tft.setCursor(288,15*4);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF);
      tft.println(4);
      c4 = true;
      delay(150);
      return true;
    }
    else if (touchY > 96.5 && touchY < 143.5 && c3) {
      tft.drawCircle(296, 120, 23.5, 0x2DE2);
      tft.fillCircle(296, 120, 21.75, 0xFFFF);
      tft.setCursor(288,15*7.25);
      tft.setTextSize(3);
      tft.setTextColor(0x0000);
      tft.println(3);
      c3 = false;
      c5 = c4 = c2 = c1 = true;
      delay(150);
      return true;
    }
    else if (touchY > 96.5 && touchY < 143.5 && !c3) {
      tft.drawCircle(296, 120, 23.5, 0x2DE2);
      tft.fillCircle(296, 120, 21.75, 0x0000);
      tft.setCursor(288,15*7.25);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF);
      tft.println(3);
      c3 = true;
      delay(150);
      return true;
    }
    else if (touchY > 144.5 && touchY < 191.5 && c2) {
      tft.drawCircle(296, 168, 23.5, 0x2DE2);
      tft.fillCircle(296, 168, 21.75, 0xFFFF);
      tft.setCursor(288,15*10.60);
      tft.setTextSize(3);
      tft.setTextColor(0x0000);
      tft.println(2);
      c2 = false;
      c5 = c4 = c3 = c1 = true;
      delay(150);
      return true;
    }
    else if (touchY > 144.5 && touchY < 191.5 && !c2) {
      tft.drawCircle(296, 168, 23.5, 0x2DE2);
      tft.fillCircle(296, 168, 21.75, 0x0000);
      tft.setCursor(288,15*10.60);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF);
      tft.println(2);
      c2 = true;
      delay(150);
      return true;
    }
    else if (touchY > 192.5 && touchY < 239.5 && c1) {
      tft.drawCircle(296, 216, 23.5, 0x2DE2);
      tft.fillCircle(296, 216, 21.75, 0xFFFF);
      tft.setCursor(288,15*13.75);
      tft.setTextSize(3);
      tft.setTextColor(0x0000);
      tft.println(1);
      c1 = false;
      c5 = c4 = c3 = c2 = true;
      delay(150);
      return true;
    }
    else if (touchY > 192.5 && touchY < 239.5 && !c1) {
      tft.drawCircle(296, 216, 23.5, 0x2DE2);
      tft.fillCircle(296, 216, 21.75, 0x0000);
      tft.setCursor(288,15*13.75);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF);
      tft.println(1);
      c1 = true;
      delay(150);
      return true;
    }
  }
}

// gauge the rating, calculate the manhattan distance, store them by their rating, and quick sort!
void restausort() {
  newtracker = 0;
  restaurant r2;
  for (int i = 0; i < NUM_RESTAURANTS; i++) {
    getRestaurantFast(i, &r2);
    // rating gauges
    if(!c5 && r2.rating >= 9){
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
    else if (!c4 && r2.rating >= 7) {
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
    else if (!c3 && r2.rating >= 5) {
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
    else if (!c2 && r2.rating >= 3) {
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
    else if (!c1 && r2.rating >= 1) {
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
    else if (c5 && c4 && c3 && c2 && c1) {
      rest_dist[newtracker].dist = abs(yegMiddleX + (cursorX-CURSOR_SIZE/2) - lon_to_x(r2.lon))
                                 + abs(yegMiddleY + (cursorY-CURSOR_SIZE/2) - lat_to_y(r2.lat));
      rest_dist[newtracker].index = i; // matches the distance with the restaurant
      newtracker++;
    }
  }
  Serial.println(newtracker); //rating|restaucount: 5|154, 4|654, 3|943, 2|1028, 1|1066 for reference
  qsort(rest_dist, 0, newtracker-1);
}

void redrawCursor(uint16_t colour) {
  tft.fillRect(cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}

// code from class lec 12 by Zac that's rearranged to simply check for movement
bool check() {
    int xVal = analogRead(JOY_HORIZ);
    int yVal = analogRead(JOY_VERT);
    bool stickPushed = false;
    if (xVal > JOY_CENTER + JOY_DEADZONE) {stickPushed = true;}
    else if (xVal < JOY_CENTER - JOY_DEADZONE) {stickPushed = true;}
    if (yVal < JOY_CENTER - JOY_DEADZONE) {stickPushed = true;}
    else if (yVal > JOY_CENTER + JOY_DEADZONE) {stickPushed = true;}
    return stickPushed;
}

void modeOneProcessJoystick() {
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);

  //positions the image drawing from where the cursor just was relative to the middle of the screen
  lcd_image_draw(&yegImage, &tft,
                yegMiddleX + (cursorX-CURSOR_SIZE/2), yegMiddleY + (cursorY-CURSOR_SIZE/2),
                cursorX-CURSOR_SIZE/2, cursorY-CURSOR_SIZE/2, 9, 9);

  // move the cursor, further pushes mean faster movement
  cursorX += (JOY_CENTER - xVal) / JOY_SPEED;
  cursorY += (yVal - JOY_CENTER) / JOY_SPEED;

  // the principle of constrain after the change holds true!
  cursorX = constrain(cursorX, (0 + CURSOR_SIZE/2), (271 - CURSOR_SIZE/2));
  cursorY = constrain(cursorY, (0 + CURSOR_SIZE/2), (239 - CURSOR_SIZE/2));

  // let's go one screen left from the right
  if (cursorX == (0 + CURSOR_SIZE/2) && yegMiddleX > 0) {
    yegMiddleX -= (DISPLAY_WIDTH-48); // reorient middle of map
    yegMiddleX = constrain(yegMiddleX, 0, 2048-272);
    redrawCursor(ILI9341_RED); // "freezes cursor at edge before transition"
    lcd_image_draw(&yegImage, &tft,
     // upper left corner in the image to draw
     yegMiddleX, yegMiddleY,
     // upper left corner of the screen to draw it in
     0, 0,
     // width and height of the patch of the image to draw
     DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
     // initial cursor position is the middle of the screen
     cursorX = (DISPLAY_WIDTH - 48)/2;
     cursorY = DISPLAY_HEIGHT/2;
  }
  // let's go one screen right from the left
  else if (cursorX == (271 - CURSOR_SIZE/2) && yegMiddleX < 2048-272) {
    yegMiddleX += (DISPLAY_WIDTH - 48);
    yegMiddleX = constrain(yegMiddleX, 0, 2048-272);
    redrawCursor(ILI9341_RED);
    lcd_image_draw(&yegImage, &tft,
     yegMiddleX, yegMiddleY,
     0, 0,
     DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
     cursorX = (DISPLAY_WIDTH - 48)/2;
     cursorY = DISPLAY_HEIGHT/2;
  }
  // let's go one screen up from below
  if (cursorY == (0 + CURSOR_SIZE/2) && yegMiddleY > 0) {
    yegMiddleY -= DISPLAY_HEIGHT;
    yegMiddleY = constrain(yegMiddleY, 0, 2048-240);
    redrawCursor(ILI9341_RED); // won't see a "freeze effect" because the draw transition happens from top to bottom
    lcd_image_draw(&yegImage, &tft,
     yegMiddleX, yegMiddleY,
     0, 0,
     DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
     cursorX = (DISPLAY_WIDTH - 48)/2;
     cursorY = DISPLAY_HEIGHT/2;
  }
  // let's go one screen down from above
  else if (cursorY == (239 - CURSOR_SIZE/2) && yegMiddleY < 2048-240) {
    yegMiddleY += DISPLAY_HEIGHT;
    yegMiddleY = constrain(yegMiddleY, 0, 2048-240);
    redrawCursor(ILI9341_RED);
    lcd_image_draw(&yegImage, &tft,
     yegMiddleX, yegMiddleY,
     0, 0,
     DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
     cursorX = (DISPLAY_WIDTH - 48)/2;
     cursorY = DISPLAY_HEIGHT/2;
  }
  redrawCursor(ILI9341_RED);
  delay(20);
}

void modeTwoProcessJoystick() {
  while (digitalRead(JOY_SEL) == HIGH) {
    int yVal = analogRead(JOY_VERT);
    // the mods allows us to manage bounds that are only within 30
    if (yVal < JOY_CENTER - JOY_DEADZONE && (selectedRest % 30) > 0) { // up scroll
      selectedRest--; // global increments
      drawName(selectedRest+1);
      drawName(selectedRest);
    }
    else if (selectedRest != newtracker-1 &&
              yVal > JOY_CENTER + JOY_DEADZONE && (selectedRest % 30) < 29) { // down scroll
      selectedRest++;
      drawName(selectedRest-1); // draw old string normally
      drawName(selectedRest);   // highlight new string
    }
    // further down scroll with last item restriction
    else if (yVal > JOY_CENTER + JOY_DEADZONE && (selectedRest % 30) == 29) {
      selectedRest++;
      displayNames(true, false);
    }
    // further up scroll
    else if (selectedRest != 0 && yVal < JOY_CENTER - JOY_DEADZONE && (selectedRest % 30) == 0) {
      selectedRest -= 30;
      displayNames(false, false);
    }
  }
  restaurant r3;
  getRestaurantFast(rest_dist[selectedRest].index, &r3);
  tft.fillScreen(ILI9341_BLACK); // erases long names from the 48 black pixels

  // restaurant is outside above
  if (lat_to_y(r3.lat) < 0) {
    lcd_image_draw(&yegImage, &tft,
      lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2, 0,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH - 48)/2;
      cursorY = 0 + CURSOR_SIZE/2;
      // reorients middle of map based on the new image draw
      yegMiddleX = lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2;
      yegMiddleY = 0;
    }
  // restaurant is outside below
  else if (lat_to_y(r3.lat) > 2048) {
    lcd_image_draw(&yegImage, &tft,
      lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2, 2048-DISPLAY_HEIGHT,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH - 48)/2;
      cursorY = 2048-CURSOR_SIZE/2-1;
      yegMiddleX = lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2;
      yegMiddleY = 2048-DISPLAY_HEIGHT;
  }
  // restaurant is too close to the boundary above
  else if (lat_to_y(r3.lat) < DISPLAY_HEIGHT/2 && lon_to_x(r3.lon) < 2048-(DISPLAY_WIDTH-48)/2) {
    lcd_image_draw(&yegImage, &tft,
      lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2, 0,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH - 48)/2;
      cursorY = lat_to_y(r3.lat);
      yegMiddleX = lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2;
      yegMiddleY = 0;
  }
  // restaurant is too close to the boundary below
  else if (lat_to_y(r3.lat) > 2048-DISPLAY_HEIGHT/2) {
    lcd_image_draw(&yegImage, &tft,
      lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2, 2048-DISPLAY_HEIGHT,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH - 48)/2;
      cursorY = DISPLAY_HEIGHT/2 + (lat_to_y(r3.lat) - 1928);
      yegMiddleX = lon_to_x(r3.lon)-(DISPLAY_WIDTH-48)/2;
      yegMiddleY = 2048-DISPLAY_HEIGHT;
  }
  // restaurant is outside to the left
  else if (lon_to_x(r3.lon) < 0) {
    lcd_image_draw(&yegImage, &tft,
      0, lat_to_y(r3.lat)- DISPLAY_HEIGHT/2,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = 0 + CURSOR_SIZE/2;
      cursorY = DISPLAY_HEIGHT/2;
      yegMiddleX = 0;
      yegMiddleY = lat_to_y(r3.lat)- DISPLAY_HEIGHT/2;
  }
  // restaurant is outside to the right
  else if (lon_to_x(r3.lon) > 2048) {
    lcd_image_draw(&yegImage, &tft,
      // accounts for the extra 48 pixels in advance
      2048 + 48 - DISPLAY_WIDTH, lat_to_y(r3.lat)- DISPLAY_HEIGHT/2,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = DISPLAY_WIDTH-48- CURSOR_SIZE/2 - 1; // edge of display
      cursorY = DISPLAY_HEIGHT/2;
      yegMiddleX = 2048 + 48 - DISPLAY_WIDTH;
      yegMiddleY = lat_to_y(r3.lat)- DISPLAY_HEIGHT/2;
  }
  // restaurant is too close to the boundary on the left
  else if (lon_to_x(r3.lon) < (DISPLAY_WIDTH - 48)/2) {
    lcd_image_draw(&yegImage, &tft,
      0, lat_to_y(r3.lat)- DISPLAY_HEIGHT/2,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = lon_to_x(r3.lon);
      cursorY = DISPLAY_HEIGHT/2;
      yegMiddleX = 0;
      yegMiddleY = lat_to_y(r3.lat)- DISPLAY_HEIGHT/2;
  }
  // restaurant is too close to the boundary on the right
  else if (lon_to_x(r3.lon) > 2048-(DISPLAY_WIDTH-48)/2) {
    lcd_image_draw(&yegImage, &tft,
      2048 + 48 - DISPLAY_WIDTH, 0,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH-48)/2 + (lon_to_x(r3.lon)-1912);
      cursorY = lat_to_y(r3.lat);
      yegMiddleX = 2048 + 48 - DISPLAY_WIDTH;
      yegMiddleY = 0;
  }
  // normal case
  else {
    lcd_image_draw(&yegImage, &tft,
      lon_to_x(r3.lon)-(DISPLAY_WIDTH - 48)/2, lat_to_y(r3.lat)-DISPLAY_HEIGHT/2,
      0, 0,
      DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
      cursorX = (DISPLAY_WIDTH - 48)/2;
      cursorY = DISPLAY_HEIGHT/2;
      yegMiddleX = lon_to_x(r3.lon)-(DISPLAY_WIDTH - 48)/2;
      yegMiddleY = lat_to_y(r3.lat)-DISPLAY_HEIGHT/2;
    }

  redrawCursor(ILI9341_RED);
}

int main() {
  setup();
  drawcircles();
  while (true) {
    if (check()) {modeOneProcessJoystick();} // avoids flicker
    if (checkselect()) {clearselect();}
    if (digitalRead(JOY_SEL) == LOW) {
      while (digitalRead(JOY_SEL) == LOW) {} // for proper button press
      restausort();
      selectedRest = 0; // set to the first item on the list
      j = 30; // incrementer reset
      displayNames(true, true);
      modeTwoProcessJoystick();
      drawcircles(); // remembers previous selection
    }
  }
  Serial.end();
  return 0;
}
