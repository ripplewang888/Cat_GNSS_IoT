 /**
 * This file is an application tool of nmealib.
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

#include "nmealib.h"
#include "nmeautil.h"
#include <stdio.h>
#include <string.h>



char* str_GGA_time( data_store_t * gpsData){
  static char strGGAtime[16];
  snprintf(strGGAtime, sizeof(strGGAtime), 
    "%02d:%02d:%02d.%02d", 
    gpsData->time.hour,
    gpsData->time.min,
    gpsData->time.sec,
    gpsData->time.secp);
  return strGGAtime;
}

char* str_GGA_latitude( data_store_t * gpsData){
  static char strGGAlatitude[16];
  if(gpsData->lat.minp == 2){
    snprintf(strGGAlatitude, sizeof(strGGAlatitude), 
      "%d:%d.%02d %s", 
      gpsData->lat.deg,
      gpsData->lat.min,
      gpsData->lat.minp1,
      (gpsData->flag.ns)?"N":"S");      
  }else if(gpsData->lat.minp == 4){
    snprintf(strGGAlatitude, sizeof(strGGAlatitude), 
      "%d:%d.%02d%02d %s", 
      gpsData->lat.deg,
      gpsData->lat.min,
      gpsData->lat.minp1,
      gpsData->lat.minp2,
      (gpsData->flag.ns)?"N":"S"); 
  }else{
    snprintf(strGGAlatitude, sizeof(strGGAlatitude), 
      "%d:%d.%02d%02d%01d %s", 
      gpsData->lat.deg,
      gpsData->lat.min,
      gpsData->lat.minp1,
      gpsData->lat.minp2,
      gpsData->lat.minp3,
      (gpsData->flag.ns)?"N":"S");       
  }
  return strGGAlatitude;
}

char* str_GGA_longitude( data_store_t * gpsData){
  static char strGGAlongitude[16];
  if(gpsData->lon.minp == 2){
    snprintf(strGGAlongitude, sizeof(strGGAlongitude), 
      "%d:%d.%02d%02d %s", 
      gpsData->lon.deg,
      gpsData->lon.min,
      gpsData->lon.minp1,
      gpsData->lon.minp2,
      (gpsData->flag.ew)?"E":"W");  
  }else if(gpsData->lon.minp == 4){
    snprintf(strGGAlongitude, sizeof(strGGAlongitude), 
      "%d:%d.%02d%02d %s", 
      gpsData->lon.deg,
      gpsData->lon.min,
      gpsData->lon.minp1,
      gpsData->lon.minp2,
      (gpsData->flag.ew)?"E":"W");
  }else{
    snprintf(strGGAlongitude, sizeof(strGGAlongitude), 
      "%d:%d.%02d%02d%01d %s", 
      gpsData->lon.deg,
      gpsData->lon.min,
      gpsData->lon.minp1,
      gpsData->lon.minp2,
      gpsData->lon.minp3,
      (gpsData->flag.ew)?"E":"W");      
  }
  return strGGAlongitude;
}

char* str_GGA_AMSL( data_store_t * gpsData){
  static char strGGA_AMSL[16];
  snprintf(strGGA_AMSL, sizeof(strGGA_AMSL), 
    "%d.%01d", 
    gpsData->amsl.integ,
    gpsData->amsl.dec);
  return strGGA_AMSL;
}

char* str_GGA_HDOP( data_store_t * gpsData){
  static char strGGA_HDOP[16];
  snprintf(strGGA_HDOP, sizeof(strGGA_HDOP), 
    "%d.%02d", 
    gpsData->hdop.integ,
    gpsData->hdop.dec);
  return strGGA_HDOP;
}
