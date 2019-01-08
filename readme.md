### 工作原理
master进程解析本地配置文件，同时负责处理各种信号中断，worker进程提供HTTP服务，通过共享内存+信号量实现master和worker之间数据同步

### 特性
* pre-fork网络服务模型，根据cpu数量调控worker进程数量，支持开启端口复用(SO_REUSEPORT);
* 修改配置无需restart进程，支持reload操作实现动态更新，保证HTTP服务稳定性; 
* 通过restful api添加配置支持graceful添加，防止内存不足强制reload nginx导致服务异常;
* 简单配置可以通过模板配置添加，复杂配置通过操作服务器本地添加，两种方式添加的结果会自动同步数据;
* master机器提供配置更新状态，slave可以通过一个简单的脚本检更新同步配置，最多支持4096个slave；
* 不依赖数据库，二进制绿色无依赖，方便迁移部署;

### 部署

编译之前确保安装boost，开发基于boost1.6.8

```bash
git clone https://github.com/firebroo/nginx-confd.git
cd nginx-confd && make build && make intall
crontab -l | sed  "/confd/d" > /tmp/cron.rule
echo "*/1 * * * * cd /home/confd/ && ./confd -c config.json -s reload" >> /tmp/cron.rule
crontab /tmp/cron.rule
```

### 启动
master机器

必须指定config.json配置文件
```JavaScript
{
    "port": "8090",                                           #HTTP服务监听端口
    "addr": "0.0.0.0",                                        #HTTP服务监听地址
    "pid_path": "confd.pid",                                  #pid无需关心
    "log_path": "confd.log",                                  #日志文件
    "nginx_bin_path": "/usr/sbin/nginx",                      #nginx二进制路径
    "nginx_conf_path": "/home/work/nginx/conf/nginx.conf",    #nginx启动指定的配置文件
    "nginx_conf_writen_path": "/home/work/nginx/conf/vhost/", #HTTP接口添加配置落地目录
    "shm_size": "10m",                                        #共享内存大小，根据代理后端服务多少决定，理论10m足够
    "worker_process": 4,                                      #进程数量
    "verbosity": 1,                                           #日志等级
    "daemon": true                                            #后台进程模式
}
```
```bash
cd /home/confd && ./confd -c config.json && tail -f confd.log
```

slave机器

```bash
nohup /usr/bin/python slave_sync.py > /dev/null 2>&1 &
```

### 使用
```bash
启动:     ./confd -c config.json

重新加载: ./confd -c config.json -s reload

停止:     ./confd -c config.json -s stop
```

### HTTP服务restful接口
-----------------------------
地址: /api/confs

方法: POST

参数：domain(string), upstreams(dict), type(string), listen(string)

标准模块添加e.g.
```javascript
{
    "code" : 200,
    "info" : "add successful.",
    "query_url" : "127.0.0.1:8090/api/confs/test1.dev.firebroo.com:80"
}
```

-----------------------------
地址: /api/confs

方法: PUT

参数：domain(string), upstreams(dict), type(string), listen(string)

标准模块添加e.g.
```javascript
{
    "code" : 200,
    "info" : "add successful.",
    "query_url" : "127.0.0.1:8090/api/confs/test1.dev.firebroo.com:80"
}
```


-----------------------------

地址: /api/confs/{$domain}

方法: GET

参数：$domain

e.g.
```javascript
{
    "code" : 200,
    "data" : 
    {
        "listen" : "443",
        "server_name" : "test.firebroo.com",
        "upstream" : 
        [
            "1.1.1.1:80"
        ]
    }
}
```

-----------------------------

地址: /api/confs/{$domain}

方法: DELETE

参数：$domain

e.g.
```javascript
{
    "code" : 200,
    "info" : "delete successful."
}

```

-----------------------------

地址: /api/nginx_health

方法: GET

参数： NULL

e.g.
```javascript
{
    "code" : "200",
    "data" : 
    {
        "nginx process memsum" : "1200Mb", //nginx 总占用内存
        "nginx shutting worker" : "1"     //nginx shutting down残留进程占用内存
    },
    "error" : ""
}
```
