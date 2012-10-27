/*
 * utilities.
 * Copyright (C) 2012 kazuya kawaguchi. See Copyright Notice in mappedbuffer.h
 */

#ifndef __UTILS_H__
#define __UTILS_H__


#define SAFE_REQ_ATTR_FREE(Req, AttrName) \
  if (Req->AttrName != NULL) { \
    free(Req->AttrName); \
    Req->AttrName = NULL; \
  }

#define SAFE_REQ_ATTR_DELETE(Req, AttrName) \
  if (Req->AttrName) { \
    delete Req->AttrName; \
    Req->AttrName = NULL; \
  }

#define DEFINE_JS_CONSTANT(Target, Name, Value) \
  Target->Set( \
    String::NewSymbol(Name), Integer::New(Value), \
    static_cast<PropertyAttribute>(ReadOnly | DontDelete) \
  )

#define DEFINE_JS_METHOD(Target, Name, Method) \
  Target->Set( \
    String::NewSymbol(Name), \
    FunctionTemplate::New(Method)->GetFunction() \
  )


#endif /* __UTILS_H__ */
