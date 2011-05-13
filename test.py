#!/usr/bin/python
import memcache
import datetime
import sys

if __name__=='__main__':
    mc = memcache.Client(['127.0.0.1:'+sys.argv[1]], debug=0)
    print datetime.datetime.now()
    for i in range(10000):
        mc.set("foo","bar")
        value = mc.get("foo")
        if not value == "bar":
            print "error"
            break;
    print datetime.datetime.now()
