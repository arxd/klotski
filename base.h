/** \file base.h \version 1.0
This is not thread safe yet.  It uses a global logger that is not synced.
*/
#ifndef BASE_H
#define BASE_H

/* explicity include these if you need them */
#include <stdio.h>
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifdef __cplusplus
#define BEGIN_C_DECL extern "C" {
#else
# define BEGIN_C_DECL
#endif

#ifdef __cplusplus
#define END_C_DECL }
#else
# define END_C_DECL
#endif
	
BEGIN_C_DECL

typedef long long Int64;
typedef unsigned long long UInt64;
typedef int Int32;
typedef unsigned int UInt32;
typedef unsigned int UInt;
typedef short Int16;
typedef unsigned short UInt16;
typedef char Int8;
typedef unsigned char UInt8;
typedef int Bool;
/** This should be the same size  as a Pointer (void*) */
typedef unsigned long UIntP;
	
/** \name Logging system
\{ */
#ifndef LOG_LEVEL
#define LOG_LEVEL LEV_WARNING
#endif

/** Same as in the linux kernel */
#define	LEV_EMERG 0	/* system is unusable */
//#define	LEV_ALERT 1	/* action must be taken immediately */
//#define	LEV_CRIT 2	/* critical conditions */
#define	LEV_ERR 3	/* error conditions */
#define	LEV_WARNING 4	/* warning conditions */
//#define	LEV_NOTICE 5	/* normal but significant condition */
#define	LEV_INFO 6	/* informational */
#define	LEV_DEBUG 7	/* debug-level messages */
#define	LEV_DEBUG2 8 /* lower level debugging */
#define	LEV_DEBUG3 9 /* even lower level debugging */


#define LOG(lev, fmt, args...) printf("%d:%s:%s:" fmt, __LINE__, __FILE__, __func__ , ##args)

#if LOG_LEVEL >= LEV_INFO
#  define LOG_INFO(fmt, args...)  LOG(LEV_INFO,  fmt, ##args)
#else
#  define LOG_INFO(fmt, args...)
#endif 

#if LOG_LEVEL >= LEV_ERR
#  define LOG_ERR(fmt, args...)  LOG(LEV_ERR,  fmt , ##args)
#else
#  define LOG_ERR(fmt, args...)
#endif 

#if LOG_LEVEL >= LEV_WARNING
#  define LOG_WARNING(fmt, args...)  LOG(LEV_WARNING,  fmt, ##args)
#else
#  define LOG_WARNING(fmt, args...)
#endif

#if LOG_LEVEL >= LEV_DEBUG
#  define LOG_DEBUG(fmt, args...)  LOG(LEV_DEBUG,  fmt, ##args)
#else
#  define LOG_DEBUG(fmt, args...)
#endif 
/** \} */

#define DBG(fmt, args...) do{fprintf(stderr, fmt , ##args); fflush(stderr);}while(0)

#define FUNCTION_ENTER
#define FUNCTION_EXIT

/** \name Standard set of errors.
The first two characters are cast to (unsigned short) and indicate the ErrorCode
ErrorCodes starting in 'e' indicate that this object should not be used further.
ErrorCodes starting in 'w' indicate that the operation failed and the state may 
	be different from what you expect, but the error was handled internally and the object may
	be used
\{ */
#define SUCCESS       0
#define E_UNKNOWN  100
#define E_IO       101
#define E_MEM      102
#define E_SUPPORT  103
#define E_FORMAT   104
#define E_READ     105
#define E_WRITE    106
#define E_CREATE   107
#define E_FOPEN    109
#define E_FIND     110
#define E_ASSERT 111
#define E_BOUNDS 112
/** \} */

#define return_SUCCESS return 0;
#define return_E(err, fmt, args...) do{LOG_ERR(fmt "\n", ##args); return (err); }while(0)
#define return_W(err, fmt, args...) do{LOG_WARNING(fmt "\n", ##args); return -(err); }while(0)

#define CRIT(f) if(f) DIE("Critical Function Failed : Line %d", __LINE__)

#if LOG_LEVEL >= LEV_DEBUG
#   define NEVER(fmt, args...) DIE("This should never happen: " fmt , ##args)
#   define ASSERT(expr, fmt, args...) if(!(expr)) DIE("%d:Assertion Failed (" #expr "): " fmt "\n", __LINE__ , ##args)
#else
#   define NEVER(fmt, args...)
#   define ASSERT(expr, fmt, args...) (expr)
#endif /* DEBUG_ASSERTS */

/**\} */

/** Use this to terminate the process abnormaly.
Useage: DIE(char *fmt, ...)
*/
#define DIE(fmt, args...) do{ \
	LOG(0, "\n" fmt "\n", ##args);\
	char *p=0;*p=0;exit(1);}while(0)

/* Typedefs
 ***********/
typedef int Err; /**< This indicates it can be an error */
typedef int Warn; /**< This indicates warnings only */

typedef void (*LoggerFunc) (	int pri, const char *file,
	const char *func, const char *str);


/* Functions
 ************/
FILE *get_log_file(void);
int find_log_level(int argc, char *argv[]);
Err set_log_file(const char *filename);
void set_log_stream(FILE *stream);
void set_log_level(int lev);
int get_log_level(void);
void printd(int pri, const char *file, const char *func, const char *fmt, ...);

/* Loggers
*/
void file_logger(int pri, const char *file, const char *func,  const char *str);
void plain_logger(int pri, const char *file, const char *func,  const char *str);

/* Extra
 *******/
void safe_free(void *ptr);
void *safe_realloc(void *ptr, int size);
void *safe_malloc(int size);
void zero_mem(void *ptr, int size);

END_C_DECL

#endif /* BASE_H */
