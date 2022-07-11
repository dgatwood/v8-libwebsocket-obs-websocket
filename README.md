v8-libwebsocket-obs-websocket
=============================

While trying to add some OBS-websocket features to an existing tool
written in C, I discovered that there was no C or C++ API for the
OBS-websocket protocol.

So, I thought, I'll just integrate node.js.  That's when I discovered
that node.js doesn't include shared libraries, is built as a static
library that's a quarter *gigabyte*, and doesn't provide any library
binaries at all as part of the binary distribution.  Oh, and it takes
an hour to build on a decked out M1 Max MacBook Pro.  And I immediately
ran into linker errors trying to follow their embedding docs.

Needless to say, increasing the build time of my tool from two seconds
to an hour didn't sit well with me even if I could manage to fix the
linker errors, so I set out to find another way.

My first thought was integrating V8 by itself.  Unfortunately, I
quickly discovered that V8 doesn't provide a WebSocket library.

So, I thought, "Why not try hacking in the one from Node.js.  After
all, it is written in JavaScript, so it should be easy, right?"  And
then I discovered just how many dozens of classes would be indirectly
pulled in and how many missing C++-side Node features I would have to
graft in, and I concluded that I'd be better off starting over.

The result of that week of effort is v8-libwebsocket-obs-websocket.

This is *not* a polished implementation.  It works fine for a single
socket at a time, for text-only communication.  The known issues are:

1.  No support for Blob types at all.
2.  No support for detecting whether the returned data should be a
    string or a binary type.
3.  Support for ArrayBuffer is untested and disabled with an #ifdef
    because it would break string results until someone implements
    the necessary code for #2.
4.  Performance issues with multiple sockets.

Unfortunately, because we need to negotiate protocols on a per-socket
basis, we can't share the context across multiple sockets.  Because
libwebsocket ignores the timeout when waiting for events, and (as
far as I could tell) provides no mechanism for waiting for events on
multiple contexts at once, performance will probably tank if you open
more than one socket.  

Ideally, this should be reworked to run each libwebsocket context
on a separate thread, which will probably involve adding a little
bit of missing glue between the threads.  I originally wrote it
thinking that they would end up on separate threads from the V8
thread, but ended up not needing to do so, so most of that is
theoretically done, but not tested.

Anyway, I hope this helps somebody else out who might be trying to
figure out how to integrate websockets into V8 and/or integrate
OBS-websocket support into a C or C++ app.
