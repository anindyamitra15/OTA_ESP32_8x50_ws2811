#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncElegantOTA.h>
#include <Button2.h>
#include <FastLED.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Pins.h"
#include "config.h"

/*======Macros======*/
// number of modes
#define BOOL_TEXT(a) a?"True":"False"
#define CUR 2

/*======Enum======*/
/**
 * @brief this enum shows the order how effects are executed
 * Just put the unwanted effect on top and,
 * assign it to -1
 */
enum modes
{
  FILL_IVORY = -1,
  FILL_GOLD,
  RAINBOWS,
  HUE_SCROLL,
  ARROW_YELLOW,
  ARROW_COLOR,
  BIT_SIN_AKM,
  DUAL_SWORD,
  PHASE_SINE,
  PHASE_SINE_GOLD,
  FIRESHIP,
  BAR_GRAPH,
  TWINKLERS,
  MODES   //this gives the number of modes, keep it at the last
};

/*============Globals============*/
bool otaInProgress = false;
bool changeModes = true;
uint8_t m_index = 0;
AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager manager(&server,&dns);
Button2 button(SW, INPUT_PULLUP, false, true);
//Initialising strip arrays
CRGB pix1[NUM];
CRGB pix2[NUM];
CRGB pix3[NUM];
CRGB pix4[NUM];
CRGB pix5[NUM];
CRGB pix6[NUM];
CRGB pix7[NUM];
CRGB pix8[NUM];



//Put your palettes here--------------
DEFINE_GRADIENT_PALETTE(stars) {
    0,  10,  10,  10, //dark
   30,  10,  10, 255, //blue
  160, 255, 255, 255, //white
  255, 255, 247, 230  //warm
};

DEFINE_GRADIENT_PALETTE(fire) {
    0,  0,    0,  0, //black
  128, 255,   0,  0, //red
  200, 255, 230, 10, //yellow
  255, 255, 255, 128 //white 

};
//--------------------------------------

CRGBPalette16 starpal = stars;
CRGBPalette16 fires = fire;

/*============Prototypes============*/
void modeChange();
void modeChange(int m);
void click(Button2 &btn);
void longpress(Button2 &btn);
void mode(int i);
//modes - non-blocking master loop functions
void fill_all_strips(uint32_t colorcode);
void fade_all_to_black_by(uint8_t amount);
void common_color_to_linear_pixels(uint16_t pixel, uint32_t colorcode);
void common_color_to_linear_pixels(uint16_t pixel, CHSV colorcode);
void common_color_to_linear_pixels(uint16_t pixel, CRGB colorcode);
void prep();
void all_black();
void all_gold();
void hue_scroll();
void arrow_color();
void arrow_yellow();
void twinklers();
void bitSinAKM();
void bar_graph();
void fireship();
void rainbows();
void all_white();
void dual_sword();
void phase_sine();
void phase_sine_gold();


void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable   detector
  Serial.begin(115200);
  pinMode(SW, INPUT_PULLUP);
  FastLED.addLeds<WS2812, PIN1, ODR>(pix1, NUM);
  FastLED.addLeds<TYPE, PIN2, ODR>(pix2, NUM);
  FastLED.addLeds<TYPE, PIN3, ODR>(pix3, NUM);
  FastLED.addLeds<TYPE, PIN4, ODR>(pix4, NUM);
  FastLED.addLeds<TYPE, PIN5, ODR>(pix5, NUM);
  FastLED.addLeds<TYPE, PIN6, ODR>(pix6, NUM);
  FastLED.addLeds<TYPE, PIN7, ODR>(pix7, NUM);
  FastLED.addLeds<TYPE, PIN8, ODR>(pix8, NUM);


  FastLED.setCorrection(Tungsten40W);
  FastLED.setBrightness(BRIGHTNESS);

  //make the leds blank and enable brownout detect
  FastLED.clear();
  FastLED.show();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //enable brownout detector
  // Wifi setup
  manager.setTimeout(300);
  manager.autoConnect("Diwali Lights AKM");
  delay(500);
  Serial.print("Connected to ");
  Serial.println(WiFi.localIP());

  // OTA server init
  AsyncElegantOTA.begin(&server);
  AsyncElegantOTA.preFotaRoutineCallback(prep);
  
  //alive response route
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //response text preparation
    String cycleText = BOOL_TEXT(changeModes);
    String otaText = BOOL_TEXT(otaInProgress);
    request->send(200, "text/html", "<h1 style=\"text-align:center;\">Hi! I am ESP32 Diwali Lights.</h1><h3>Mode number: "
    + String(m_index) + "</h3>" + "<h3>Cycle State: " + cycleText + "</h3><h3>OTA state: " + otaText + "</h3>");
  });

  //next mode change route
  server.on("/next", HTTP_GET, [](AsyncWebServerRequest *request) {
    modeChange();
    request->send(200, "text/html", "<h1 style=\"text-align:center;\">Mode Changed to " + String(m_index) + "</h1>");
  });

  //callback to reset mode index to 0
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    modeChange(0);
    request->send(200, "text/html", "<h1 style=\"text-align:center;\">Mode Reset to " + String(m_index) + "</h1>");
  });

  // set mode using query parameter m = 0, 1, 2, etc.
  server.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebParameter* p = request->getParam(0);
    if (p->name() == "m"){
      String mode = p->value();
      int m = mode.toInt();

      // checking bound
      if(0 <= m && m < MODES){
        modeChange(m);
      }
      else{
        request->send(200, "text/html", "<h1 style=\"text-align:center;\">Mode remains "+ String(m_index)+", the same!</h1>");
        return;
      }
    }
    request->send(200, "text/html", "<h1 style=\"text-align:center;\">Mode changed to " + String(m_index) + "</h1>");
  });

  //toggle fixed/cycle around modes
  server.on("/cycle", HTTP_GET, [](AsyncWebServerRequest *request) {
    changeModes ^= true; //toggle mode
    String changeText = BOOL_TEXT(changeModes); //response text preparation
    request->send(200, "text/html", "<h1 style=\"text-align:center;\">Cycle mode changed to: " + changeText + "</h1>");
  });

  
  // Start server
  server.begin();
  // multifunction button
  button.begin(SW);
  button.setTapHandler(click);
  button.setLongClickHandler(longpress);
}



void loop() {

  if(!otaInProgress) {

    mode(m_index);

    FastLED.show();

    EVERY_N_SECONDS(200) {
      if(changeModes)
        modeChange();
    }
  }
  button.loop();
}



void mode(int i) {
  switch(i){
    case FILL_GOLD:
      all_gold();
      break;
    case FILL_IVORY:
      all_white();
      break;
    case RAINBOWS:
      rainbows();
      break;
    case HUE_SCROLL:
      hue_scroll();
      break;
    case ARROW_YELLOW:
      arrow_yellow();
      break;
    case ARROW_COLOR:
      arrow_color();
      break;
    case BIT_SIN_AKM:
      bitSinAKM();
      break;
    case DUAL_SWORD:
      dual_sword();
      break;
    case PHASE_SINE:
      phase_sine();
      break;
    case PHASE_SINE_GOLD:
      phase_sine_gold();
      break;
    case FIRESHIP:
      fireship();
      break;
    case BAR_GRAPH:
      bar_graph();
      break;
    case TWINKLERS:
      twinklers();
        break;
  }
}

void prep() {
  otaInProgress = true; //enable OTA/disable Lights'
  FastLED.clear();
  delay(10);
  FastLED.show();
}

//modeChange incrementer function
void modeChange (){
  m_index = ( m_index + 1 ) % MODES;
  FastLED.setBrightness(BRIGHTNESS);
}

void modeChange(int m){
  m_index = m % MODES;
  FastLED.setBrightness(BRIGHTNESS);
}

//click or tap responder callback
void click(Button2 &btn){
  modeChange();
  Serial.print("Mode changed to ");
  Serial.println(m_index);
}

//longpress responder callback
void longpress(Button2 &btn){
  unsigned int time = btn.wasPressedFor();
  Serial.print(time);
  Serial.println(" ms");
  if (time > 3000){
    Serial.println("Resetting WiFi...");
    manager.resetSettings();
    Serial.println("Rebooting");
    ESP.restart();
  }
}

void fill_all_strips(uint32_t colorcode) {
  fill_solid(pix1, NUM, colorcode);
  fill_solid(pix2, NUM, colorcode);
  fill_solid(pix3, NUM, colorcode);
  fill_solid(pix4, NUM, colorcode);
  fill_solid(pix5, NUM, colorcode);
  fill_solid(pix6, NUM, colorcode);
  fill_solid(pix7, NUM, colorcode);
  fill_solid(pix8, NUM, colorcode);
}

void fade_all_to_black_by(uint8_t amount) {
  fadeToBlackBy(pix1, NUM, amount);
  fadeToBlackBy(pix2, NUM, amount);
  fadeToBlackBy(pix3, NUM, amount);
  fadeToBlackBy(pix4, NUM, amount);
  fadeToBlackBy(pix5, NUM, amount);
  fadeToBlackBy(pix6, NUM, amount);
  fadeToBlackBy(pix7, NUM, amount);
  fadeToBlackBy(pix8, NUM, amount);
}


//three separate definitions for different types of color being handled by fastled
void common_color_to_linear_pixels(uint16_t pixel, uint32_t colorcode) {
  pix1[pixel] = colorcode;
  pix2[pixel] = colorcode;
  pix3[pixel] = colorcode;
  pix4[pixel] = colorcode;
  pix5[pixel] = colorcode;
  pix6[pixel] = colorcode;
  pix7[pixel] = colorcode;
  pix8[pixel] = colorcode;
}

void common_color_to_linear_pixels(uint16_t pixel, CHSV colorcode) {
  pix1[pixel] = colorcode;
  pix2[pixel] = colorcode;
  pix3[pixel] = colorcode;
  pix4[pixel] = colorcode;
  pix5[pixel] = colorcode;
  pix6[pixel] = colorcode;
  pix7[pixel] = colorcode;
  pix8[pixel] = colorcode;
}

void common_color_to_linear_pixels(uint16_t pixel, CRGB colorcode) {
  pix1[pixel] = colorcode;
  pix2[pixel] = colorcode;
  pix3[pixel] = colorcode;
  pix4[pixel] = colorcode;
  pix5[pixel] = colorcode;
  pix6[pixel] = colorcode;
  pix7[pixel] = colorcode;
  pix8[pixel] = colorcode;
}

/*=========Effects in master loop non-blocking mode=========*/

void hue_scroll(){
  uint8_t hue = beat8(40, 0);
  fill_solid(pix1, NUM, CHSV(hue      , 255, 255));
  fill_solid(pix2, NUM, CHSV(hue + 32 , 255, 255));
  fill_solid(pix3, NUM, CHSV(hue + 64 , 255, 255));
  fill_solid(pix4, NUM, CHSV(hue + 95 , 255, 255));
  fill_solid(pix5, NUM, CHSV(hue + 127, 255, 255));
  fill_solid(pix6, NUM, CHSV(hue + 159, 255, 255));
  fill_solid(pix7, NUM, CHSV(hue + 191, 255, 255));
  fill_solid(pix8, NUM, CHSV(hue + 223, 255, 255));
}

void all_gold() {
  EVERY_N_SECONDS(CUR) {  
    FastLED.setBrightness(BRIGHTNESS*0.8);
    fill_all_strips(CRGB::Gold);
  }
}

//just in case...
void all_black() {  
  fill_all_strips(CRGB::Black);
}


void all_white() {
  EVERY_N_SECONDS(CUR) {
    FastLED.setBrightness(BRIGHTNESS*0.8);
    fill_all_strips(CRGB::Ivory);
  }
}

void bitSinAKM(){
  uint8_t sinBeat1 = beatsin8(20, 0, NUM-1, 0, 0);  // runs thru the strip
  uint8_t sinBeat2 = beatsin8(5, 1, 5, 0, 5);  //sine ripple
  uint8_t sinBeat3 = beatsin8(sinBeat2, 0, 255, 0, 128); //hue ripple
  uint8_t sinBeat4 = beatsin8(15, 4, 10, 0, 0);  //chaser width ripple
  
  common_color_to_linear_pixels(sinBeat1, CHSV(sinBeat3, 255, 255));
  
  fade_all_to_black_by(sinBeat4);
}

void twinklers(){
  EVERY_N_MILLISECONDS(20) {
    pix1[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix2[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix3[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix4[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix5[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix6[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix7[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
    pix8[random(0, NUM-1)] = ColorFromPalette(starpal, random8(), 255, LINEARBLEND);
  }
  fade_all_to_black_by(8);
}

void arrow_yellow(){
  uint8_t beat1 = beat8(60, 0);
  uint8_t beat2 = beat1 + 20;
  uint8_t beat3 = beat2 + 20;
  uint8_t beat4 = beat3 + 20;
  uint8_t sinBeat3 = beatsin8(40, 28, 36, 0, 0);
  pix1[map8(beat1, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix2[map8(beat2, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix3[map8(beat3, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix4[map8(beat4, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix5[map8(beat4, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix6[map8(beat3, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix7[map8(beat2, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix8[map8(beat1, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);

  fade_all_to_black_by(6);
}

void arrow_color(){
  uint8_t beat1 = beat8(60, 0);
  uint8_t beat2 = beat1 + 20;
  uint8_t beat3 = beat2 + 20;
  uint8_t beat4 = beat3 + 20;
  uint8_t sinBeat3 = beatsin8(10, 0, 255, 0, 0);

  pix1[map8(beat1, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix2[map8(beat2, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix3[map8(beat3, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix4[map8(beat4, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix5[map8(beat4, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix6[map8(beat3, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix7[map8(beat2, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);
  pix8[map8(beat1, 0, NUM-1)] = CHSV(sinBeat3, 255, 255);

  fade_all_to_black_by(7);
}

void bar_graph() {
  uint8_t beat = beatsin8(50, 0, NUM - 1, 0, 0);

  common_color_to_linear_pixels(beat, CRGB::WhiteSmoke);

  fade_all_to_black_by(10);
}

void fireship() {
  uint8_t beat = beat8(20, 0);
  fill_palette(pix1, NUM, beat     , 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix2, NUM, beat + 20, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix3, NUM, beat + 40, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix4, NUM, beat + 60, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix5, NUM, beat + 60, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix6, NUM, beat + 40, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix7, NUM, beat + 20, 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
  fill_palette(pix8, NUM, beat     , 256/NUM, fires, BRIGHTNESS, LINEARBLEND);
}

void rainbows() {
  uint8_t huehue = beat8(40, 0);
  fill_rainbow(pix1, NUM, huehue, 255/NUM);
  fill_rainbow(pix2, NUM, huehue, 255/NUM);
  fill_rainbow(pix3, NUM, huehue, 255/NUM);
  fill_rainbow(pix4, NUM, huehue, 255/NUM);
  fill_rainbow(pix5, NUM, huehue, 255/NUM);
  fill_rainbow(pix6, NUM, huehue, 255/NUM);
  fill_rainbow(pix7, NUM, huehue, 255/NUM);
  fill_rainbow(pix8, NUM, huehue, 255/NUM);
}

void dual_sword() {
  uint8_t sin1 = beatsin8(40, 0, NUM-1, 0, 0);
  uint8_t sin2 = beatsin8(40, 0, NUM-1, 0, 127);  //180 deg
  uint8_t sin_hsv1 = beatsin8(10, 0, 255, 0, 0);
  uint8_t sin_hsv2 = beatsin8(10, 0, 255, 0, 127);  //180 deg

  common_color_to_linear_pixels(sin1, CHSV(sin_hsv1, 255, 255));

  common_color_to_linear_pixels(sin2, CHSV(sin_hsv2, 255, 255));

  fade_all_to_black_by(15);
}

void phase_sine() {
  uint8_t sin_1 = beatsin8(25, 0, NUM-1, 0, 0 );
  uint8_t sin_2 = beatsin8(25, 0, NUM-1, 0, 32);
  uint8_t sin_3 = beatsin8(25, 0, NUM-1, 0, 64);
  uint8_t sin_4 = beatsin8(25, 0, NUM-1, 0, 96);
  uint8_t sin_5 = beatsin8(25, 0, NUM-1, 0, 128);
  uint8_t sin_6 = beatsin8(25, 0, NUM-1, 0, 160);
  uint8_t sin_7 = beatsin8(25, 0, NUM-1, 0, 192);
  uint8_t sin_8 = beatsin8(25, 0, NUM-1, 0, 224);

  pix1[sin_1] = CRGB::Ivory;
  pix2[sin_2] = CRGB::Ivory;
  pix3[sin_3] = CRGB::Ivory;
  pix4[sin_4] = CRGB::Ivory;
  pix5[sin_5] = CRGB::Ivory;
  pix6[sin_6] = CRGB::Ivory;
  pix7[sin_7] = CRGB::Ivory;
  pix8[sin_8] = CRGB::Ivory;

  fade_all_to_black_by(30);
}

void phase_sine_gold() {
  uint8_t sin_1 = beatsin8(25, 0, NUM-1, 0, 0 );
  uint8_t sin_2 = beatsin8(25, 0, NUM-1, 0, 32);
  uint8_t sin_3 = beatsin8(25, 0, NUM-1, 0, 64);
  uint8_t sin_4 = beatsin8(25, 0, NUM-1, 0, 96);
  uint8_t sin_5 = beatsin8(25, 0, NUM-1, 0, 128);
  uint8_t sin_6 = beatsin8(25, 0, NUM-1, 0, 160);
  uint8_t sin_7 = beatsin8(25, 0, NUM-1, 0, 192);
  uint8_t sin_8 = beatsin8(25, 0, NUM-1, 0, 224);

  pix1[sin_1] = CRGB::Gold;
  pix2[sin_2] = CRGB::Gold;
  pix3[sin_3] = CRGB::Gold;
  pix4[sin_4] = CRGB::Gold;
  pix5[sin_5] = CRGB::Gold;
  pix6[sin_6] = CRGB::Gold;
  pix7[sin_7] = CRGB::Gold;
  pix8[sin_8] = CRGB::Gold;

  fade_all_to_black_by(30);
}
