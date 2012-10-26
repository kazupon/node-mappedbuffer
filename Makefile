BUILDTYPE ?= Release

ifeq (${BUILDTYPE},Debug)
GYP_BUILD_TYPE = --d
else
GYP_BUILD_TYPE = --r
BUILDTYPE = Release
endif

TARGET = build/${BUILDTYPE}/mappedbuffer.node


distclean: clean
	rm -rf node_modules

node_modules: package.json
	npm install

test: node_modules
	./node_modules/.bin/mocha --reporter spec test/*.js

.PHONY: test
