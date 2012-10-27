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
      after(function (done) {
        fs.closeSync(fd);
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
      after(function (done) {
        fs.closeSync(fd);
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

  describe('unmap', function () {
    var fd;
    var buffer;
    beforeEach(function (done) {
      fd = fs.openSync(__filename, 'r');
      var size = fs.fstatSync(fd).size;
      MappedBuffer(
        size, MappedBuffer.PROT_READ, MappedBuffer.MAP_SHARED, fd, function (err, buf) {
        if (err) { return done(err); }
        buffer = buf;
        done();
      });
    });
    afterEach(function (done) {
      fs.closeSync(fd);
      done();
    });

    it('should be succeed', function (done) {
      buffer.unmap().should.be.a.ok;
      buffer.length.should.eql(0);
      done();
    });

    describe('many call', function () {
      it('should be succeed', function (done) {
        buffer.unmap().should.be.true;
        buffer.length.should.eql(0);
        buffer.unmap().should.be.false;
        buffer.length.should.eql(0);
        buffer.unmap().should.be.false;
        buffer.length.should.eql(0);
        done();
      });
    });
  }); 

  describe('indexed mapping memory access', function () {
    var fd;
    var path;
    var buffer;
    before(function (done) {
      path = __dirname + '/indexed';
      fd = fs.openSync(path, 'a+');
      var buf = new Buffer(MappedBuffer.PAGESIZE);
      buf.fill(0xFF, 0, buf.length);
      fs.writeSync(fd, buf, 0, buf.length);
      MappedBuffer(
        buf.length, MappedBuffer.PROT_READ | MappedBuffer.PROT_WRITE, 
        MappedBuffer.MAP_SHARED, fd, function (err, buf) {
        if (err) { return done(err); }
        buffer = buf;
        done();
      });
    });
    after(function (done) {
      fs.closeSync(fd);
      fs.unlink(path);
      done();
    });
    it('should be accessed', function (done) {
      for (var i = 0; i < buffer.length; i++) {
        buffer[i].should.eql(0xFF);
        buffer[i] = 0x00;
        buffer[i].should.eql(0x00);
      }
      done();
    });
  });

  describe('shared memory', function () {
    var fd;
    var shared_path = __dirname + '/shared';
    var size = MappedBuffer.PAGESIZE;
    before(function (done) {
      fd = fs.openSync(shared_path, 'a+');
      var buf = new Buffer(size);
      buf.fill(0x00, 0, buf.length);
      fs.writeSync(fd, buf, 0, buf.length);
      done();
    });
    after(function (done) {
      fs.closeSync(fd);
      fs.unlink(shared_path);
      done();
    });
    it('should be read `0xFF` value', function (done) {
      var map = new MappedBuffer(
        size, MappedBuffer.PROT_READ | MappedBuffer.PROT_WRITE, 
        MappedBuffer.MAP_SHARED, fd
      );
      map[0] = 0xFF;
      var worker = fork(__dirname + '/worker.js');
      worker.on('message', function (msg) {
        if (!msg || !msg.cmd || msg.cmd !== 'get') {
          worker.kill();
          return done('an unexpected error');
        }
        msg.value.should.eql(0xFF);
        worker.kill();
        map.unmap();
        done();
      });
      worker.send({ cmd: 'get' });
    });
  });
});
