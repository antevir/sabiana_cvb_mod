#pragma once

#include <Arduino.h>

typedef enum {
   LED_STATE_OFF,
   LED_STATE_GREEN,
   LED_STATE_GREEN_RED,
   LED_STATE_RED
} led_state_t;

class Leds {
   private:
      uint8_t greenGpio;
      uint8_t redGpio;
      unsigned long lastBlinkTime = 0;
      led_state_t state;
      
      void update(void)
      {
         if ((lastBlinkTime == 0) || (millis() - lastBlinkTime >= 100)) {
            lastBlinkTime = 0;
            switch (state)
            {
               case LED_STATE_OFF:
                  digitalWrite(redGpio, LOW);
                  digitalWrite(greenGpio, LOW);
                  break;
               case LED_STATE_GREEN:
                  digitalWrite(redGpio, LOW);
                  digitalWrite(greenGpio, HIGH);
                  break;
               case LED_STATE_GREEN_RED:
                  digitalWrite(redGpio, HIGH);
                  digitalWrite(greenGpio, HIGH);
                  break;
               case LED_STATE_RED:
                  digitalWrite(redGpio, HIGH);
                  digitalWrite(greenGpio, LOW);
                  break;
            }
         }
      }
      
   public:
      Leds(uint8_t greenGpio, uint8_t redGpio) :
      greenGpio(greenGpio),
      redGpio(redGpio)
      {}
      
      void init(void)
      {
         pinMode(redGpio, OUTPUT);
         pinMode(greenGpio, OUTPUT); 
      }
      
      void set(led_state_t state)
      {
         this->state = state;
         update();
      }
      
      void blink(void)
      {
         int redValue = digitalRead(redGpio);
         int greenValue = digitalRead(greenGpio);
         digitalWrite(redGpio, !redValue);
         digitalWrite(greenGpio, !greenValue);
         lastBlinkTime = millis();
      }
      
      void handle(void)
      {
         update();
      }
};