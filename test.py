#!/usr/bin/python
import memcache

if __name__=='__main__':
    mc = memcache.Client(['127.0.0.1:11211'], debug=0)
    mc.set("foo","bar")
    value = mc.get("foo")
    print value
