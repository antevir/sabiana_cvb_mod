#pragma once

#include <Arduino.h>
#include "LittleFS.h"
#include "settings_table.h"

#define SETTINGS_FILE   "/settings.txt"

typedef void (*setting_modified_t)(void);

class Settings {
   protected:
      setting_modified_t modifiedCb;

      #define DEFINE_VAR(TYPE, DEF_VAL, NAME, GETSET_NAME) TYPE NAME = DEF_VAL;
      SETTINGS_TABLE(DEFINE_VAR)

      bool readValue(File f, String &value) {
         char buffer[64];
         int l = f.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
         if (l <= 0) {
            return false;
         }
         buffer[l] = 0;
         value = buffer;
         return true; 
      }
      
      bool readValue(File f, int &value) {
         String strValue;
         if (!readValue(f, strValue)) {
            return false;
         }
         value = strValue.toInt();
         return true; 
      }
      
      void writeValue(File f, String &line) {
         f.write((uint8_t *)line.c_str(), line.length());
         f.write('\n');
      }
      
      void writeValue(File f, int &value) {
         String strValue(value);
         writeValue(f, strValue);
      }

      void loadAllValues(File f) {
          #define DEFINE_READ(TYPE, DEF_VAL, NAME, GETSET_NAME) readValue(f, NAME);
          SETTINGS_TABLE(DEFINE_READ)
      }
      void storeAllValues(File f) {
          #define DEFINE_WRITE(TYPE, DEF_VAL, NAME, GETSET_NAME) writeValue(f, NAME);
          SETTINGS_TABLE(DEFINE_WRITE)
      }

      void notifyModified() {
         if (modifiedCb != NULL) {
            modifiedCb();
         }
      }
  
  public:
      static Settings& getInstance()
      {
          static Settings instance;
          return instance;
      }

      #define DEFINE_SET_GET(TYPE, DEF_VAL, NAME, GETSET_NAME) \
          TYPE get##GETSET_NAME() { return this->NAME; } \
          void set##GETSET_NAME(TYPE NAME) { \
              if (this->NAME == NAME) { \
                 return; \
              } \
              this->NAME = NAME; \
              notifyModified(); \
          }
      SETTINGS_TABLE(DEFINE_SET_GET)
      
      void init(setting_modified_t modifiedCb = NULL) {
         this->modifiedCb = modifiedCb;
         if (!LittleFS.begin(true)) {
            Serial.println( "Failed to start LittleFS" );
            LittleFS.format();
            LittleFS.begin();
         }

         Serial.println("LittleFS mounted successfully!");
         File f = LittleFS.open(SETTINGS_FILE, "r", true);
         String line = "";
         loadAllValues(f);
         printf("setTemp: %d\n", setTemp);
         f.close();
      }
      
      void store(void) {
         printf("Storing settings\n");
         File f = LittleFS.open(SETTINGS_FILE, "w", true);
         storeAllValues(f);
         f.close();
      }
};
