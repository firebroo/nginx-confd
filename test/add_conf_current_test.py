# -*- coding: utf-8 -*-  
import json
import time
import random
import string
import requests
import threading

def add_randomkey_conf():
    confd_api = "http://127.0.0.1:8090/api/confs"
    for i in xrange(2):
        post = {}
        random_str = ''.join(random.sample(string.ascii_letters + string.digits, 20)) 
        random_str = random_str + ".firebroo.com" 
        post["domain"] = random_str
        post["upstreams"] = [{"server": "2.2.2.2:3333"}, {"server": "3.3.3.3:3333"}]
        post["type"] = "websocket"
        post["listen"] = "80" 
        ret = requests.post(confd_api, data = json.dumps(post))
        data = ret.text
        json_data = json.loads(data)
        if (json_data["code"] != 200):
            print "add %s failed." % (random_str)
        


if __name__ == '__main__': 
    threads = []

    start = time.time()
    for i in range(100):
        t = threading.Thread(target=add_randomkey_conf)
        threads.append(t)
    for thread in threads:
        thread.start() 
    for thread in threads:
        thread.join()
    end = time.time()
    print "100 threads add 200 domain finish, time: %s" % (end - start)
