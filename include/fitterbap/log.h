/*
 * Copyright 2014-2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file
 *
 * \brief Trivial logging support.
 */

#ifndef FBP_LOG_H_
#define FBP_LOG_H_

#include "cmacro_inc.h"
#include "fitterbap/config.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_log Console logging
 *
 * @brief Generic console logging with compile-time levels.
 *
 * To use this module, call fbp_log_initialize() with the appropriate
 * handler for your application.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @def FBP_LOG_GLOBAL_LEVEL
 *
 * @brief The global logging level.
 *
 * The maximum level to compile regardless of the individual module level.
 * This value should be defined in the project CMake (makefile).
 */
#ifndef FBP_LOG_GLOBAL_LEVEL
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL
#endif

/**
 * @def FBP_LOG_LEVEL
 *
 * @brief The module logging level.
 *
 * Typical usage 1:  (not MISRA C compliant, but safe)
 *
 *      #define FBP_LOG_LEVEL FBP_LOG_LEVEL_WARNING
 *      #include "log.h"
 */
#ifndef FBP_LOG_LEVEL
#define FBP_LOG_LEVEL FBP_LOG_LEVEL_INFO
#endif

/**
 * @def \_\_FILENAME\_\_
 *
 * @brief The filename to display for logging.
 *
 * When compiling C and C++ code, the __FILE__ define may contain a long path
 * that just confuses the log output.  The build tools, such as make and cmake,
 * can define __FILENAME__ to produce more meaningful results.
 *
 * A good Makefile usage includes:
 *
 */
#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

#ifdef __GNUC__
/* https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html */
#define FBP_LOG_PRINTF_FORMAT __attribute__((format (printf, 1, 2)))
#else
#define FBP_LOG_PRINTF_FORMAT
#endif

/**
 * @brief The printf-style function.
 *
 * @param format The print-style formatting string.
 * The remaining parameters are arguments for the formatting string.
 * @return The number of characters printed.
 *
 * For PC-based applications, a common implementation is::
 *
 *     #include <stdarg.h>
 *     #include <stdio.h>
 *
 *     void fbp_log_printf(const char * format, ...) {
 *         va_list arg;
 *         va_start(arg, format);
 *         vprintf(format, arg);
 *         va_end(arg);
 *     }
 *
 * If your application calls the LOG* macros from multiple threads, then
 * the fbp_log_printf implementation must be thread-safe and reentrant.
 */
typedef void(*fbp_log_printf)(const char * format, ...) FBP_LOG_PRINTF_FORMAT;

extern volatile fbp_log_printf fbp_log_printf_;

/**
 * @brief Initialize the logging feature.
 *
 * @param handler The log handler.  Pass NULL or call fbp_log_finalize() to
 *      restore the default log handler.
 *
 * @return 0 or error code.
 *
 * The library initializes with a default null log handler so that logging
 * which occurs before fbp_log_initialize will not cause a fault.  This function
 * may be safely called at any time, even without finalize.
 */
FBP_API int fbp_log_initialize(fbp_log_printf handler);

/**
 * @brief Finalize the logging feature.
 *
 * This is equivalent to calling fbp_log_initialize(0).
 */
FBP_API void fbp_log_finalize();

/**
 * @def FBP_LOG_PRINTF
 * @brief The printf function including log formatting.
 *
 * @param level The level for this log message
 * @param format The formatting string
 * @param ... The arguments for the formatting string
 */
#ifndef FBP_LOG_PRINTF
#define FBP_LOG_PRINTF(level, format, ...) \
    fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif

/**
 * @brief The available logging levels.
 */
enum fbp_log_level_e {
    /** Logging functionality is disabled. */
    FBP_LOG_LEVEL_OFF         = -1,
    /** A "panic" condition that may result in significant harm. */
    FBP_LOG_LEVEL_EMERGENCY   = 0,
    /** A condition requiring immediate action. */
    FBP_LOG_LEVEL_ALERT       = 1,
    /** A critical error which prevents further functions. */
    FBP_LOG_LEVEL_CRITICAL    = 2,
    /** An error which prevents the current operation from completing or
     *  will adversely effect future functionality. */
    FBP_LOG_LEVEL_ERROR       = 3,
    /** A warning which may adversely affect the current operation or future
     *  operations. */
    FBP_LOG_LEVEL_WARNING     = 4,
    /** A notification for interesting events. */
    FBP_LOG_LEVEL_NOTICE      = 5,
    /** An informative message. */
    FBP_LOG_LEVEL_INFO        = 6,
    /** Detailed messages for the software developer. */
    FBP_LOG_LEVEL_DEBUG1      = 7,
    /** Very detailed messages for the software developer. */
    FBP_LOG_LEVEL_DEBUG2      = 8,
    /** Insanely detailed messages for the software developer. */
    FBP_LOG_LEVEL_DEBUG3      = 9,
    /** All logging functionality is enabled. */
    FBP_LOG_LEVEL_ALL         = 10,
};

/** Detailed messages for the software developer. */
#define FBP_LOG_LEVEL_DEBUG FBP_LOG_LEVEL_DEBUG1

/**
 * @brief Map log level to a string name.
 */
extern char const * const fbp_log_level_str[FBP_LOG_LEVEL_ALL + 1];

/**
 * @brief Map log level to a single character.
 */
extern char const fbp_log_level_char[FBP_LOG_LEVEL_ALL + 1];

/**
 * @brief Check the current level against the static logging configuration.
 *
 * @param level The level to query.
 * @return True if logging at level is permitted.
 */
#define FBP_LOG_CHECK_STATIC(level) ((level <= FBP_LOG_GLOBAL_LEVEL) && (level <= FBP_LOG_LEVEL) && (level >= 0))

/**
 * @brief Check a log level against a configured level.
 *
 * @param level The level to query.
 * @param cfg_level The configured logging level.
 * @return True if level is permitted given cfg_level.
 */
#define FBP_LOG_LEVEL_CHECK(level, cfg_level) (level <= cfg_level)

/*!
 * \brief Macro to log a printf-compatible formatted string.
 *
 * \param level The fbp_log_level_e.
 * \param format The printf-compatible formatting string.
 * \param ... The arguments to the formatting string.
 */
#define FBP_LOG(level, format, ...) \
    do { \
        if (FBP_LOG_CHECK_STATIC(level)) { \
            FBP_LOG_PRINTF(level, format, __VA_ARGS__); \
        } \
    } while (0)


#ifdef _MSC_VER
/* Microsoft Visual Studio compiler support */
/** Log a emergency using printf-style arguments. */
#define FBP_LOG_EMERGENCY(format, ...)  FBP_LOG(FBP_LOG_LEVEL_EMERGENCY, format, __VA_ARGS__)
/** Log a alert using printf-style arguments. */
#define FBP_LOG_ALERT(format, ...)  FBP_LOG(FBP_LOG_LEVEL_ALERT, format, __VA_ARGS__)
/** Log a critical failure using printf-style arguments. */
#define FBP_LOG_CRITICAL(format, ...)  FBP_LOG(FBP_LOG_LEVEL_CRITICAL, format, __VA_ARGS__)
/** Log an error using printf-style arguments. */
#define FBP_LOG_ERROR(format, ...)     FBP_LOG(FBP_LOG_LEVEL_ERROR, format, __VA_ARGS__)
/** Log a warning using printf-style arguments. */
#define FBP_LOG_WARNING(format, ...)      FBP_LOG(FBP_LOG_LEVEL_WARNING, format, __VA_ARGS__)
/** Log a notice using printf-style arguments. */
#define FBP_LOG_NOTICE(format, ...)    FBP_LOG(FBP_LOG_LEVEL_NOTICE,   format, __VA_ARGS__)
/** Log an informative message using printf-style arguments. */
#define FBP_LOG_INFO(format, ...)      FBP_LOG(FBP_LOG_LEVEL_INFO,     format, __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG1(format, ...)    FBP_LOG(FBP_LOG_LEVEL_DEBUG1,    format, __VA_ARGS__)
/** Log a very detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG2(format, ...)    FBP_LOG(FBP_LOG_LEVEL_DEBUG2,  format, __VA_ARGS__)
/** Log an insanely detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG3(format, ...)    FBP_LOG(FBP_LOG_LEVEL_DEBUG3,  format, __VA_ARGS__)

#else
/* GCC compiler support */
// zero length variadic arguments are not allowed for macros
// this hack ensures that LOG(message) and LOG(format, args...) are both supported.
// https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick
#define _FBP_LOG_SELECT(PREFIX, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, SUFFIX, ...) PREFIX ## _ ## SUFFIX
#define _FBP_LOG_1(level, message) FBP_LOG(level, "%s", message)
#define _FBP_LOG_N(level, format, ...) FBP_LOG(level, format, __VA_ARGS__)
#define _FBP_LOG_DISPATCH(level, ...)  _FBP_LOG_SELECT(_FBP_LOG, __VA_ARGS__, N, N, N, N, N, N, N, N, N, N, 1, 0)(level, __VA_ARGS__)

/** Log a emergency using printf-style arguments. */
#define FBP_LOG_EMERGENCY(...)  _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_EMERGENCY, __VA_ARGS__)
/** Log a alert using printf-style arguments. */
#define FBP_LOG_ALERT(...)  _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_ALERT, __VA_ARGS__)
/** Log a critical failure using printf-style arguments. */
#define FBP_LOG_CRITICAL(...)  _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_CRITICAL, __VA_ARGS__)
/** Log an error using printf-style arguments. */
#define FBP_LOG_ERROR(...)     _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_ERROR, __VA_ARGS__)
/** Log a warning using printf-style arguments. */
#define FBP_LOG_WARNING(...)      _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_WARNING, __VA_ARGS__)
/** Log a notice using printf-style arguments. */
#define FBP_LOG_NOTICE(...)    _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_NOTICE,   __VA_ARGS__)
/** Log an informative message using printf-style arguments. */
#define FBP_LOG_INFO(...)      _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_INFO,     __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG1(...)    _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_DEBUG1,    __VA_ARGS__)
/** Log a very detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG2(...)    _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_DEBUG2,  __VA_ARGS__)
/** Log an insanely detailed debug message using printf-style arguments. */
#define FBP_LOG_DEBUG3(...)    _FBP_LOG_DISPATCH(FBP_LOG_LEVEL_DEBUG3,  __VA_ARGS__)
#endif

/** Log an error using printf-style arguments.  Alias for FBP_LOG_ERROR. */
#define FBP_LOG_ERR FBP_LOG_ERROR
/** Log a warning using printf-style arguments.  Alias for FBP_LOG_WARNING. */
#define FBP_LOG_WARN FBP_LOG_WARNING
/** Log a detailed debug message using printf-style arguments.  Alias for FBP_LOG_DEBUG1. */
#define FBP_LOG_DEBUG FBP_LOG_DEBUG1
/** Log a detailed debug message using printf-style arguments.  Alias for FBP_LOG_DEBUG1. */
#define FBP_LOG_DBG FBP_LOG_DEBUG1

#define FBP_LOGE FBP_LOG_ERROR
#define FBP_LOGW FBP_LOG_WARNING
#define FBP_LOGN FBP_LOG_NOTICE
#define FBP_LOGI FBP_LOG_INFO
#define FBP_LOGD FBP_LOG_DEBUG1
#define FBP_LOGD1 FBP_LOG_DEBUG1
#define FBP_LOGD2 FBP_LOG_DEBUG2
#define FBP_LOGD3 FBP_LOG_DEBUG3

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_LOG_H_ */
