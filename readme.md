### 工作原理
master进程解析本地配置文件，同时负责处理各种信号中断，worker进程提供HTTP服务，通过共享内存+信号量实现master和worker之间数据同步

### 特性
* pre-fork网络服务模型，根据cpu数量调控worker进程数量，支持开启端口复用(SO_REUSEPORT);
* 修改配置无需restart进程，支持reload操作实现动态更新，保证HTTP服务稳定性; 
* 简单配置可以通过模板配置添加，复杂配置通过操作服务器本地添加，两种方式添加的结果会自动同步数据;
* 不依赖数据库，二进制绿色无依赖，方便迁移部署;

### 部署
必须指定config.json配置文件
```JavaScript
{
    "port": 8090,
    "addr": "127.0.0.1",
    "pid_path": "confd.pid",
    "log_path": "confd.log",
    "nginx_bin_path": "/usr/sbin/nginx",
    "nginx_conf_path": "/etc/nginx/nginx.conf",
    "nginx_conf_writen_path": "/etc/nginx/vhost/",
    "shm_size": "10m",
    "worker_process": 16,
    "verbosity": 1,
    "daemon": true
}

```

### 使用
启动:     ./confd -c config.json

重新加载: ./confd -c config.json -s reload

停止:     ./confd -c config.json -s stop


### HTTP服务接口
-----------------------------
地址: /api/confs

方法: POST

参数：domain(string), upstreams(dict), type(string), listen(string)

标准模块添加e.g.
```bash
curl -XPOST 127.0.0.1:8090/api/confs --data '{"domain": "test.dev.firebroo.com", "upstreams": [{"server": "1.1.1.1:80"}], "type": "standard", "listen": "80"}'
```

-----------------------------

地址: /api/confs/{$ip}

方法: GET

参数：$ip

-----------------------------

地址: /api/confs/{$domain}

方法: GET

参数：$domain

### 测试
在已有3000+域名得基础，增加3000域名，100线程消耗时间6分钟左右，多次测试没有添加失败情况，无并发问题，主要瓶颈在nginx -t这一步骤，nginx需要解析加载配置文件去测试语法，

```py
python test/add_conf_current_test.py
100 threads add 3000 domain finish, time: 365.082176924
```
