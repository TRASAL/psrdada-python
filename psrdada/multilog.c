
#include "multilog.h"
#include "dada_def.h"

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

// #define _DEBUG 1

multilog_t* multilog_open (const char* program_name, char syslog)
{
  multilog_t* m = (multilog_t*) malloc (sizeof(multilog_t));
  assert (m != 0);

  if (syslog)
    openlog (program_name, LOG_CONS, LOG_USER);

  m->syslog = syslog;
  m->name = strdup(program_name);
  assert (m->name != 0);
  m->logs = 0;
  m->nlog = 0;
  m->port = 0;
  m->timestamp = 1;

  pthread_mutex_init(&(m->mutex), NULL);

  return m;
}

int multilog_close (multilog_t* m)
{
  pthread_mutex_lock (&(m->mutex));

  free (m->name);
  if (m->logs)
    free (m->logs);
  m->logs = 0;

  pthread_mutex_unlock (&(m->mutex));
  pthread_mutex_destroy (&(m->mutex));

  free (m);
  return 0;
}

int multilog_add (multilog_t* m, FILE* fptr)
{
  pthread_mutex_lock (&(m->mutex));

  m->logs = realloc (m->logs, (m->nlog+1)*sizeof(multilog_t));
  assert (m->logs != 0);
  m->logs[m->nlog] = fptr;
  m->nlog ++;

  pthread_mutex_unlock (&(m->mutex));

  return 0;
}

int multilog (multilog_t* m, int priority, const char* format, ...)
{
  unsigned ilog = 0;
  va_list arguments;

  if (!m)
    return -1;

  va_start (arguments, format);

  if (m->syslog)
    vsyslog (priority, format, arguments);

  pthread_mutex_lock (&(m->mutex));

#ifdef _DEBUG
  fprintf (stderr, "multilog: %d logs\n", m->nlog);
#endif

  va_end(arguments);

  for (ilog=0; ilog < m->nlog; ilog++)  {
    va_start(arguments, format);
          
    if (ferror (m->logs[ilog]))  {
#ifdef _DEBUG
      fprintf (stderr, "multilog: error on log[%d]", ilog);
#endif
      fclose (m->logs[ilog]);
      m->logs[ilog] = m->logs[m->nlog-1];
      m->nlog --;
      ilog --;
    }
    else {
      if (m->timestamp) {
        unsigned buffer_size = 64;
        static char* buffer = 0;
        if (!buffer)
          buffer = malloc (buffer_size);
        assert (buffer != 0);
        time_t now = time(0);
        strftime (buffer, buffer_size, DADA_TIMESTR,
                         (struct tm*) localtime(&now));
        fprintf(m->logs[ilog],"[%s] ",buffer);
      }
             
      if (priority == LOG_ERR) fprintf(m->logs[ilog], "ERR: ");
      if (priority == LOG_WARNING) fprintf(m->logs[ilog], "WARN: ");
      //fprintf (m->logs[ilog], "%s: ", m->name);
      if (vfprintf (m->logs[ilog], format, arguments) < 0)
        perror ("multilog: error vfprintf");
    }
    va_end(arguments);
  }

  pthread_mutex_unlock (&(m->mutex));

  return 0;
}

int multilog_fprintf(FILE* stream, int priority, const char* format, ...)
{

  va_list arguments;

  if (!stream)
    return -1;

  va_start(arguments, format);

  unsigned buffer_size = DADA_TIMESTR_LENGTH;
  static char* buffer = 0;
  if (!buffer)
    buffer = malloc (buffer_size);
  assert (buffer != 0);
  time_t now = time(0);
  strftime (buffer, buffer_size, DADA_TIMESTR, (struct tm*) localtime(&now));
  fprintf(stream,"[%s] ",buffer);

  if (priority == LOG_ERR) fprintf(stream, "ERR: ");
  if (priority == LOG_WARNING) fprintf(stream, "WARN: ");
  if (vfprintf (stream, format, arguments) < 0) 
    perror ("multilog: error vfprintf");

  va_end(arguments);

  return 0;

}
