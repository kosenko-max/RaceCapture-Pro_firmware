/*
 * Race Capture Firmware
 *
 * Copyright (C) 2016 Autosport Labs
 *
 * This file is part of the Race Capture firmware suite
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should
 * have received a copy of the GNU General Public License along with
 * this code. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include "cpp_guard.h"

#include <stddef.h>
#include <stdint.h>

CPP_GUARD_BEGIN

typedef enum {
    SERIAL_USB = 0,
    SERIAL_GPS,
    SERIAL_TELEMETRY,
    SERIAL_WIRELESS,
    SERIAL_AUX
} serial_id_t;

#define SERIAL_COUNT 5

typedef struct _Serial {

    void (*init)(unsigned int bits, unsigned int parity, unsigned int stopBits, unsigned int baud);
    int (*get_c_wait)(char *c, size_t delay);
    char (*get_c)(void);

    int (*get_line)(char *s, int len);
    int (*get_line_wait)(char *s, int len, size_t delay);

    void (*put_c)(char c);
    void (*put_s)(const char *);

    void (*flush)(void);

} Serial;


void init_serial(void);

Serial * get_serial(serial_id_t port);

void configure_serial(serial_id_t port, uint8_t bits, uint8_t parity, uint8_t stopBits, uint32_t baud);

size_t serial_read_byte(Serial *serial, uint8_t *b, size_t delay);

void put_int(Serial * serial, int n);

void put_ll(Serial *serial, long long l);

void put_hex(Serial * serial, int n);

void put_float(Serial * serial, float f, int precision);

void put_double(Serial * serial, double f, int precision);

void put_uint(Serial * serial, unsigned int n);

void put_escapedString(Serial * serial, const char *v, int length);

void put_nameUint(Serial * serial, const char *s, unsigned int n);

void put_nameSuffixUint(Serial * serial, const char *s, const char *suf, unsigned int n);

void put_nameIndexUint(Serial * serial, const char *s, int i, unsigned int n);

void put_nameInt(Serial * serial, const char *s, int n);

void put_nameSuffixInt(Serial * serial, const char *s, const char *suf, int n);

void put_nameIndexInt(Serial * serial, const char *s, int i, int n);

void put_nameDouble(Serial * serial, const char *s, double n, int precision);

void put_nameSuffixDouble(Serial * serial, const char *s, const char *suf, double n, int precision);

void put_nameIndexDouble(Serial * serial, const char *s, int i, double n, int precision);

void put_nameFloat(Serial * serial, const char *s, float n, int precision);

void put_nameSuffixFloat(Serial * serial, const char *s, const char *suf, float n, int precision);

void put_nameIndexFloat(Serial * serial, const char *s, int i, float n, int precision);

void put_nameString(Serial * serial, const char *s, const char *v);

void put_nameSuffixString(Serial * serial, const char *s, const char *suf, const char *v);

void put_nameIndexString(Serial * serial, const char *s, int i, const char *v);

void put_nameEscapedString(Serial * serial, const char *s, const char *v, int length);

void put_bytes(Serial *serial, char *data, unsigned int length);

void put_crlf(const Serial * serial);

void read_line(Serial *serial, char *buffer, size_t bufferSize);

void interactive_read_line(Serial *serial, char * buffer, size_t bufferSize);

CPP_GUARD_END

#endif /* SERIAL_H_ */
