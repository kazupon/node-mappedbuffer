/*
 * node-mappedbuffer -- Buffer to store the memory that is mapped by the `mmap`
 * Copyright (C) 2012 kazuya kawaguchi. All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
 */

#ifndef __MAPPED_BUFFER_H__
#define __MAPPED_BUFFER_H__

#include <node.h>

using namespace node;
using namespace v8;


class MappedBuffer : public ObjectWrap {
  public:
    static Persistent<Function> ctor;
    static void Init(Handle<Object> target);
    bool unmap();

  private:
    char *map_;
    size_t length_;
    bool released_;

    MappedBuffer(
      Handle<Object> wrapper, size_t size, int32_t protection, 
      int32_t flags, int32_t fd, off_t offset, char *data
    );
    ~MappedBuffer();

    static Handle<Value> New(const Arguments &args);
    static Handle<Value> Unmap(const Arguments &args);

    static void OnWork(uv_work_t *work_req);
    static void OnWorkDone(uv_work_t *work_req);
};


#endif /* __MAPPED_BUFFER_H__ */

