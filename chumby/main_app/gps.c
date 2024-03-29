//GPS NMEA Driver + Comparitive positioning computation
////    Written by Matthew Rasmussen, 2010
////    Released under GPL license.
////        !!When compiling for Chumby, use Lib Math ("-lm" flag) or      !!
////        !!it won't work!                        !!

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "gps.h"

#define PI 3.14159265358979
//Radius in meters:

#define R       6378000
#define DtoR    (PI / 180)
#define RtoD    (180 / PI)

const char * gps_err_msgs[NUM_GPS_ERRORS] = {
    "NO_ERROR",
    "GPS_OPEN_FAIL",
    "GPS_DATA_TIMEOUT",
    "GPS_READ_TIMEOUT",
    "GPS_DATA_INVALID"
};

static int tty;

static double convert_nmea_ll(char * lat);

int gps_init()
{
  struct termios gps_term;

  if(tty > 0)
    close(tty);

  system("stty -F /dev/gps 4800");
  tty = open("/dev/gps", O_RDWR | O_NOCTTY);
  if(tty < 0)
    return GPS_OPEN_FAIL;
    
  tcgetattr(tty, &gps_term);
  cfmakeraw(&gps_term);
  cfsetspeed(&gps_term, B4800);
  gps_term.c_cflag = CS8 | CLOCAL | CREAD;
  gps_term.c_iflag = IGNPAR | IGNBRK;
  gps_term.c_lflag = ICANON;
  gps_term.c_cc[VTIME] = 10;
  gps_term.c_cc[VMIN] = 0; 
  tcsetattr(tty,TCSAFLUSH,&gps_term);

  return GPS_NO_ERROR;
}

double convert_nmea_ll(char * lat)
{
    double degrees = 0;
    double minutes = 0;

    int count;
    int dot = 0;
    //double a = 1;
    char direction = 0;

    while (lat[dot] != '.')
    {
    dot++;
    }

    for ( count = 0; count < dot - 2; count++ )
    {
    degrees *= 10;
    degrees += lat[count] - '0';
    }

    sscanf(&(lat[count]),"%lf,%c",&minutes,&direction);

    degrees = (degrees + (minutes / 60)) * ((direction == 'W' || direction == 'S') ? -1 : 1);
    return degrees;
}

//Read GPS coordinates and return lat and long - data is in NMEA 0183 format
int gps_get_position(struct Location* position)
{
    unsigned char buf[64]; //Fill up with each line
    unsigned char latitude[12];
    unsigned char longitude[13];
    int read_retries = 0;
    int data_retries = 0;

    latitude[11] = 0;
    longitude[12] = 0;
    
    //Disregard data until $GPRMC is found and is valid
    while(1)
    {
      while (read(tty, &buf[0], 64) <= 0)
      {
        usleep(5000);
        if(read_retries++ > 50)
        return GPS_READ_TIMEOUT;
      }

      if(strncmp((char *)buf, "$GPRMC", 6) == 0)
        //Read in GPS data and get lat and long
        break;

      if(data_retries++ > 20)
        return GPS_DATA_TIMEOUT;
    }

    buf[63] = 0;
    //fprintf(stderr,"\nString: %s\n\n",buf);
    memcpy(latitude, &buf[20],11);
    memcpy(longitude,&buf[32],12);

    position->latitude = convert_nmea_ll((char *)latitude);
    position->longitude = convert_nmea_ll((char *)longitude);
    //if(buf[18] == 'A')
    if(buf[18] == 'V')
    {
      //Data from GPS was formatted correctly, but not trustworthy
      //(inValid), likely satellites could not be seen.
      return GPS_DATA_INVALID;
    }

    //Data being returned is Active (valid/trustworthy)
    return GPS_NO_ERROR;
}

double calc_target_distance(struct Location* pos, struct Location* dest)
{
    double a1,b1,a2,b2,dist;

    a1 = pos->latitude * DtoR;
    b1 = pos->longitude * DtoR;
    a2 = dest->latitude * DtoR;
    b2 = dest->longitude * DtoR;

    dist = acos((cos(a1) * cos(b1) * cos(a2) * cos(b2)) + (cos(a1) * sin(b1) * cos(a2) * sin(b2)) + (sin(a1) * sin(a2))) * R;

    //printf("a1: %f\nb1: %f\na2: %f\nb2: %f\n",a1,b1,a2,b2);
    //printf("distance: %f meters.\n",dist);

    return dist;
}

double calc_target_heading(struct Location* pos, struct Location* dest)
{
    double dlong, lat1, lat2, y, x, bearing;

    lat1 = pos->latitude * DtoR;
    lat2 = dest->latitude * DtoR;
    dlong = (dest->longitude - pos->longitude) * DtoR;
    
    y = sin(dlong) * cos(lat2);
    x = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(dlong));
    bearing = atan2(y,x) * RtoD;
    bearing = (bearing < 0) ? bearing + 360 : bearing;

    return bearing;
}

double get_relative_heading(double compass, double target)
{
    double result = target - compass;
    if (result < -180)
    result += 360;
    if (result > 180)
    result -= 360;
    return result;
}

void gps_close() {
    close(tty);
}
#if 0
main()
{
    double compass, target;
    for (compass = 0; compass < 360; compass += 30)
    {
    for (target = 0; target < 360; target+= 30)
    {
        printf("Aiming at %f, to get to %f, turn at %f.\n",compass,target,get_relative_heading(compass,target));
    }
    }
}
#endif

#if 0
main()
{
    struct Location pos, dest;
    //Test values, at the Geisel end of Warren Mall.
    dest.latitude = 32.881136091348665;
    dest.longitude = -117.23563715815544;

    //Uncomment to spoof NMEA Strings on stdio
    //tty = 0;

    while(gps_init() < 1)
    {}

    while (1)
    {
    if (gps_get_position(&pos) < 1)
        printf("Data inValid.\n");
    printf("From %f,%f to %f,%f: %f  at heading %f\n",pos.latitude,pos.longitude,dest.latitude,dest.longitude,calc_target_distance(&pos,&dest),calc_target_heading(&pos,&dest));
    }
}
#endif
