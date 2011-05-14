#!/usr/bin/python
import memcache
import time
import sys
from threadpool import *

def test(n):
    mc = memcache.Client(['127.0.0.1:'+sys.argv[2]], debug=0)
    for i in range(n):
        mc.set("foo","bar")
        value = mc.get("foo")
        if not value == "bar":
            print "error: ", value
            break;
    return True

if __name__=='__main__':
    pool = ThreadPool(int(sys.argv[1]))
    reqs = list()
    for i in range(int(sys.argv[1])):
        reqs.append(100)
    try:
        t1 = time.time()
        requests = makeRequests(test, reqs)
        [pool.putRequest(req) for req in requests]
        pool.wait()
        t2 = time.time()
        print t2 - t1
    except:
        pass
