# DeepSpec Web Server
[![Build Status](https://travis-ci.org/liyishuai/myhttpd.svg?branch=master)](https://travis-ci.org/liyishuai/myhttpd)

How to Use
-------------

1. `cmake . && make`.
2. Put the static website you want to host under `web` directory,
   under `bin/`.
3. Run `bin/server` with a port number as its argument (for example,
   `server 8888`.
4. Run `bin/daemon` and enter the Process ID of `server` when prompted.
5. Enter the Process ID of `daemon` when `server` promps it.
