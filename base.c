/** \file base.c \version 1.0

*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "base.h"

static FILE *g_logfile = NULL;

static LoggerFunc g_logger = plain_logger;//file_logger;

/* State info for file logger.
Not thread safe
*/
static const char *last_file=0, *last_func=0;
static int trailnl =1, hdrlen=0, last_pri=0;


/** The debug level is set to LEV_WARNING by default
If LEV_WARNING is too great then it is set to the max.
*/
#if LOG_LEVEL > LEV_WARNING
static int g_loglev = LEV_WARNING;
#else
static int g_loglev = LOG_LEVEL;
#endif

int find_log_level(int argc, char *argv[]) {
	int i;
	for(i=1; i < argc; i++) {
		if(strlen(argv[i]) == 2 && argv[i][0] == '-') {
			if(argv[i][1] >= '0' && argv[i][1] <= '9')
				return argv[i][1] - '0';
		}
	}
	return -1;
}

void set_log_level(int lev)
{
	if(lev > LOG_LEVEL)
		DIE("Can't set debug higher than compiled LOG_LEVEL (%d)", LOG_LEVEL);
	if(lev >= 0)
		g_loglev = lev;
}

int get_log_level(void)
{
	return g_loglev;
}

FILE *get_log_file(void)
{
	if(g_logfile)
		return g_logfile;
	return stderr;
}

Err set_log_file(const char *filename)
{
	FILE *file = NULL;
	if(filename && !(file = fopen(filename, "w")))
		return_W(E_FOPEN, "Can't open \'%s\' for writing", filename);
	g_logfile = file;
	return_SUCCESS;
}

void set_log_stream(FILE *stream)
{
	g_logfile = stream;
}

static const char *get_pri_str(int pri)
{
	switch(pri) {
		case LEV_EMERG: return "EMERGENCY";
		case LEV_ERR: return "ERROR";
		case LEV_WARNING: return "WARNING";
		case LEV_INFO: return "INFO";
		case LEV_DEBUG: return "DEBUG";
		default: return "";
	}
}

#define MAXCHARS 1024
void printd(int pri, const char *file, const char *func, const char *fmt, ...)
{
	char str[1024];
	
	va_list ap;
	if(fmt) {
		va_start(ap, fmt);
		vsnprintf(str, 1024, fmt, ap);
		va_end(ap);
	}
	g_logger(pri, file, func, str);
}



static int print_special(FILE *log, int indent, const char *str)
{
	int i, j, len = strlen(str), trailnl = 0;
	for(i=0; i < len; i++) {
		fputc(str[i], log);
		if(str[i] == '\n') {
			if(i == len-1) {
				trailnl = 1;
				break;
			}
			for(j=0; j < indent; j++)
				fputc(' ', log);
		}
	}
	return trailnl;
}

void plain_logger(int pri, const char *file, const char *func, const char *str)
{
	if(pri <= g_loglev)
		printf(str); fflush(stdout);
}

void file_logger(int pri, const char *file, const char *func, const char *str)
{
	int i;
	if(pri <= g_loglev) {
		FILE *log = get_log_file();
		if(file == last_file && func == last_func && pri == last_pri) {
			if(trailnl)
				for(i=0; i < hdrlen; i++)
					fputc(' ', log);
			trailnl = print_special(log, hdrlen, str);
		} else {
			char hdr[128];
			hdr[0] = 0;
			if(pri <= LEV_ERR)
				snprintf(hdr, 127, "%s:%s:%s: ", get_pri_str(pri), file, func);
			hdrlen = strlen(hdr);
			if(!trailnl)
				fputc('\n', log);
			fputs(hdr, log);
			trailnl = print_special(log, hdrlen, str);
			last_file = file;
			last_func = func;
			last_pri = pri;
		}
	}
}

void safe_free(void *ptr)
{
	if(ptr) free(ptr);
}

void *safe_realloc(void *ptr, int size)
{
	void *tmp = realloc(ptr, size);
	if(!tmp && size)
		DIE("Memory");
	return tmp;
}

void *safe_malloc(int size)
{
	void *tmp = malloc(size);
	if(!tmp && size)
		DIE("Memory");
	return tmp;
}

void zero_mem(void *ptr, int size)
{
	memset(ptr, 0, size);
}
