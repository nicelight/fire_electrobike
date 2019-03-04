#include <FastLED.h>
#include <EEPROM.h>

// количество светодиодов в одной ленте
#define NUM_LEDS    26

// пин к которому через резистор 100 - 1000 Ом подключена левая лента
#define LEFT_STRIP     11
// пин к которому через резистор 100 - 1000 Ом подключена правая лента
#define RIGHT_STRIP     9
// пин, к которому подключен концевик на тормоз
#define BREAK_BUTTON 4
//пин к которому подключен концевик на поволор влево
#define LEFT_BUTTON 3
//пин к которому подключен концевик на поволор вправо
#define RIGHT_BUTTON 2

// как долго желтый светится на поворотнике, прежде чем потухнуть
#define TURNS_DELAY 700

// пин на который подключен динамик пищалка
#define SOUND 6

#define POLICE_BUTTON A7

// ячейка памяти EEPROM для режима работы
#define MODE_ADRESS 10
#define CHIPSET     WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS  200
#define FRAMES_PER_SECOND 60

//#define DEBUG  // отладочка

bool gReverseDirection = false;

CRGB leds_left[NUM_LEDS];
CRGB leds_right[NUM_LEDS];



// Fire2012 with programmable Color Palette
//
// This code is the same fire simulation as the original "Fire2012",
// but each heat cell's temperature is translated to color through a FastLED
// programmable color palette, instead of through the "HeatColor(...)" function.
//
// Four different static color palettes are provided here, plus one dynamic one.
//
// The three static ones are:
//   1. the FastLED built-in HeatColors_p -- this is the default, and it looks
//      pretty much exactly like the original Fire2012.
//
//  To use any of the other palettes below, just "uncomment" the corresponding code.
//
//   2. a gradient from black to red to yellow to white, which is
//      visually similar to the HeatColors_p, and helps to illustrate
//      what the 'heat colors' palette is actually doing,
//   3. a similar gradient, but in blue colors rather than red ones,
//      i.e. from black to blue to aqua to white, which results in
//      an "icy blue" fire effect,
//   4. a simplified three-step gradient, from black to red to white, just to show
//      that these gradients need not have four components; two or
//      three are possible, too, even if they don't look quite as nice for fire.
//
// The dynamic palette shows how you can change the basic 'hue' of the
// color palette every time through the loop, producing "rainbow fire".

CRGBPalette16 gPal;

byte mode = 0; // цветовая гамма динамических огней ленты
bool dark_flag = 0; // переменная mode  сбрасывается  при повороте на право почему то, будем ее клонировать)))) КОСТЫЛИЩЕ!!!!
byte state = 1; // состояние работы ленты. 1 - огни. 2 - поворотники
byte left_auto = 0; // автомат отрисовки поворота влево
byte right_auto = 0; // автомат отрисовки поворота вправо
unsigned long ms = 0, rightMs = 0, leftMs = 0;

void setup() {
  pinMode(BREAK_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(POLICE_BUTTON, INPUT_PULLUP);
  Serial.begin(115200);
  delay(300); // sanity delay
  FastLED.addLeds<CHIPSET, LEFT_STRIP, COLOR_ORDER>(leds_left, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, RIGHT_STRIP, COLOR_ORDER>(leds_right, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
#ifdef DEBUG
  Serial.println(F("DEBUG MODE")) ;
#endif


  // режим свечения из памяти тянем
  mode = EEPROM.read(MODE_ADRESS);
  // если значения в памяти кривые , поправляем
  if (mode > 7) {
    mode = 0;
    EEPROM.update(MODE_ADRESS, mode);
  }
#ifdef DEBUG
  Serial.print(F("mode from EEPROM = "));
  Serial.println(mode);
#endif
  // установим палитру из памяти
  switch (mode) {
    case 0:
      //red-white eeprom mode == 0
      gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);
      break;
    case 1:
      // orange eeprom mode == 1
      gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Orange,  CRGB::Yellow);
      break;
    case 2:
      //green-white eeprom mode == 2
      gPal = CRGBPalette16( CRGB::Black, CRGB::Green, CRGB::Aqua,  CRGB::White);
      break;
    case 3:
      // aqua-white eeprom mode == 3
      gPal = CRGBPalette16( CRGB::Black, CRGB::Aqua, CRGB::White,  CRGB::White);
      break;
    case 4:
      //blue-white eeprom mode == 4
      gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);
      break;
    case 5:
      //white-purple eeprom mode == 5
      gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Red,  CRGB::White);
      break;
    case 6:
      //purple-blue eeprom mode == 6
      gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Blue,  CRGB::Purple);
      break;
    case 7:
      //black eeprom mode == 7
      gPal = CRGBPalette16( CRGB::Black, CRGB::Black, CRGB::Black,  CRGB::Black);
      all_black();
      break;
  }// mode

  // установка цвета сияния ленты, если зажат какой то из поворотников
  if ((!digitalRead(LEFT_BUTTON)) || (!digitalRead(RIGHT_BUTTON))) {
    delay(20);
    if ((!digitalRead(LEFT_BUTTON)) || (!digitalRead(RIGHT_BUTTON))) {
      delay(50);
#ifdef DEBUG
      Serial.println(F("change palete mode"));
#endif
      //подсчитываем трети чтобы красиво индицировать какой сейчас режим
      byte firstThird = NUM_LEDS / 3;
      byte secondThird = firstThird * 2;
      while ((!digitalRead(LEFT_BUTTON)) || (!digitalRead(RIGHT_BUTTON))) {
        // нажатием на тормоз, переключаем режимы (цветовые палитры) свечения
        if (!digitalRead(BREAK_BUTTON)) {
          delay(20);
          if (!digitalRead(BREAK_BUTTON)) {
            while (!digitalRead(BREAK_BUTTON));
            delay(50);
            if (mode < 7) mode ++;
            else mode = 0;
#ifdef DEBUG
            Serial.print(F("mode set to "));
            Serial.println(mode);
#endif
            // индицируем в каком режиме сейчас и устанавливаем палитру
            switch (mode) {
              case 0:
                //red-white
                gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Yellow;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Red;
                }
                FastLED.show(); // display this frame
                break;

              case 1:
                //red-yellow
                gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Orange,  CRGB::Yellow);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Yellow;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Orange;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Red;
                }
                FastLED.show(); // display this frame
                break;
              case 2:
                //green- white
                gPal = CRGBPalette16( CRGB::Black, CRGB::Green, CRGB::Aqua,  CRGB::White);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Aqua;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Green;
                }
                FastLED.show(); // display this frame
                break;
              case 3:
                // aqua-white eeprom mode == 3
                gPal = CRGBPalette16( CRGB::Black, CRGB::Aqua, CRGB::White,  CRGB::White);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Aqua;
                }
                FastLED.show(); // display this frame
                break;
              case 4:
                //blue-white eeprom mode == 4
                gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Aqua;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Blue;
                }
                FastLED.show(); // display this frame
                break;
              case 5:
                //white-purple eeprom mode == 5
                gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Red,  CRGB::White);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::White;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Red;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Red;
                }
                FastLED.show(); // display this frame
                break;
              case 6:
                //purple-blue eeprom mode == 6
                gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Blue,  CRGB::Purple);
                for ( int i = 0; i < firstThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Purple;
                }
                for ( int i = firstThird; i < secondThird; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Blue;
                }
                for ( int i = secondThird; i < NUM_LEDS; i++) {
                  leds_left[i] = leds_right[i] = CRGB::Blue;
                }
                FastLED.show(); // display this frame
                break;
              case 7:
                //BLACK eeprom mode == 7
                gPal = CRGBPalette16( CRGB::Black, CRGB::Black, CRGB::Black,  CRGB::Black);
                all_black();
                break;
            }// switch mode

          }// while changing mode
          EEPROM.update(MODE_ADRESS, mode);

        }// if break
      }// if break
    }//if changing mode
  }//if changing mode


#ifdef DEBUG
  Serial.println(F("start program"));
#endif
  if (mode == 7) dark_flag = 1;
}//setup

void loop()
{
  ms = millis();
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());

  // Fourth, the most sophisticated: this one sets up a new palette every
  // time through the loop, based on a hue that changes every time.
  // The palette is a gradient from black, to a dark color based on the hue,
  // to a light color based on the hue, to white.
  //
  //   static uint8_t hue = 0;
  //   hue++;
  //   CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
  //   CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
  //   gPal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);

  // to set color dirrectly
  // leds[i] = CRGB( 50, 100, 150);
  Serial.print("dark_flag ");
  Serial.println(dark_flag);
  Serial.println("  mode ");
  Serial.println(mode);
  if (!digitalRead(BREAK_BUTTON)) {
    delay(30);
    if (!digitalRead(BREAK_BUTTON)) {
      // если включен какой либо режим динамических огней, делаем задержку, чтобы было видно что это не эффект а тормоз
      if (dark_flag) {
        for ( int i = 0; i < NUM_LEDS; i++) {
          leds_left[i] = leds_right[i] = CRGB::Red;
        }//for all leds
        FastLED.show(); // display this frame
      }
      else {
        all_black();
        delay(250);
      }
      while (!digitalRead(BREAK_BUTTON)) {
        for ( int i = 0; i < NUM_LEDS; i++) {
          leds_left[i] = leds_right[i] = CRGB::Red;
        }//for all leds
        FastLED.show(); // display this frame
      }//while BREAK_BUTTON
      // если включен какой либо режим динамических огней, делаем задержку, чтобы было видно что это не эффект а тормоз
      all_black();
      if (dark_flag != 7) {
        delay(250);
      }
    }//if BREAK_BUTTON
  }//if BREAK_BUTTON


  // если нажаты поворотники, происходит смена режима работы с пылающего огня на повороты
  // эти режимы работают только если не нажат тормоз
  if ((!digitalRead(LEFT_BUTTON)) || (!digitalRead(RIGHT_BUTTON))) {
    delay(50);
    if (!digitalRead(LEFT_BUTTON)) {
      if (state != 2) {
        state = 2; // отправляем автомат состояния лент в режим поворотников
        left_auto = 0; // сбрасываем отрисовку поворота на исходную
      }
    } //if left turn
    else if (!digitalRead(RIGHT_BUTTON)) {
      if (state != 3) {
        state = 3; // отправляем автомат состояния лент в режим поворотников
        right_auto = 0; // сбрасываем отрисовку поворота на исходную
      }
    }//if right turn
  }//if left turn
  else {
    all_black();
    state = 1; // иначе огни
  }

  // выбор состояния
  // 1 - огонь
  // 2 -  поворотники влево
  // 3 - поворотники вправо
  switch (state) {
    case 1:
      // если  не темный режим, рисуем огни
      if (mode != 7) {
        Fire2012WithPalette_left(); // run simulation frame, using palette colors
        Fire2012WithPalette_right(); // run simulation frame, using palette colors
        FastLED.show(); // display this frame
        FastLED.delay(1000 / FRAMES_PER_SECOND);
      }// if not 7 dark_flag
      break;
    // state = 2 индикация поворота влево
    case 2:
      switch (left_auto) {
        case 0:
          all_black();
          leftMs = ms + 200;
          left_auto = 5;// GO
          break;
        // тушим все
        case 3:
          if ((ms - leftMs) > TURNS_DELAY) {
#ifdef DEBUG
            Serial.println(F("turns left")) ;
#endif
            leftMs = ms;
            all_black();
            left_auto = 5;// GO
          }
          break;
        // отрисовка бегущей желтой точки
        case 5:
          if ((ms - leftMs) > 500) { // спустя пол секунды
            // зажигаем все
            for ( int i = NUM_LEDS; i >= 0; i--) {
              leds_left[i] = CRGB::Yellow;
              leds_right[0] = CRGB::Black; // костыль .. непонятно, почему, зажигается первый  светик
              FastLED.show(); // display this frame
              delay(20);
            }
            leftMs = ms;
            left_auto = 3;
          }//if ms
          break;
      }//switch(left_auto)
      break;
    // индикация поворота вправо
    case 3:
      switch (right_auto) {
        case 0:
          all_black();
          rightMs = ms + 300;
          right_auto = 5;
          break;
        // тушим все
        case 3:
          if ((ms - rightMs) > TURNS_DELAY) {
#ifdef DEBUG
            Serial.println(F("turns right")) ;
#endif
            rightMs = ms;
            all_black();
            right_auto = 5;// GO
          }
          break;
        // отрисовка бегущей желтой точки
        case 5:
        right_auto = 7;
        break;
        case 7:
          if ((ms - rightMs) > 500ul) {
            for ( byte i = NUM_LEDS-1; i >= 1; i--) {
              leds_right[i] = CRGB::Yellow;
              FastLED.show();
              delay(20);
            }
            rightMs = ms;
            right_auto = 3;
          }//if ms
          break;
      }//switch(right_auto)
      break;
  }//switch(state)

}//loop


void all_black() {
  // все тушим, чтобы не сбивать с толку
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds_left[i] = leds_right[i] = CRGB::Black;
  }
  FastLED.show(); // display this frame
}//all_black()
/*
  // Fire2012 by Mark Kriegsman, July 2012
  // as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
  ////
  // This basic one-dimensional 'fire' simulation works roughly as follows:
  // There's a underlying array of 'heat' cells, that model the temperature
  // at each point along the line.  Every cycle through the simulation,
  // four steps are performed:
  //  1) All cells cool down a little bit, losing heat to the air
  //  2) The heat from each cell drifts 'up' and diffuses a little
  //  3) Sometimes randomly new 'sparks' of heat are added at the bottom
  //  4) The heat from each cell is rendered as a color into the leds array
  //     The heat-to-color mapping uses a black-body radiation approximation.
  //
  // Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
  //
  // This simulation scales it self a bit depending on NUM_LEDS; it should look
  // "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
  //
  // I recommend running this simulation at anywhere from 30-100 frames per second,
  // meaning an interframe delay of about 10-35 milliseconds.
  //
  // Looks best on a high-density LED setup (60+ pixels/meter).
  //
  //
  // There are two main parameters you can play with to control the look and
  // feel of your fire: COOLING (used in step 1 above), and SPARKING (used
  // in step 3 above).
  //
*/
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120


void Fire2012WithPalette_left()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);
    CRGB color = ColorFromPalette( gPal, colorindex);
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds_left[pixelnumber] = color;
  }
}

void Fire2012WithPalette_right()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);
    CRGB color = ColorFromPalette( gPal, colorindex);
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds_right[pixelnumber] = color;
  }
}//right_fire
