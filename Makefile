BUILDTYPE ?= Release

ifeq (${BUILDTYPE},Debug)
GYP_BUILD_TYPE = --d
else
GYP_BUILD_TYPE = --r
BUILDTYPE = Release
endif

TARGET = build/${BUILDTYPE}/mappedbuffer.node


all: $(TARGET)

$(TARGET): src/mappedbuffer.cc
	node-gyp rebuild ${GYP_BUILD_TYPE}

clean:
	node-gyp clean
	rm -rf build

distclean: clean
	rm -rf node_modules

node_modules: package.json
	npm install

test: node_modules $(TARGET)
	./node_modules/.bin/mocha --reporter spec test/*.js

.PHONY: test node_modules distclean clean all
