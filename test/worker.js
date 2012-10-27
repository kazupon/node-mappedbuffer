var MappedBuffer = require('../lib/mappedbuffer').MappedBuffer;
var fs = require('fs');

var shared_path = __dirname + '/shared';
var size = MappedBuffer.PAGESIZE;
var fd = fs.openSync(shared_path, 'a+');
var map = MappedBuffer(size, MappedBuffer.PROT_READ | MappedBuffer.PROT_WRITE, MappedBuffer.MAP_SHARED, fd);

process.on('message', function (msg, srv) {
  if (!msg || !msg.cmd) { return; }

  switch (msg.cmd) {
    case 'get':
      process.send({
        cmd: 'get',
        value: map[0]
      });
      break;
  }
});

process.on('exit', function () {
  map.unmap();
  fs.closeSync(fd);
});
