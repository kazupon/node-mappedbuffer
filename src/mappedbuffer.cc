/*
 * mappedbuffer main
 * Copyright (C) 2012 kazuya kawaguchi. See Copyright Notice in mappedbuffer.h
 */

#define BUILDING_NODE_EXTENSION

#include "mappedbuffer.h"
#include "debug.h"
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>


typedef struct mmap_req_t {
  size_t size;
  int32_t protection;
  int32_t flags;
  int32_t fd;
  off_t offset;
  char *map;
  Persistent<Object> obj;
  Persistent<Function> cb;
};


static Persistent<String> length_symbol;
Persistent<Function> MappedBuffer::ctor;


MappedBuffer::MappedBuffer(
  Handle<Object> wrapper, size_t size, int32_t protection, 
  int32_t flags, int32_t fd, off_t offset, char *data)
  : map_(NULL), length_(0), released_(false), ObjectWrap() {
  TRACE(
    "constructor: size=%ld, protection=%d, flags=%d, fd=%d, offset=%lld, data=%p\n",
    size, protection, flags, fd, offset, data
  );
  Wrap(wrapper);

  map_ = (data == NULL ? (char *)mmap(NULL, size, protection, flags, fd, offset) : data);
  TRACE("map_=%p\n", map_);
  length_ = size;

  if (map_ == MAP_FAILED || map_ == NULL) {
    length_ = 0;
  }

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
  mmap_req_t *req = NULL;
  off_t offset = 0;

  if (args.Length() == 5) {
    if (args[4]->IsFunction()) {
      req = reinterpret_cast<mmap_req_t *>(malloc(sizeof(mmap_req_t)));
      req->size = size;
      req->protection = protection;
      req->flags = flags;
      req->fd = fd;
      req->offset = offset;
      req->map = NULL;
      req->obj = Persistent<Object>::New(Handle<Object>::Cast(args.This()));
      req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[4]));
    } else {
      offset = args[4]->ToInteger()->Value();
    }
  } else if (args.Length() == 6) {
    req = reinterpret_cast<mmap_req_t *>(malloc(sizeof(mmap_req_t)));
    req->size = size;
    req->protection = protection;
    req->flags = flags;
    req->fd = fd;
    req->offset = args[4]->ToInteger()->Value();
    req->map = NULL;
    req->obj = Persistent<Object>::New(Handle<Object>::Cast(args.This()));
    req->cb = Persistent<Function>::New(Handle<Function>::Cast(args[5]));
  }

  if (req == NULL) { // sync
    MappedBuffer *obj = new MappedBuffer(
      args.This(), size, protection, flags, fd, offset, NULL
    );
    return args.This();
  } else { // async
    uv_work_t *uv_req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
    assert(uv_req != NULL);
    uv_req->data = req;

    int32_t ret = uv_queue_work(uv_default_loop(), uv_req, OnWork, OnWorkDone);
    TRACE("uv_queue_work: ret=%d\n", ret);
    assert(ret == 0);
    return scope.Close(args.This());
  }
}

Handle<Value> MappedBuffer::Unmap(const Arguments &args) {
  HandleScope scope;
  TRACE("Unmap\n");

  MappedBuffer *buf = ObjectWrap::Unwrap<MappedBuffer>(args.This());
  assert(buf != NULL);

  return scope.Close(Boolean::New(buf->unmap()));
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

  MappedBuffer *buf = ObjectWrap::Unwrap<MappedBuffer>(args.This());
  assert(buf != NULL);

  memset((void *)(buf->map_ + start), value, end - start);

  return scope.Close(args.This());
}

void MappedBuffer::OnWork(uv_work_t *work_req) {
  TRACE("work_req=%p\n", work_req);

  mmap_req_t *req = reinterpret_cast<mmap_req_t *>(work_req->data);
  assert(req != NULL);

  req->map = (char *)mmap(NULL, req->size, req->protection, req->flags, req->fd, req->offset);
  TRACE("mmap: map =%p\n", req->map);
}

void MappedBuffer::OnWorkDone(uv_work_t *work_req) {
  HandleScope scope;
  TRACE("work_req=%p\n", work_req);

  mmap_req_t *req = static_cast<mmap_req_t *>(work_req->data);
  assert(req != NULL);

  // init callback arguments.
  int32_t argc = 0;
  Local<Value> argv[2] = { 
    Local<Value>::New(Null()),
    Local<Value>::New(Null()),
  };

  // set error to callback arguments.
  if (req->map == MAP_FAILED) {
    const char *name = "mmap error";
    Local<String> message = String::NewSymbol(name);
    Local<Value> err = Exception::Error(message);
    argv[argc] = err;
  }
  argc++;

  MappedBuffer *mappedbuffer = new MappedBuffer(
    req->obj, req->size, req->protection, req->flags, req->fd, req->offset, req->map
  );
  Local<Value> buf = Local<Value>::New(mappedbuffer->handle_);
  argv[argc++] = buf;

  // execute callback
  if (!req->cb.IsEmpty()) {
    TryCatch try_catch;
    MakeCallback(Context::GetCurrent()->Global(), req->cb, argc, argv);
    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
  } 

  // releases
  req->cb.Dispose();
  req->obj.Dispose();
  req->map = NULL;
  free(req);
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
  ctor->Set(
    String::NewSymbol("PAGESIZE"), Integer::New(sysconf(_SC_PAGESIZE)),
    static_cast<PropertyAttribute>(ReadOnly | DontDelete)
  );
}


void init(Handle<Object> target) {
  MappedBuffer::Init(target);
}

NODE_MODULE(mappedbuffer, init);
