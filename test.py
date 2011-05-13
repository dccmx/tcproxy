#!/usr/bin/python
import memcache
import datetime
import time
import sys

if __name__=='__main__':
    mc = memcache.Client(['127.0.0.1:'+sys.argv[1]], debug=0)
    t1 = time.time()
    for i in range(10000):
        mc.set("foo","bar")
        value = mc.get("foo")
        if not value == "bar":
            print "error"
            break;
    t2 = time.time()
    print t2 - t1
