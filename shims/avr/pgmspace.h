// Desktop replacement for <avr/pgmspace.h>: program memory is plain memory.
#ifndef DRUMFAKE_SHIMS_AVR_PGMSPACE_H_
#define DRUMFAKE_SHIMS_AVR_PGMSPACE_H_

#include <stdint.h>

#define PROGMEM

typedef char prog_char;
typedef uint8_t prog_uint8_t;
typedef uint16_t prog_uint16_t;
typedef uint32_t prog_uint32_t;

#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#define pgm_read_word(p) (*reinterpret_cast<const uint16_t*>(p))
#define pgm_read_dword(p) (*reinterpret_cast<const uint32_t*>(p))

#endif  // DRUMFAKE_SHIMS_AVR_PGMSPACE_H_
