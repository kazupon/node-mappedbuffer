'use strict';

var should = require('should');
var assert = require('assert');
var format = require('util').format;
var fs = require('fs');
var fork = require('child_process').fork;
var MappedBuffer = require('../lib/mappedbuffer').MappedBuffer;


var itShouldBeDefined = function (obj, prop) {
  var msg = format('should be defined `%s`', prop);
  it(msg, function () {
    should.exist(obj[prop]);
  });
};


describe('MappedBuffer', function () {
  itShouldBeDefined(MappedBuffer, 'PROT_NONE');
  itShouldBeDefined(MappedBuffer, 'PROT_READ');
  itShouldBeDefined(MappedBuffer, 'PROT_WRITE');
  itShouldBeDefined(MappedBuffer, 'PROT_EXEC');
  itShouldBeDefined(MappedBuffer, 'MAP_SHARED');
  itShouldBeDefined(MappedBuffer, 'MAP_PRIVATE');
  itShouldBeDefined(MappedBuffer, 'MAP_NORESERVE');
  itShouldBeDefined(MappedBuffer, 'MAP_FIXED');
  itShouldBeDefined(MappedBuffer, 'PAGESIZE');

  describe('new', function () {
    describe('arguments', function () {
      var fd, size;
      before(function (done) {
        fd = fs.openSync(__filename, 'r');
        size = fs.fstatSync(fd).size;
        done();
      });

      // TODO: should be check argument pattern ...
      it('should be succeed with `all` arguments', function (done) {
        var buf = new MappedBuffer(
          size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd, 0
        );
        buf.should.be.an.instanceOf(MappedBuffer);
        buf.length.should.eql(size);
        done();
      });

      it('should be succeed with `3` arguments', function (done) {
        (function () {
          var buf = new MappedBuffer(
            size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED
          );
        }).should.throw(/Bad argument/);
        done();
      });

      it('should be succeed with `4` arguments', function (done) {
        var buf = new MappedBuffer(
          size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd
        );
        buf.should.be.an.instanceOf(MappedBuffer);
        buf.length.should.eql(size);
        done();
      });
    });
  });

  describe('MappedBuffer function call', function () {
    describe('arguments', function () {
      var fd, size;
      before(function (done) {
        fd = fs.openSync(__filename, 'r');
        size = fs.fstatSync(fd).size;
        done();
      });

      // TODO: should be check argument pattern ...
      it('should be succeed with `all` arguments', function (done) {
        var buf = MappedBuffer(
          size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd, 0
        );
        buf.should.be.an.instanceOf(MappedBuffer);
        buf.length.should.eql(size);
        done();
      });

      it('should be succeed with `3` arguments', function (done) {
        (function () {
          var buf = MappedBuffer(
            size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED
          );
        }).should.throw(/Bad argument/);
        done();
      });

      it('should be succeed with `4` arguments', function (done) {
        var buf = MappedBuffer(
          size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd
        );
        buf.should.be.an.instanceOf(MappedBuffer);
        buf.length.should.eql(size);
        done();
      });

      it('should be succeed with arguments and callback', function (done) {
        MappedBuffer(
          size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd, 0, 
          function (err, buf) {
          if (err) { return done(err); }
          buf.should.be.an.instanceOf(MappedBuffer);
          buf.length.should.eql(size);
          done();
        });
      });
    });
  });
});
