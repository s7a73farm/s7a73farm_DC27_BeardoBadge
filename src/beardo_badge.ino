/* s7a73farm Beardo Badge DC27 Badge
Badge design and concept
https://twitter.com/s7a73farm
Badge assembly and support
https://twitter.com/L1ttl3_w1tch
My wife and partner with out here there would be no badge!

Badge engineering work https://twitter.com/compukidmike
Back his projects he is PCB magician 

Code Contributions by https://twitter.com/Made2Glow
Please support this maker and buy his stuff!

https://twitter.com/gr3yR0n1n
Please follow him on Twitter he does many amazing things


*/
#include <FastLED.h>
#include <Goertzel.h>


#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontRobotron.h>

#define LED_PIN        21
#define COLOR_ORDER    GRB
#define CHIPSET        WS2812B

#define MATRIX_WIDTH   16
#define MATRIX_HEIGHT  -7
#define MATRIX_TYPE    HORIZONTAL_MATRIX

cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> ledMatrix;

cLEDText ScrollingMsg;

const unsigned char TxtDemo[] = { EFFECT_FRAME_RATE "\x04" 
                                  EFFECT_HSV_AH "\x00\xff\xff\xff\xff\xff" "           s7a73farm"};
                                  //EFFECT_HSV_AH "\x00\xff\xff\xff\xff\xff" "           AND LIFE IS RYTHIM!!  " };



//#define NUM_LEDS              112  // How many LEDs. 26 for Small 35 for Large.
#define MAX_MA                200 // Calculated milliamp brightness cut off in milliamps
#define BRIGHTNESS_THREASHOLD 40    // The default setting on a reset, and where to place the red marker on brightness select sequence.
#define ROW  7    
#define COL  16   
#define NUM_LEDS      (ROW*COL)



// ======== [Hardware Setup]=======
#define DATA_PIN              21   // (Pin 6 For batch of 25 sold at ANW2018) Pin for your WS2812b LEDs
#define BUTTON_1              17  // Connect this to your button, other wire to ground.
#define BUTTON_2              18
#define BUTTON_3              19
#define BUTTON_PIN_GND        1  // Using 7 as a ground for the button. Makes construction easier. But you don't have to use this, just put your button to a ground. 
#define MIC_PIN               33  // Microphone pin. I used a MAX9814 auto gain mic. Search for it on Aliexpress they are cheap.
#define GAIN                  40  // Gain Pin on the MAX9814 microphone. HIGH: 40 dB LOW: 50 dB Float: 60 dB

// ======== [Internal Setup]=======

#define NUM_SEQUENCE          22    // How many sequences/modes we got.
#define SAMPLING_FREQUENCY    8900  // Hz Don't change this unless you want to break everything!
#define NUM_SAMPLES           36    // 36 typical. High for better resolution, but loop speed suffers.
#define TARGET_FREQUENCY      70    // 70 Hz what Frequency you want to match. You only get to do one!
#define BEAT_THRESHOLD        100   // 128 Seems to work best. 0 - 255. Higher = less beat potential.
#define AUTOSEQUENCE_DELAY    40000 // ms
#define NUM_DISPLAY           6     // How long the led menu is when switching sequences.

// ======== [Global Variables]=======
bool _ButtonNow = false;      // Instantanious state always on, like Hold. But without the delay.
bool _ButtonPress = false;    // Simular to Now, but will only be true for one frame. You gotta Press and let go like a tap for this to work.
bool _ButtonHold = false;// This will be true, but will bypass Press. So you can do trigger Hold events without Press events happening. Now will still become true however.

bool _ButtonNow2 = false;
bool _ButtonHold2 = false;
bool _ButtonPress2 = false;

bool _ButtonNow3 = false;
bool _ButtonHold3 = false;
bool _ButtonPress3 = false;

bool _runonce = true;         // Used for when going into an animation function for the first time to setup one time things.
uint8_t num[2];               // two nums to advance animation mechanics.
uint8_t hue,sat,val;          // global hue, saturation, value vars
uint8_t _brightness = 64;    // global _brightness to be changed on the fly. I dunno if I use this, but its here anyways.
uint32_t animtime = millis(); // important var to store the time to keep all the animations running nicely.
uint8_t _savedhue;            // This is used for EEPROM handling.
uint8_t _savedsequence;       // This is used for EEPROM handling.
uint8_t _freq =         0;    // Magnitude of Target Frequency
uint8_t _freqmax =      0;    // On the fly adjusted max value of the freq magnitude. Use this in your map statements for the maximum value. It keeps your music animations interesting in varrying loudness environments.
bool    _beat =         false;// true if Beat.
bool    _music =        true;// true if music is detected. 
bool    _useMusic =     true; // Global switch to turn music on or off.
uint32_t _time = millis();
bool _blend = false;


CRGB leds[NUM_LEDS];          // LED buffer
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }


Goertzel goertzel = Goertzel(TARGET_FREQUENCY, NUM_SAMPLES, SAMPLING_FREQUENCY); // For digital audio processing
float magnitude = 0;


void ProcessGoertzel() {
  // Lots of filtering and processing here. The signal comes in with a wave form. 
  // We chop off the lower half and process the averages all the time. 
  // In theory different audio sources can be fed in and this will auto adjust for the signal.
  // Not tested with anything other than a MAX9814.
  
  static uint16_t centermag = 5000; 
  static uint16_t centermagmax = 5600;
  static uint16_t maxmag = 9000;  
  static uint8_t count = 0;
  static uint32_t avg = 0;
  uint8_t freq;
  static uint32_t music_time = millis();
  static uint16_t music_count = 0;
  static uint16_t gain_count = 0;

  goertzel.sample(MIC_PIN); // Get analog samples.
  //float magnitude = goertzel.detect(); // Magic happens here.
  magnitude = goertzel.detect(); // Magic happens here.

  // Process Max Magnitude variable. Drop it if levels get lower.
  if (magnitude > maxmag) maxmag = magnitude;
  else if (maxmag > centermag*1.9) maxmag -= 30; // 1.9

  // Process average magnitude, using 100 samples.
  if (count < 100) { 
    avg += magnitude;
    count++;
  } else {
    count = 0;
    centermag = avg/100;
    avg = 0;
  }
  // Averaging the center of the wave form. It keeps moving!
  if (centermag > centermagmax) centermagmax = centermag;
  else if (centermagmax > centermag) centermagmax--;
  
  // Scale the magnitude into a 1 byte var for easy LED animating.
  if (magnitude > centermagmax && magnitude < maxmag) {
    freq = map(magnitude,centermagmax,maxmag,0,255);
    if (_freq < freq) _freq = freq;
    else if (_freq > 16) _freq -= 16;
  }
  // Update the frequency maximum value.
  if(_freq > _freqmax) _freqmax = _freq;
  else if (_freqmax > 224) _freqmax--;
  
  // Process beat detection. Not very complex, but it vwerks!
  if (_freq > BEAT_THRESHOLD) _beat = true;
  else  _beat = false;

  // 'Music' detection. If you can comment this all out and make _music = true to troubleshoot.
  if (freq > 32) {
    music_count++;
    if (music_count > 2) {
      _music = true;
      music_count = 0;
      music_time = millis();
    }
  } else {
    if (music_count > 0) music_count--;
  }
  if (millis() - music_time > 10000) {
    _music = false;
  }
  if (!_music) _freqmax = 255;
}

// Button processing.
void GetButtonInput() {
  static uint16_t count = 0;
  _ButtonNow = false;
  _ButtonPress = false;
  if (!digitalRead(BUTTON_1)) {
    _ButtonNow = true;
    count++;
  } else {
    if (count > 2 && count < 20) _ButtonPress = true;
    count = 0;
  }
  if (count > 20)  _ButtonHold = true;
  else _ButtonHold = false;
}

void GetButtonInput2(){
  static uint16_t count = 0;
  _ButtonNow2 = false;
  _ButtonPress2 = false;
  if (!digitalRead(BUTTON_2)) {
    _ButtonNow2 = true;
    count++;
  } else {
    if (count > 2 && count < 20) {
      _ButtonPress2 = true;
    }
    count = 0;
  }
  if (count > 20) {
    _ButtonHold2 = true;
  } else {
    _ButtonHold2 = false;
  } 
}

void GetButtonInput3(){
  static uint16_t count = 0;
  _ButtonNow3 = false;
  _ButtonPress3 = false;
  if (!digitalRead(BUTTON_3)) {
    _ButtonNow3 = true;
    count++;
  } else {
    if (count > 2 && count < 20) {
      _ButtonPress3 = true;
    }
    count = 0;
  }
  if (count > 20) {
    _ButtonHold3 = true;
  } else {
    _ButtonHold3 = false;
  } 
}


// Fade all the LEDs by scale. 0 = total blackout of the strip in on frame. 255 = nothing will happen. I usually go with 230-254 for a nice smooth fade.
void fadeall(uint8_t scale) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(scale);
  }
}

// This for the boot up menu 
void displaymenu(uint8_t hue, uint8_t sat,uint8_t val) {
  for (uint16_t i = 0; i < 8%NUM_LEDS; i++) {
    leds[i] = CHSV(hue,sat,val);
  }
}

// Set the whole strip to your choice of Hue, Saturation, or Value.
void displayall(uint8_t hue, uint8_t sat,uint8_t val) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue,sat,val);
  }
}

// This displays a bar, used for boot up menu systems.
void DisplayBar(uint8_t num) {
  displayall(num*64+63,255,200);
  
  for (uint16_t i = 0; i < 4; i++) {
    leds[i] = CHSV(0,0,64);
  }
  leds[num] = CHSV(96,255,200);
}

// Show the menu when you press the button to change the sequence.
void DisplayMode(uint8_t num) {
  uint8_t hue = 0;
  //displayall(0,0,32);
  switch (num/NUM_DISPLAY) {
    case 0: hue = 0; break;
    case 1: hue = 96; break;
    case 2: hue = 160; break;
    default: hue = 64; break;
  }
  for (uint8_t i = 0; i < NUM_DISPLAY; i++) {
    leds[i] = CHSV(0,0,32);
  }
  for (uint8_t i = 0; i <= num%NUM_DISPLAY; i++) {
    leds[i] = CHSV(hue,255,127);
  }
}

// This fun function shifts the data in the LED buffer by one pixel. You can select the direction and if the data wraps or not.
void ColorPusher(bool dir, bool wrap) { 
  CRGB templed[1];
  if (dir) {
    templed[0] = leds[0];
    for (uint16_t i = 0; i < NUM_LEDS;i++) {
      leds[i] = leds[i+1];
    }
    if (wrap) leds[NUM_LEDS-1] = templed[0]; 
    else leds[NUM_LEDS-1] = CRGB(0,0,0);
  } else {
    templed[0] = leds[NUM_LEDS-1];
    for (uint16_t i = NUM_LEDS-1; i > 0;i--) {
      leds[i] = leds[i-1];
    }
    if (wrap) leds[0] = templed[0];
    else leds[0] = CRGB(0,0,0);
  }
}

// Flashes!
void flash(uint16_t flashdelay, uint8_t hue, uint8_t sat, uint8_t val) {
  static bool flipflop = true;
  static uint32_t time = millis();
  if (millis() - time > flashdelay) {
    time = millis();
    flipflop = !flipflop;
  }
  if (flipflop) displayall(hue,sat,val);
  else LEDS.clear();
}

#define BRIGHT_MAX    255
#define BRIGHT_MIN    16

void ChangeBrightness() {
  static uint8_t num = 0;
  if (_ButtonNow2) {
    if (_brightness > BRIGHT_MIN) _brightness --;
    num = 255;
    FastLED.setBrightness(_brightness);
  }

  if (_ButtonNow3) {
    if (_brightness < BRIGHT_MAX) _brightness ++;
    num = 255;
    FastLED.setBrightness(_brightness);
  }
  
  if (num > 0) {
    num--;
    for (int8_t x = 0; x < COL;x++) {
      for (int8_t y = 0; y < ROW;y++) {
        if (y > ROW/2) writePixel(x,y,0,0,255,false);
        else writePixel(map(_brightness,BRIGHT_MIN,BRIGHT_MAX,0,COL-1),y,96,255,255,false);

      }
    }
  }
} 
//------------------- GFX Functions ------------------------
void writePixel(int16_t x, int16_t y, 
  uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  if (wrap) {
    
    x = x % COL;
    y = y % ROW;

    if (x < 0) x = COL + x;
    if (y < 0) y = ROW + y;
  } else {
    if ((x < 0) || (y < 0) || (x >= COL) || (y >= ROW)) 
      return;
    
  }
  if (_blend)
    leds[x+(y*COL)] |= CHSV(hue,sat,val);
  else
    leds[x+(y*COL)] = CHSV(hue,sat,val);
}


void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      writePixel(y0,x0,hue,sat,val,wrap);
    } else {
      writePixel(x0,y0,hue,sat,val,wrap);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void writeFastVLine(int16_t x, int16_t y,
  int16_t h, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  writeLine(x,y,x,y+h-1,hue,sat,val,wrap); 
}

void writeFastHLine(int16_t x, int16_t y,
  int16_t w, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  writeLine(x,y,x+w-1,y,hue,sat,val,wrap); 
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
      uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  if(x0 == x1){
    if(y0 > y1) _swap_int16_t(y0, y1);
    writeFastVLine(x0, y0, y1 - y0 + 1, hue,sat,val,wrap);
  } else if(y0 == y1){
    if(x0 > x1) _swap_int16_t(x0, x1);
    writeFastHLine(x0, y0, x1 - x0 + 1, hue,sat,val,wrap);
  } else {
    writeLine(x0, y0, x1, y1, hue,sat,val,wrap);
  }
}

void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
  uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  fillRect(x,y,w,h,hue,sat,val,wrap);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
        uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  
  for (int16_t i=x; i<x+w; i++) {
      writeFastVLine(i, y, h, hue,sat,val,wrap);
  }
}
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
  uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  writeFastHLine(x, y, w, hue,sat,val,wrap);
  writeFastHLine(x, y+h-1, w, hue,sat,val,wrap);
  writeFastVLine(x, y, h, hue,sat,val,wrap);
  writeFastVLine(x+w-1, y, h, hue,sat,val,wrap);
}

void fillScreen(uint8_t hue, uint8_t sat, uint8_t val) {
    fillRect(0, 0, COL, ROW, hue,sat,val,true);
}

void drawCircle(int16_t x0, int16_t y0, int16_t r,
  uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  writePixel(x0  , y0+r, hue,sat,val,wrap);
  writePixel(x0  , y0-r, hue,sat,val,wrap);
  writePixel(x0+r, y0  , hue,sat,val,wrap);
  writePixel(x0-r, y0  , hue,sat,val,wrap);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    writePixel(x0 + x, y0 + y, hue,sat,val,wrap);
    writePixel(x0 - x, y0 + y, hue,sat,val,wrap);
    writePixel(x0 + x, y0 - y, hue,sat,val,wrap);
    writePixel(x0 - x, y0 - y, hue,sat,val,wrap);
    writePixel(x0 + y, y0 + x, hue,sat,val,wrap);
    writePixel(x0 - y, y0 + x, hue,sat,val,wrap);
    writePixel(x0 + y, y0 - x, hue,sat,val,wrap);
    writePixel(x0 - y, y0 - x, hue,sat,val,wrap);
  }
}

void drawCircleHelper( int16_t x0, int16_t y0,
    int16_t r, uint8_t cornername, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      writePixel(x0 + x, y0 + y, hue,sat,val,wrap);
      writePixel(x0 + y, y0 + x, hue,sat,val,wrap);
    }
    if (cornername & 0x2) {
      writePixel(x0 + x, y0 - y, hue,sat,val,wrap);
      writePixel(x0 + y, y0 - x, hue,sat,val,wrap);
    }
    if (cornername & 0x8) {
      writePixel(x0 - y, y0 + x, hue,sat,val,wrap);
      writePixel(x0 - x, y0 + y, hue,sat,val,wrap);
    }
    if (cornername & 0x1) {
      writePixel(x0 - y, y0 - x, hue,sat,val,wrap);
      writePixel(x0 - x, y0 - y, hue,sat,val,wrap);
    }
  }
}

void drawRoundRect(int16_t x, int16_t y, int16_t w,
  int16_t h, int16_t r, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  // smarter version
  writeFastHLine(x+r  , y    , w-2*r, hue,sat,val,wrap); // Top
  writeFastHLine(x+r  , y+h-1, w-2*r, hue,sat,val,wrap); // Bottom
  writeFastVLine(x    , y+r  , h-2*r, hue,sat,val,wrap); // Left
  writeFastVLine(x+w-1, y+r  , h-2*r, hue,sat,val,wrap); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, hue,sat,val,wrap);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, hue,sat,val,wrap);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, hue,sat,val,wrap);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, hue,sat,val,wrap);
}

void fillRoundRect(int16_t x, int16_t y, int16_t w,
  int16_t h, int16_t r, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  // smarter version
  writeFillRect(x+r, y, w-2*r, h, hue,sat,val,wrap);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, hue,sat,val,wrap);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, hue,sat,val,wrap);
}

void fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
  uint8_t cornername, int16_t delta, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {

  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      writeFastVLine(x0+x, y0-y, 2*y+1+delta, hue,sat,val,wrap);
      writeFastVLine(x0+y, y0-x, 2*x+1+delta, hue,sat,val,wrap);
    }
    if (cornername & 0x2) {
      writeFastVLine(x0-x, y0-y, 2*y+1+delta, hue,sat,val,wrap);
      writeFastVLine(x0-y, y0-x, 2*x+1+delta, hue,sat,val,wrap);
    }
  }
}

void fillCircle(int16_t x0, int16_t y0, int16_t r,
    uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
    
  writeFastVLine(x0, y0-r, 2*r+1, hue,sat,val,wrap);
  fillCircleHelper(x0, y0, r, 3, 0, hue,sat,val,wrap);   
}
// 0 = top; 1 = left; 2 = right; of triangle.
void drawTriangle(int16_t x0, int16_t y0,
    int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t hue, uint8_t sat, uint8_t val, bool wrap) {
  
  drawLine(x0, y0, x1, y1, hue,sat,val,wrap);
  drawLine(x1, y1, x2, y2, hue,sat,val,wrap);
  drawLine(x2, y2, x0, y0, hue,sat,val,wrap);
}

// ---------------------- END GFX functions ---------------------------

// =====================================================================
// ============================= MODES =================================
// =====================================================================
// ============================= Modes - Begin =================================


// Looky eyes
void Mode23() {
 
  static uint8_t val = 64;
  static int8_t num = 0;
  static uint8_t num1 = 0;
  
   FastLED.clear();
   if (_beat) {
    val = 128;
    num = random8();
    num1++;
  } else {
    num++;
    val = 32;  
  }
  //num1++;
  num = map(sin8(num1),0,255,((ROW/2)-ROW)/2,(ROW-(ROW/2))/2);

  for (int8_t x = 0; x < COL;x++) {
    for (int8_t y = 0; y < ROW;y++) {
      writePixel(x,y,x*(255/COL)+num1,255,val,false);
      
    }
  }
  
  fillCircle(COL/4-1,ROW/2,ROW-4,190,255,0,false); // Big eye left
  fillCircle(COL/4-1+num,ROW/2,1,96,0,val,false); // Pupil eye left
  drawCircle(COL/4-1,ROW/2,ROW-4,190,255,val,false); // Big eye left
  
  fillCircle((COL/4)+(COL/2),ROW/2,ROW-4,190,255,0,false); // Big eye right
  fillCircle((COL/4)+(COL/2)+num,ROW/2,1,160,0,val,false); // Pupil eye right
  drawCircle((COL/4)+(COL/2),ROW/2,ROW-4,190,255,val,false); // Big eye right
}



//Triangle
void Mode01 () {
  
  if (!_music) num[0] += NUM_LEDS/10;
  else num[0] += NUM_LEDS/4;

  if (num[0] == 0)
    hue = random8();
   // fadeall(1);
  //static uint8_t hue = 0;
  static uint8_t sat = 255;
  static uint8_t val = 32;
  uint8_t num = 0;

  static uint8_t count = 0; // To prevent crazy color change on a loud 'hummm' sound.
  if (_beat && count == 0) {
    count = 255;
  }
  if (count > 0) count--; 
  hue++;
  num = count;
  drawTriangle(
        map(sin8(num),0,255,1,30), //top, left
        map(sin8(num),0,255,6,6), //left, right
        map(sin8(num),0,255,30,7), //right top
        map(sin8(num),0,255,0,0),
        map(sin8(num),0,255,8,15),
        map(sin8(num),0,255,0,0),
        hue,sat,val,true);  
}

//Make It Rain
void Mode02() {
  static uint8_t rain[COL];
  static uint8_t hue = 0;
  static uint8_t sat = 134;
  static uint8_t val = 89;
   static uint8_t count = 89; // To prevent crazy color change on a loud 'hummm' sound.
  if (_beat && count == 0) {
    count = 255;
    //hue ++;
  }
  if (count > 0) count--; 
  
  fadeall(240);
  
  for (uint8_t i = 0; i < COL;i++) {
    if (random8() < 64)
      rain[i]++;
    if (rain[i] >= ROW)
      rain[i] = 0;
    leds[i+(rain[i]*COL)] = CHSV(i*10+sin8(hue),sat,count);
  }
}

// O Rigns
void Mode03 () {
  
  static uint8_t hue = 0;
  static uint8_t sat = 255;
  static uint8_t val = 128;
  static int8_t randcircle[6][3];
  static uint8_t sizecircle[6][2];

  static uint8_t count = 0; // To prevent crazy color change on a loud 'hummm' sound.
  if (_beat && count == 0) {
    count = 255;
    //hue ++;
  }
  if (count > 0) count--; 

  if (_beat) {
    _blend = true;
    fadeall(224);
    if (millis() - _time > 50) {
      _time = millis();    
      for (uint8_t i = 0; i < 6;i++) {
        sizecircle[i][1]++;
        if(sizecircle[i][1] > sizecircle[i][0]) {
          sizecircle[i][1] = 0;
          randcircle[i][0] = random(-5,ROW+5);
          randcircle[i][1] = random(-5,COL+5);
          randcircle[i][2] = random8();
          sizecircle[i][0] = random(10,60);
        } 
        drawCircle(randcircle[i][0],randcircle[i][1],sizecircle[i][1],hue*(i+10)+randcircle[i][2],sat,val,false);
      }
    }
  }
}

//music rings
void Mode04 () {

  static uint8_t hue[6] = {random8(),random8(),random8(),random8(),random8(),random8()};
  static uint8_t sat = 255;
  static uint8_t val = 128;
  static uint8_t num = 0;
  
  FastLED.clear();
  Serial.println(num);
  if (num == 64) {
     for(uint8_t i = 0; i < 6; i++) {
      hue[i] = random8();
    }
    
  } 
  
  static uint8_t count = 0; // To prevent crazy color change on a loud 'hummm' sound.
  if (_beat && count == 0) {
    //hue += 32;
    count = 255;
  }
  
    num++;
    if (count > 0) count--; 
  
  drawCircle(8,0,map(sin8(count),0,255,0,50),hue[0],sat,val,false);
  drawCircle(17,0,map(sin8(count),0,255,0,30),hue[1],sat,val,false);
  drawCircle(26,0,map(sin8(count),0,255,0,25),hue[2],sat,val,false);
  drawCircle(35,0,map(sin8(count),0,255,0,25),hue[3],sat,val,false);
  drawCircle(42,0,map(sin8(count),0,255,0,30),hue[4],sat,val,false);
  drawCircle(51,0,map(sin8(count),0,255,0,50),hue[5],sat,val,false);
}

// convert x/y cordinates to LED index on zig-zag grid
uint16_t getIndex(uint16_t x, uint16_t y)
{
  uint16_t index;
  if (y == 0)
  {
    index = x;
  }
  else if (y % 2 == 0)
  {
    index = y * 16 + x;
  }
  else
  {
    index = ((y * 16) + (16-1)) - x;
  }
  return index;
 } 

// Pixel scroll change to random color.
void Mode05() {
  static uint8_t count = 0; // To prevent crazy color change on a loud 'hummm' sound.
  if (!_music) num[0] += NUM_LEDS/10;
  else num[0] += NUM_LEDS/4;

  if (num[0] == 0)
    hue = random8();

  if (_beat && count == 0) {
    hue += 32;
    count = 20;
  }
  if (count > 0) count--; 

  displayall(_savedhue == 0?hue:_savedhue,255,200);
  if (_beat) num[0] = 127;

  leds[map(num[0],0,255,0,NUM_LEDS-1)] = CHSV(0,0,255);
  leds[map(num[0],0,255,NUM_LEDS-1,0)] = CHSV(0,0,255);
}

// RGB
void Mode06() {
  if (!_music) {
    _freq = 255; 
    hue++;
  } else {
    hue += map(_freq,0,_freqmax,0,20);
  }
  
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV (hue+i*4,255,255);
  }
}

// RGB fade
void Mode07() {
  if (!_music) {
    hue++;
    num[0]++;
  } else if (_beat){
    hue+=8;
    num[0]+=8;
  }
  
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue+i*8,255,map(sin8(i*4+num[0]),0,255,8,255));

  }
}

// Stacker
void Mode08() {
  if (_runonce) {_runonce = false; num[0] = 0; num[1] = NUM_LEDS-1; hue = _savedhue;}
  if (_music) {
    if (_beat) num[0]++;
  } else {
    num[0]++;
  }
  if (num[0] > num[1]) {
    num[0] = 0;
    num[1]--;
    if (_savedhue == 0) 
      hue += 8;
    else
      hue = num[1]%2 == 0?_savedhue:(_savedhue+16);
  }

  if (num[1] == 0) {
    num[1] = NUM_LEDS-1;
  }
  if (num[0] != 0) leds[num[0]-1] = CHSV(0,0,0);
  leds[num[0]] = CHSV(hue,num[0] == num[1]?255:0,200);
} 

// Two color random pixels
void Mode09() {
  static uint32_t time = millis();
  if (!_music) _freq = 100;
  
  if (millis() - animtime > 1000) {
    animtime = millis();
    hue = random8();
  }
  fadeall(200);
  if (millis() - time > map(_freq,0,_freqmax,250,0)) {
   time = millis();
   for (uint8_t i = 0; i < map(_freq,0,_freqmax,1,25); i++ ) {
      if (_savedhue == 0) {
        leds[map(random8(),0,255,0,NUM_LEDS-1)] = CHSV (hue,255,255);
        leds[map(random8(),0,255,0,NUM_LEDS-1)] = CHSV (hue+64,255,255);
      } else {
        leds[map(random8(),0,255,0,NUM_LEDS-1)] = CHSV (_savedhue,255,255);
        leds[map(random8(),0,255,0,NUM_LEDS-1)] = CHSV (_savedhue+64,255,255);
      }
    }
  }
  
}

// RGB sine
void Mode10() {
  hue++;
  if (!_music) num[0]++;
  else num[0] += map(_freq,0,_freqmax,0,4);
  
  leds[map(sin8(num[0]),0,255,0,NUM_LEDS-1)] = CHSV(_savedhue == 0?hue:_savedhue,255,255);
  fadeall(240);
}

// Solid one color RGB cycle.
void Mode11() { 
  if (!_music) _freq = 255;
  hue++;
  displayall(_savedhue == 0?hue:_savedhue,255,map(_freq,0,_freqmax,0,255));
}

// Half color scrolling
void Mode12() {
  if (!_music) _freq = 128;
  if (millis() - animtime > 200) {
    animtime = millis();
    num[0]++;
  }
  if (_beat) num[0]++;
  if (num[0]%NUM_LEDS == 8) hue += 8;

  if (num[0] > NUM_LEDS) num[0] = 0;
  
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i > NUM_LEDS/2)
      leds[(i+num[0])%NUM_LEDS] = CHSV(_savedhue == 0?hue:_savedhue,255,32);
    else
      leds[(i+num[0])%NUM_LEDS] = CHSV(_savedhue == 0?hue:_savedhue,255,255);
  }
}

// Pusher
void Mode13() {
  
  if (!_music) {
    if (random8() > 240) _beat = true;
    else _beat = false;
  }
  ColorPusher(false,false);
  if (_beat) {
    if (_savedhue == 0) {
      num[0] += 8;
    } else {
      
    }
    leds[0] = CHSV(_savedhue == 0?num[0]:_savedhue,255,255);
  } 
}

// Strobe.
void Mode14() {
  static uint8_t pos[4];
  
  displayall(_savedhue == 0?hue:_savedhue,255,100);
  if (pos[0] == 0) hue++;
  if (_music) {
    for(uint8_t i = 0; i < 4; i++) {
      if (_beat) pos[i] = map(random8(),0,255,0,NUM_LEDS-1);
      leds[pos[i]] = CHSV(_savedhue == 0?127-hue:127-_savedhue,255,255);
    }
  }
}

// White scroll.
void Mode15() {
  if (!_music) {
    _freq = 100;
    _freqmax = 100;
  }
  if (millis() - animtime > map(_freq,0,_freqmax,100,0) || _ButtonHold) {
    animtime = millis();
    if (_music) num[0]+=32;
    else num[0]++;
  }
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV (_savedhue,_savedhue == 0?0:255,map(sin8(i*(NUM_LEDS/2)+num[0]),0,255,32,255));
  }
}

// Middle VU meter
void Mode16() {
  static uint8_t myfreq;
  if (!_music) {
    myfreq = sin8(num[0]++);
    _freqmax = 255;
  }
  else myfreq = _freq;
  
  fadeall(200);
  for (uint16_t i = 0; i < map(myfreq,0,_freqmax,0,NUM_LEDS/2); i++) {
    leds[(NUM_LEDS/2-1)-i] = CHSV(i*(128/(NUM_LEDS/2))+_savedhue,255,255);
    leds[NUM_LEDS/2+i] = CHSV(i*(128/(NUM_LEDS/2))+_savedhue,255,255);
  }
}

// Breath in/out of color of choice.
void Mode17() {
  if (!_music) {
    num[0]++;
    if (num[0] == 0) hue += 8;
    displayall(_savedhue,255,map(sin8(num[0]),0,255,16,255));
  }
  else {
    if (num[0] > 8) num[0] -= 8;
    if (_beat) {
      num[0] = 127;
      //hue++;// random8();
    }
    displayall(_savedhue,255,num[0]);
  }
}

// Multiple Sine waves
void Mode18() { 
  if (!_music) {
    _freq = 255;
    _freqmax = 255;
  }
  fadeall(240);
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(hue, 255, map(_freq,0,_freqmax,16,255));
    hue += 32;
  }
}

// Multiple colored dots appear on a beat.
void Mode19() { 
  if (!_music) _freq = num[0]++;
  fadeall(245);
  
  if (_freq > 150) {
    for (uint8_t i = 0; i < 3; i++) {
      leds[random(0,NUM_LEDS-1)] = CHSV(random8(),255,255);
    }
  }
}

// VU meter
void Mode20() {
  static uint8_t myfreq;
  if (!_music) {
    myfreq = sin8(num[0]++);
    _freqmax = 255;
  }
  else myfreq = _freq;
  fadeall(200);
  for (uint16_t i = 0; i < map(myfreq,0,_freqmax,0,NUM_LEDS); i++) {
    leds[i] = CHSV(i*(200/NUM_LEDS)+_savedhue,255,255);
  }
}





// Matrix Rain
void Mode21(){
  static uint8_t count = 0; // To prevent crazy color change on a loud 'hummm' sound.
  
  if (_beat && count == 0) {
    count = 23;
    //hue ++;
  }
  if (count > 0) count--; 
  
  if (_music) {
  EVERY_N_MILLIS(65) // falling speed
  {
    // move code downward
    // start with lowest row to allow proper overlapping on each column
    for (int8_t row=7-1; row>=0; row--)
    {
      for (int8_t col=0; col<16; col++)
      {
        if (leds[getIndex(col, row)] == CRGB(175,255,175))
        {
          leds[getIndex(col, row)] = CRGB(27,130,39); // create trail
          if (row < 7-1) leds[getIndex(col, row+1)] = CRGB(175,255,175);
        }
      }
    }

    // fade all leds
    for(int i = 0; i < NUM_LEDS; i++) {
      if (leds[i].g != 255) leds[i].nscale8(192); // only fade trail
    }

    // check for empty screen to ensure code spawn
    bool emptyScreen = true;
    for(int i = 0; i < NUM_LEDS; i++) {
      if (leds[i])
      {
        emptyScreen = false;
        break;
      }
    }

    // spawn new falling code
    if (random8(count) == 0 || emptyScreen) // lower number == more frequent spawns
    {
      int8_t spawnX = random8(16);
      leds[getIndex(spawnX, 0)] = CRGB(175,255,175 );
    }

  }
  }
}



// Crazy Eyes
void Mode22(){
  static uint8_t hue = 0;
  static uint8_t sat = 255;
  static uint8_t val = 64;
  static uint8_t num = 64;
  
  FastLED.clear();
  
  if (_beat) {
    val = 128;
    num = random8();
  } else {
    num++;
    val = 64;  
  }
  
  fillCircle(COL/4-1,ROW/2,map(num,0,255,0,ROW-4),_beat?0:96,_beat?255:0,val,false); // Pupil eye left
  drawCircle(COL/4-1,ROW/2,ROW-4,192,255,val,false); // Big eye left
  
  fillCircle((COL/4)+(COL/2),ROW/2,map(uint8_t(num+128),0,255,0,ROW-4),_beat?0:96,_beat?255:0,val,false); // Pupil eye right
  drawCircle((COL/4)+(COL/2),ROW/2,ROW-4,192,255,val,false); // Big eye right
}

// Scrolling Text
void Mode00() {
  
  if (ScrollingMsg.UpdateText() == -1) {
    ScrollingMsg.SetText((unsigned char *)TxtDemo, sizeof(TxtDemo) - 1);
  //} else {
    //FastLED.show();
  }
  delay(10);
}

// =====================================================================
// ============================= MODES END =============================
// =====================================================================

void setup() {
  Serial.begin(112500); // Uncomment this if you want to use the Serial for debugging.

  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds,NUM_LEDS); // Preset to using WS2812b LEDs.
  FastLED.setMaxPowerInVoltsAndMilliamps(5,MAX_MA); // 5 volts and MAX_MA
  
  FastLED.clear();
  delay(100);

  pinMode(BUTTON_1,INPUT_PULLUP); // Turn on the internal pullup resistor.
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  
  pinMode(BUTTON_PIN_GND,OUTPUT); // Setting the fake ground pin to OUTPUT.
  pinMode(GAIN,OUTPUT); // Gain control on the MAX9814 mic.
  
  delay(100);
  digitalWrite(BUTTON_PIN_GND,LOW); // Bring the fake ground to LOW. I do this so I can solder a button direct to the Nano board. Saves on wires and such.

  
  ledMatrix.SetLEDArray(leds);
  
  ScrollingMsg.SetFont(RobotronFontData);
  ScrollingMsg.Init(&ledMatrix, ledMatrix.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)TxtDemo, sizeof(TxtDemo) - 1);
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0x00, 0xff);
}

// Loop! 
void loop() {
  static uint8_t sequence = _savedsequence;
  static bool autoshow = false; //sequence == 0?true:false;
  static uint32_t time = millis();
  static uint32_t time_sequence = millis();
  static uint16_t countdown = 0;
  

  
  if (_useMusic) ProcessGoertzel(); // Magic happens here.
  else _music = false;
  
  if (millis() - time > 10) { // Wrap the animations in a 10 MS delay, without using the evil delay(). This leaves the loop to run as fast as I can to process audio!
    time = millis();
    GetButtonInput();
      
   
    
    if (_ButtonPress) {
      if (countdown > 2950) {
        sequence++;
        autoshow = false;
      }
      FastLED.clear();
      countdown = 3000;
    }
    
    if (_ButtonHold) countdown = 2940;
    if (countdown > 0) countdown--;
    }
    if (sequence > NUM_SEQUENCE) sequence = 0;
    
    if (millis() - time_sequence > AUTOSEQUENCE_DELAY && autoshow) {
      time_sequence = millis();
      sequence++;
      if (sequence > NUM_SEQUENCE-2) sequence = 0;
    }

     // How we change all the modes/sequences.
    switch (sequence) { //sequence
      case 0: Mode00();   break;
      case 1: Mode01();   break;
      case 2: Mode02();   break;
      case 3: Mode03();   break;
      case 4: Mode04();   break;
      case 5: Mode05();   break;
      case 6: Mode06();   break;
      case 7: Mode07();   break;
      case 8: Mode08();   break;
      case 9: Mode09();   break;
      case 10: Mode10();  break;
      case 11: Mode11();  break;
      case 12: Mode12();  break;
      case 13: Mode13();  break;
      case 14: Mode14();  break;
      case 15: Mode15();  break;
      case 16: Mode16();  break;
      case 17: Mode17();  break;
      case 18: Mode18();  break;
      case 19: Mode19();  break;
      case 20: Mode20();  break;
      case 21: Mode21();  break;
      case 22: Mode22();  break;
      case 23: Mode23();  break;
      default: Mode00();  break;
    }
    if (_ButtonHold && sequence < 23) { autoshow = true; flash(500,96,255,255);}
    if (countdown > 2950) DisplayMode(sequence);

    
    GetButtonInput2();
    GetButtonInput3();;
    ChangeBrightness();
   
    FastLED.show();
    
  
  }

// Its over!
