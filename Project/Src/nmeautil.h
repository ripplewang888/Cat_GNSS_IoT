/*
 * This file is part of nmealib.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef __NMEA_UTIL_H__
#define __NMEA_UTIL_H__
#include "nmealib.h"

extern char* str_GGA_time(data_store_t *);
extern char* str_GGA_latitude( data_store_t *);
extern char* str_GGA_longitude( data_store_t *);
extern char* str_GGA_AMSL( data_store_t *);
extern char* str_GGA_HDOP( data_store_t *);

#endif /*__NMEA_UTIL_H__*/
