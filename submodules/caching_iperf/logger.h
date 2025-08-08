#include <stdbool.h>
#include <stdarg.h>
typedef enum
{
  INFO,
  VERBOSE,
  ERROR,
  DEBUG,
  LOGPRINT_MAX
} LogprintTag_e;

void _logprint(LogprintTag_e tag, const char* format, ...);

extern const char logprintTagChars[LOGPRINT_MAX];
extern bool logprintTags[LOGPRINT_MAX];

#define loginfo(...) _logprint(INFO, __VA_ARGS__) // default
#define logverbose(...) _logprint(VERBOSE, __VA_ARGS__)
#define logdebug(...) _logprint(DEBUG, __VA_ARGS__)
#define logerror(...) _logprint(ERROR, __VA_ARGS__)
