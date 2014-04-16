/*
 * mappedbuffer main
 * Copyright (C) 2012 kazuya kawaguchi. See Copyright Notice in mappedbuffer.h
 */

#define BUILDING_NODE_EXTENSION

#include "mappedbuffer.h"
#include "debug.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>


enum mb_async_t {
  MB_ASYNC_MMAP,
  MB_ASYNC_MUNMAP,
  MB_ASYNC_FILL,
  MB_ASYNC_MSYNC,
};

#define MB_ERR_NO_MAP(XX) \
  XX(-1, UNKNWON, "Unknown error") \
  XX(0, OK, "Success") \
  XX(1, MMAP, "Failed mmap") \
  XX(2, MUNMAP, "Failed munmap") \
  XX(3, MSYNC, "Failed msync")

#define MB_ERR_NO_GEN(Val, Name, Str) MB_ERR_##Name = Val,
typedef enum {
  MB_ERR_NO_MAP(MB_ERR_NO_GEN)
} mb_err_code;
#undef MB_ERR_NO_GEN


#define MB_REQ_FIELD \
  mb_async_t type; \
  mb_err_code code; \
  Persistent<Function> cb

struct mb_req_t {
  MB_REQ_FIELD;
};

struct mb_mmap_req_t {
  MB_REQ_FIELD;
  Persistent<Object> obj;
  size_t size;
  int32_t protection;
  int32_t flags;
  int32_t fd;
  off_t offset;
  char *map;
};

struct mb_munmap_req_t {
  MB_REQ_FIELD;
  MappedBuffer *buffer;
};

struct mb_fill_req_t {
  MB_REQ_FIELD;
  MappedBuffer *buffer;
  int32_t value;
  int32_t start;
  int32_t end;
};

struct mb_msync_req_t {
  MB_REQ_FIELD;
  MappedBuffer *buffer;
  size_t size;
  int32_t flags;
};


static Persistent<String> length_symbol;
Persistent<Function> MappedBuffer::ctor;


#define MB_STR_ERR_GEN(Val, Name, Str) case MB_ERR_##Name : return Str;
static const char* mb_strerror(mb_err_code code) {
  switch (code) {
    MB_ERR_NO_MAP(MB_STR_ERR_GEN)
    default:
      return "Unknown system error";
  }
}
#undef MB_STR_ERR_GEN


MappedBuffer::MappedBuffer(Handle<Object> wrapper, size_t length, char *data)
  : map_(data), length_(length), released_(false), ObjectWrap() {
  TRACE("constructor: length_=%ld, map_=%p\n", length_, map_);
  assert(length_ > 0 && map_ != NULL);
  Wrap(wrapper);

  handle_->Set(length_symbol, Integer::NewFromUnsigned(length_));
  handle_->SetIndexedPropertiesToExternalArrayData(
    map_, kExternalUnsignedByteArray, length_
  );
}

MappedBuffer::~MappedBuffer() {
  TRACE("destructor\n");
  if (!released_ && map_ != NULL && length_ > 0) {
    int32_t ret = munmap(map_, length_);
    TRACE("munmap: ret=%d\n", ret);
    released_ = true;
  }
}

bool MappedBuffer::unmap() {
  TRACE("map_=%p, length_=%ld, released=%d\n", map_, length_, released_);
  if (released_) {
    return false;
  }
  assert(!released_);

  int32_t ret = munmap(map_, length_);
  TRACE("munmap: ret=%d\n", ret);
  map_ = NULL;
  length_ = 0;
  handle_->Set(length_symbol, Integer::NewFromUnsigned(length_));
  released_ = true;
  return true;
}

Handle<Value> MappedBuffer::New(const Arguments &args) {
  HandleScope scope;
  TRACE("New\n");

  if (!args.IsConstructCall()) {
    const int argc = args.Length();
    Local<Value> *argv = new Local<Value>[argc];
    for (int i = 0; i < argc; ++i) {
      argv[i] = args[i];
    }
    Local<Object> instance = ctor->NewInstance(argc, argv);
    delete[] argv;
    return scope.Close(instance);
  }

	if (args.Length() <= 3) {
		return ThrowException(Exception::Error(String::New("Bad argument")));
	}

  const size_t size = args[0]->ToInteger()->Value();
  const int32_t protection = args[1]->ToInteger()->Value();
  const int32_t flags = args[2]->ToInteger()->Value();
  const int32_t fd = args[3]->ToInteger()->Value();
  mb_mmap_req_t *req = NULL;
  off_t offset = 0;

  if (args.Length() == 5) {
    if (args[4]->IsFunction()) {
      req = reinterpret_cast<mb_mmap_req_t *>(malloc(sizeof(mb_mmap_req_t)));
      req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[4]));
      req->code = MB_ERR_OK;
      req->obj = Persistent<Object>::New(Handle<Object>::Cast(args.This()));
      req->type = MB_ASYNC_MMAP;
      req->size = size;
      req->protection = protection;
      req->flags = flags;
      req->fd = fd;
      req->offset = offset;
      req->map = NULL;
    } else {
      offset = args[4]->ToInteger()->Value();
    }
  } else if (args.Length() == 6) {
    req = reinterpret_cast<mb_mmap_req_t *>(malloc(sizeof(mb_mmap_req_t)));
    req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[5]));
    req->code = MB_ERR_OK;
    req->obj = Persistent<Object>::New(Handle<Object>::Cast(args.This()));
    req->type = MB_ASYNC_MMAP;
    req->size = size;
    req->protection = protection;
    req->flags = flags;
    req->fd = fd;
    req->offset = args[4]->ToInteger()->Value();
    req->map = NULL;
  }

  if (req == NULL) { // sync
    char *data = (char *)mmap(NULL, size, protection, flags, fd, offset);
    TRACE("mmap: %p\n", data);
    MappedBuffer *obj = new MappedBuffer(args.This(), size, data);
    return args.This();
  } else { // async
    uv_work_t *uv_req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
    assert(uv_req != NULL);
    uv_req->data = req;

    int32_t ret = uv_queue_work(
      uv_default_loop(), uv_req, MappedBuffer::OnWork, (uv_after_work_cb)MappedBuffer::OnWorkDone
    );
    assert(ret == 0);
    return scope.Close(args.This());
  }
}

Handle<Value> MappedBuffer::Unmap(const Arguments &args) {
  HandleScope scope;
  TRACE("Unmap\n");

  MappedBuffer *mappedbuffer = ObjectWrap::Unwrap<MappedBuffer>(args.This());
  assert(mappedbuffer != NULL);

  if (args.Length() == 1 && args[0]->IsFunction()) {
    if (mappedbuffer->released_) {
      return scope.Close(Boolean::New(true));
    }
    assert(!mappedbuffer->released_);

    mb_munmap_req_t *req = reinterpret_cast<mb_munmap_req_t *>(malloc(sizeof(mb_munmap_req_t)));
    assert(req != NULL);
    req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
    req->code = MB_ERR_OK;
    req->type = MB_ASYNC_MUNMAP;
    req->buffer = mappedbuffer;

    uv_work_t *uv_req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
    assert(uv_req != NULL);
    uv_req->data = req;

    int32_t ret = uv_queue_work(
      uv_default_loop(), uv_req, MappedBuffer::OnWork, (uv_after_work_cb)MappedBuffer::OnWorkDone
    );
    assert(ret == 0);

    mappedbuffer->Ref();
    return scope.Close(args.This());
  } else {
    if (mappedbuffer->released_) {
      return scope.Close(Boolean::New(true));
    }
    assert(!mappedbuffer->released_);

    int32_t ret = munmap(mappedbuffer->map_, mappedbuffer->length_);
    TRACE("munmap: %d\n", ret);
    if (ret == 0) {
      mappedbuffer->map_ = NULL;
      mappedbuffer->length_ = 0;
      mappedbuffer->handle_->Set(length_symbol, Integer::NewFromUnsigned(mappedbuffer->length_));
      mappedbuffer->released_ = true;
      return scope.Close(Boolean::New(true));
    } else if (ret == -1) {
      // TODO: should be checked mumap detail error
      return scope.Close(Boolean::New(false));
    } else {
      return ThrowException(Exception::Error(String::New("Unknown munmap return value")));
    }
  }
}

Handle<Value> MappedBuffer::Fill(const Arguments &args) {
  HandleScope scope;
  TRACE("Fill\n");

	if (args.Length() < 3) {
		return ThrowException(Exception::Error(String::New("Bad argument")));
	}

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::Error(String::New("Value is not a number")));
  }
  const int32_t value = (char)args[0]->Int32Value();

  // TODO; should be checked 'start' value type and range.
  const int32_t start = args[1]->ToInteger()->Value();

  // TODO; should be checked 'end' value type and range.
  const int32_t end = args[2]->ToInteger()->Value();

  MappedBuffer *mappedbuffer = ObjectWrap::Unwrap<MappedBuffer>(args.This());
  assert(mappedbuffer != NULL);

  if (args.Length() == 4 && args[3]->IsFunction()) {
    mb_fill_req_t *req = reinterpret_cast<mb_fill_req_t *>(malloc(sizeof(mb_fill_req_t)));
    assert(req != NULL);
    req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[3]));
    req->code = MB_ERR_OK;
    req->type = MB_ASYNC_FILL;
    req->buffer = mappedbuffer;
    req->value = value;
    req->start = start;
    req->end = end;

    uv_work_t *uv_req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
    assert(uv_req != NULL);
    uv_req->data = req;

    int32_t ret = uv_queue_work(
      uv_default_loop(), uv_req, MappedBuffer::OnWork, (uv_after_work_cb)MappedBuffer::OnWorkDone
    );
    assert(ret == 0);

    mappedbuffer->Ref();
    return scope.Close(args.This());
  } else {
    // TODO: should be checked memset detail error
    memset((void *)(mappedbuffer->map_ + start), value, end - start);
    return scope.Close(args.This());
  }
}

Handle<Value> MappedBuffer::Sync(const Arguments &args) {
  HandleScope scope;
  TRACE("Sync\n");

	if (args.Length() < 2) {
		return ThrowException(Exception::Error(String::New("Bad argument")));
	}

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::Error(String::New("Value is not a number")));
  }
  // TODO; should be checked 'size' value type and range.
  const int32_t size = args[0]->ToInteger()->Value();

  // TODO; should be checked 'flags' range.
  const int32_t flags = args[1]->ToInteger()->Value();

  MappedBuffer *mappedbuffer = ObjectWrap::Unwrap<MappedBuffer>(args.This());
  assert(mappedbuffer != NULL);

  if (args.Length() == 3 && args[2]->IsFunction()) {
    mb_msync_req_t *req = reinterpret_cast<mb_msync_req_t *>(malloc(sizeof(mb_msync_req_t)));
    assert(req != NULL);
    req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[2]));
    req->code = MB_ERR_OK;
    req->type = MB_ASYNC_MSYNC;
    req->buffer = mappedbuffer;
    req->size = size;
    req->flags = flags;

    uv_work_t *uv_req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
    assert(uv_req != NULL);
    uv_req->data = req;

    int32_t ret = uv_queue_work(
      uv_default_loop(), uv_req, MappedBuffer::OnWork, (uv_after_work_cb)MappedBuffer::OnWorkDone
    );
    assert(ret == 0);

    mappedbuffer->Ref();
    return scope.Close(args.This());
  } else {
      // TODO: should be checked msync detail error
    int32_t ret = msync((void *)(mappedbuffer->map_), size, flags);
    TRACE("msync: %d\n", ret);
    return scope.Close(args.This());
  }
}


void MappedBuffer::OnWork(uv_work_t *work_req) {
  TRACE("work_req=%p\n", work_req);
  assert(work_req != NULL);

  mb_req_t *mb_req = reinterpret_cast<mb_req_t *>(work_req->data);
  assert(mb_req != NULL);

  switch (mb_req->type) {
    case MB_ASYNC_MMAP:
      {
        mb_mmap_req_t *req = reinterpret_cast<mb_mmap_req_t *>(work_req->data);
        assert(req != NULL);
        req->map = (char *)mmap(
          NULL, req->size, req->protection, req->flags, req->fd, req->offset
        );
        TRACE("mmap: %p\n", req->map);
        // TODO: should be checked mmap detail error
        if (req->map == MAP_FAILED) {
          req->code = MB_ERR_MMAP;
        }
      }
      break;
    case MB_ASYNC_MUNMAP:
      {
        mb_munmap_req_t *req = reinterpret_cast<mb_munmap_req_t *>(work_req->data);
        assert(req != NULL);
        assert(!req->buffer->released_);

        int32_t ret = munmap(req->buffer->map_, req->buffer->length_);
        TRACE("munmap: %d\n", ret);
        // TODO: should be checked munmap detail error
        if (ret != 0) {
          req->code = MB_ERR_MUNMAP;
        }
      }
      break;
    case MB_ASYNC_FILL:
      {
        mb_fill_req_t *req = reinterpret_cast<mb_fill_req_t *>(work_req->data);
        assert(req != NULL);
        memset((void *)(req->buffer->map_ + req->start), req->value, req->end - req->start);
      }
      break;
    case MB_ASYNC_MSYNC:
      {
        mb_msync_req_t *req = reinterpret_cast<mb_msync_req_t *>(work_req->data);
        int32_t ret = msync((void *)(req->buffer->map_), req->size, req->flags);
        TRACE("msync: %d\n", ret);
        // TODO: should be checked msync detail error
        if (ret != 0) {
          req->code = MB_ERR_MSYNC;
        }
        assert(req != NULL);
      }
    default:
      assert(false);
      break;
  }
}

void MappedBuffer::OnWorkDone(uv_work_t *work_req) {
  TRACE("work_req=%p\n", work_req);
  HandleScope scope;

  mb_req_t *mb_req = reinterpret_cast<mb_req_t *>(work_req->data);
  assert(mb_req != NULL);

  // init callback arguments.
  int32_t argc = 0;
  Local<Value> argv[2] = { 
    Local<Value>::New(Null()),
    Local<Value>::New(Null()),
  };

  // set error to callback arguments.
  if (mb_req->code != MB_ERR_OK) {
    const char *name = mb_strerror(mb_req->code);
    Local<String> message = String::NewSymbol(name);
    Local<Value> err = Exception::Error(message);
    Local<Object> obj = err->ToObject();
    obj->Set(
      String::NewSymbol("code"), Integer::New(mb_req->code),
      static_cast<PropertyAttribute>(ReadOnly | DontDelete)
    );
    argv[argc] = err;
  }
  argc++;

  // operations
  switch (mb_req->type) {
    case MB_ASYNC_MMAP:
      {
        mb_mmap_req_t *req = static_cast<mb_mmap_req_t *>(work_req->data);
        assert(req != NULL);
        MappedBuffer *mappedbuffer = new MappedBuffer(req->obj, req->size, req->map);
        Local<Value> buf = Local<Value>::New(mappedbuffer->handle_);
        argv[argc++] = buf;
      }
      break;
    case MB_ASYNC_MUNMAP:
      {
        mb_munmap_req_t *req = static_cast<mb_munmap_req_t*>(work_req->data);
        assert(req != NULL);
        if (req->code == MB_ERR_OK) {
          req->buffer->map_ = NULL;
          req->buffer->length_ = 0;
          req->buffer->handle_->Set(length_symbol, Integer::NewFromUnsigned(req->buffer->length_));
          req->buffer->released_ = true;
        }
      }
      break;
    case MB_ASYNC_FILL:
    case MB_ASYNC_MSYNC:
      break;
    default:
      assert(false);
      break;
  }

  // execute callback
  if (!mb_req->cb.IsEmpty()) {
    TryCatch try_catch;
    MakeCallback(Context::GetCurrent()->Global(), mb_req->cb, argc, argv);
    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
  } 


  // releases
  mb_req->cb.Dispose();

  switch (mb_req->type) {
    case MB_ASYNC_MMAP:
      {
        mb_mmap_req_t *req = static_cast<mb_mmap_req_t *>(work_req->data);
        assert(req != NULL);
        req->obj.Dispose();
        req->map = NULL;
        free(req);
      }
      break;
    case MB_ASYNC_MUNMAP:
      {
        mb_munmap_req_t *req = static_cast<mb_munmap_req_t*>(work_req->data);
        assert(req != NULL);
        req->buffer->Unref();
        req->buffer = NULL;
        free(req);
      }
      break;
    case MB_ASYNC_FILL:
      {
        mb_fill_req_t *req = static_cast<mb_fill_req_t *>(work_req->data);
        assert(req != NULL);
        req->buffer->Unref();
        req->buffer = NULL;
      }
      break;
    case MB_ASYNC_MSYNC:
      {
        mb_msync_req_t *req = static_cast<mb_msync_req_t *>(work_req->data);
        assert(req != NULL);
        req->buffer->Unref();
        req->buffer = NULL;
      }
      break;
    default:
      assert(false);
      break;
  }

  work_req->data = NULL;
  free(work_req);
}

void MappedBuffer::Init(Handle<Object> target) {
  TRACE("load MappedBuffer module: target=%p\n", &target);

  length_symbol = NODE_PSYMBOL("length");

  // prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(MappedBuffer::New);
  tpl->SetClassName(String::NewSymbol("MappedBuffer"));

  Local<ObjectTemplate> insttpl = tpl->InstanceTemplate();
  insttpl->SetInternalFieldCount(1);

  // prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "unmap", MappedBuffer::Unmap);
  NODE_SET_PROTOTYPE_METHOD(tpl, "fill", MappedBuffer::Fill);
  NODE_SET_PROTOTYPE_METHOD(tpl, "sync", MappedBuffer::Sync);

  ctor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("MappedBuffer"), ctor);

  // define constants
  NODE_DEFINE_CONSTANT(ctor, PROT_READ);
  NODE_DEFINE_CONSTANT(ctor, PROT_NONE);
  NODE_DEFINE_CONSTANT(ctor, PROT_READ);
  NODE_DEFINE_CONSTANT(ctor, PROT_WRITE);
  NODE_DEFINE_CONSTANT(ctor, PROT_EXEC);
  NODE_DEFINE_CONSTANT(ctor, MAP_SHARED);
  NODE_DEFINE_CONSTANT(ctor, MAP_PRIVATE);
  NODE_DEFINE_CONSTANT(ctor, MAP_NORESERVE);
  NODE_DEFINE_CONSTANT(ctor, MAP_FIXED);
  NODE_DEFINE_CONSTANT(ctor, MS_ASYNC);
  NODE_DEFINE_CONSTANT(ctor, MS_SYNC);
  NODE_DEFINE_CONSTANT(ctor, MS_INVALIDATE);
  ctor->Set(
    String::NewSymbol("PAGESIZE"), Integer::New(sysconf(_SC_PAGESIZE)),
    static_cast<PropertyAttribute>(ReadOnly | DontDelete)
  );
}


void init(Handle<Object> target) {
  MappedBuffer::Init(target);
}

NODE_MODULE(mappedbuffer, init);
