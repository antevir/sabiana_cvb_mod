#pragma once

typedef enum {
   FAN_LEVEL_UNKNOWN = 255,
   FAN_LEVEL_OFF = 0,
   FAN_LEVEL_AUTO,
   FAN_LEVEL_1,
   FAN_LEVEL_2,
   FAN_LEVEL_3
} fan_level_t;

typedef enum {
   OP_MODE_OFF = 0,
   OP_MODE_FAN,
   OP_MODE_HEAT,
   OP_MODE_COOL,
   OP_MODE_AUTO
} op_mode_t;
