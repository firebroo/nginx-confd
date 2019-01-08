#!/usr/bin/env python
# -*- coding: utf-8 -*-  
import sys
import time
import requests
import subprocess
import traceback

class slaveSync:
    def __init__(self, solt, logfile):
        self.solt = solt
        self.status_api = "http://127.0.0.1:8090/api/status/solt/%s" % (self.solt)
        self.update_status_api = "http://127.0.0.1:8090/api/status_update/solt/%s" % (self.solt)
        self.logfile = logfile
        self.log_cursor = open(self.logfile, "a+")

    def __del__(self):
        self.log_cursor.close()
         
    def current_time(self):
        return time.strftime('%Y-%m-%d %H:%M:%S ',time.localtime(time.time()))
    
    def force_sync(self):
        cmd = "rsync xxx"
        pipe = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        self.log_cursor.write(self.current_time() + pipe.stdout.read())
        return True
    
    def graceful_reload_nginx(self):
        cmd = '/home/work/nginx/sbin/nginx -t 2>&1 && /home/work/nginx/sbin/nginx -s reload 2>&1'
        pipe = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        self.log_cursor.write(self.current_time() + pipe.stdout.read())
        return True
    
    def get_status(self, status_api):
        res = requests.get(status_api).json()
        return res["status"]
        
    
    def update_status(self, update_status_api):
        res = requests.get(update_status_api)
        self.log_cursor.write(self.current_time())
        self.log_cursor.write("sync successful, result:\n%s\n" % (res.text))
     
     
    def loop_sync(self, timer):
        while True:
            self.log_cursor.write("%s slave %s sync.\n" % (self.current_time(), self.solt))
            try:
                if self.get_status(self.status_api):
                    self.force_sync() and self.graceful_reload_nginx() and self.update_status(self.update_status_api)
            except Exception, e:
                self.log_cursor.write(self.current_time() + traceback.format_exc())
            self.log_cursor.flush()
            time.sleep(timer)

if __name__ == "__main__":
    solt = 0
    timer = 10
    sync = slaveSync(solt, "slave_sync.log")
    sync.loop_sync(timer)
