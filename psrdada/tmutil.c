
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#include "tmutil.h"

time_t str2time (const char* str)
{
  struct tm time;
  return str2tm (&time, str);
}

time_t str2utctime (const char* str)
{
  struct tm time;
  return str2utctm (&time, str);
}


time_t str2tm (struct tm* time, const char* str)
{
  char* temp = 0;         /* duplicate of input */
  int   trav = 0;         /* travelling index */
  int   endstr = 0;       /* length of string */
  char  infield = 0;      /* true when current character is a digit */
  int   field_count = 0;  /* incremented when character becomes digit */
  int   digits = 0;       /* count of digits in string */

  time->tm_year = 0;
  time->tm_mon = 0;
  time->tm_mday = 0;
  time->tm_hour = 0;
  time->tm_min = 0;
  time->tm_sec = 0;

  temp = strdup (str);

  /* count the number of fields and cut the string off after a year, day,
     hour, minute, and second can be parsed */
  while (temp[trav] != '\0') {
    if (isdigit(temp[trav])) {
      digits ++;
      if (!infield) {
        /* count only the transitions from non digits to a field of digits */
        field_count ++;
      }
      infield = 1;
    }
    else {
      infield = 0;
    }
    if (field_count == 6) {
      /* currently in the seconds field */
      temp[trav+2] = '\0';
      break;
    }
    else if (digits == 14) {
      /* enough digits for a date */
      temp[trav+1] = '\0';
      break;
    }
    trav ++;
  }

  endstr = strlen(temp);
  /* cut off any trailing characters that are not ASCII numbers */
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return -1;
  temp [endstr+1] = '\0'; 


  /* parse UTC seconds */
  trav = endstr - 1;
  if ((trav < 0) || !isdigit(temp[trav]))
    trav++;
  sscanf (temp+trav, "%2d", &(time->tm_sec));

  /* cut out seconds and extra characters */
  endstr = trav-1;
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return 0;
  temp [endstr+1] = '\0'; 

  /* parse UTC minutes */
  trav = endstr - 1;
  if ((trav < 0) || !isdigit(temp[trav]))
    trav++;
  sscanf (temp+trav, "%2d", &(time->tm_min));

  /* cut out minutes and extra characters */
  endstr = trav-1;
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return 0;
  temp [endstr+1] = '\0'; 

  /* parse UTC hours */
  trav = endstr - 1;
  if ((trav < 0) || !isdigit(temp[trav]))
    trav++;
  sscanf (temp+trav, "%2d", &(time->tm_hour));

  /* cut out minutes and extra characters */
  endstr = trav-1;
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return 0;
  temp [endstr+1] = '\0'; 

  /* parse UTC days in month */
  trav = endstr - 1;
  if ((trav < 0) || !isdigit(temp[trav]))
    trav++;
  sscanf (temp+trav, "%2d", &(time->tm_mday));

  /* cut out minutes and extra characters */
  endstr = trav-1;
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return 0;
  temp [endstr+1] = '\0'; 

  /* parse UTC months */
  trav = endstr - 1;
  if ((trav < 0) || !isdigit(temp[trav]))
    trav++;
  sscanf (temp+trav, "%2d", &(time->tm_mon));
  /* month is stored 0->11 in struct tm */
  time->tm_mon --;

  /* cut out minutes and extra characters */
  endstr = trav-1;
  while ((endstr>=0) && !isdigit(temp[endstr])) endstr --;
  if (endstr < 0)
    return 0;
  temp [endstr+1] = '\0'; 

  /* parse UTC year */
  trav = endstr;
  while ((trav >= 0) && (endstr-trav < 4) && isdigit(temp[trav]))
    trav--;
  sscanf (temp+trav+1, "%4d", &(time->tm_year));

  free (temp);

  time->tm_wday = 0;
  time->tm_yday = 0;
  time->tm_isdst = -1;

  /* this may cause a Y3.8K bug */
  if (time->tm_year > 1900)
    time->tm_year -= 1900;

  /* Y2K bug assumption */
  if (time->tm_year < 30)
     time->tm_year += 100;

  return mktime (time);
  
} 

time_t str2utctm (struct tm* time, const char* str)
{
  
  /* append the GMT+0 timeszone information */
  char * str_utc = malloc(sizeof(char) * (strlen(str) + 4 + 1));
  sprintf(str_utc, "%s UTC",str);

  const char * format = "%Y-%m-%d-%H:%M:%S %Z";
  
  strptime(str_utc, format, time);

  free(str_utc);
  return timegm(time);
}

time_t mjd2utctm (double mjd)
{
  const int seconds_in_day = 86400;
  int days = (int) mjd;
  double fdays = mjd - (double) days;
  double seconds = fdays * (double) seconds_in_day;
  int secs = (int) seconds;
  double fracsec = seconds - (double) secs;
  if (fracsec - 1 < 0.0000001)
    secs++;

  int julian_day = days + 2400001;

  int n_four = 4  * (julian_day+((6*((4*julian_day-17918)/146097))/4+1)/2-37);
  int n_dten = 10 * (((n_four-237)%1461)/4) + 5;

  struct tm gregdate;
  gregdate.tm_year = n_four/1461 - 4712 - 1900; // extra -1900 for C struct tm
  gregdate.tm_mon  = (n_dten/306+2)%12;         // struct tm mon 0->11
  gregdate.tm_mday = (n_dten%306)/10 + 1;

  gregdate.tm_hour = secs / 3600;
  secs -= 3600 * gregdate.tm_hour;


  gregdate.tm_min = secs / 60;
  secs -= 60 * (gregdate.tm_min);

  gregdate.tm_sec = secs;

  gregdate.tm_isdst = -1;
  time_t date = mktime (&gregdate);

  return date;

}

void float_sleep (float seconds)
{
  struct timeval t ;
  t.tv_sec = seconds;
  seconds -= t.tv_sec;
  t.tv_usec = seconds * 1e6;
  select (0, 0, 0, 0, &t) ;
}


