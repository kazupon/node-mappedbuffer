/*
 * debug utilities.
 * Copyright (C) 2012 kazuya kawaguchi. See Copyright Notice in mappedbuffer.h
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#if defined(NDEBUG) && NDEBUG
#define TRACE(fmt, ...)     ((void)0)
#else
#define TRACE(fmt, ...) \
  do { \
    fprintf(stderr, "%s: %d: (%p) %s: " fmt, __FILE__, __LINE__, pthread_self(), __func__, ##__VA_ARGS__); \
  } while (0)
#endif /* DEBUG */


#endif /* __DEBUG_H__ */

