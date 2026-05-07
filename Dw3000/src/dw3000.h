#pragma once

#ifndef MAIN_H_
#define MAIN_H_

//#include <avr/io.h> // all the standard AVR functions
#define __DELAY_BACKWARD_COMPATIBLE__ // this enables uint32 to be used in sleep functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define debug_assert(condition, message) if (condition) { printf(message); } else {}
#else
#define debug_assert(condition, message) (void)0
#endif

#ifndef USE_ARDUINO
#define USE_ARDUINO 0
#endif

#if USE_ARDUINO
#include <Arduino.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include "dw3000_uart.h"
#include "dw3000_port.h"
#include "dw3000_device_api.h"
#include "dw3000_shared_functions.h"

#define _BV(n) (1 << n) // sets 1 at position of BIT "n"
#define __INLINE inline

#endif /* MAIN_H_ */
