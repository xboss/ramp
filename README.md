# Ramp
Ramp is an encrypted VPN of L2 network, just like ZeroTier.

## 状态
主分支是经过测试且可用的状态

## 特性
* 基于UDP的加密传输二层网络的数据，加密后的传输数据没有任何特征，可以不加密，性能更佳
* 可用来内网穿透或其它，类似ZeroTier

## 安装
运行环境：Linux

下载源码并解压后:
```
cd ramp
mkdir build
make
```

## 环境配置
### client
* 开启ip转发，将以下配置添加到 "/etc/sysctl.conf" 文件，执行“sysctl -p”生效。
```
net.ipv4.ip_forward=1
net.ipv4.conf.all.route_localnet = 1
net.ipv4.conf.default.route_localnet = 1
net.ipv4.conf.[网卡接口].route_localnet = 1
net.ipv4.conf.lo.route_localnet = 1
net.ipv4.conf.[虚拟网卡接口].route_localnet = 1
```

* 当启动client后，设置tap
  * 设置ip "ifconfig tap0 [tap的ip]/24 netmask 255.255.255.0"
  * 设置mtu，mtu必须小于等于1472， "ifconfig tap0 mtu 1400"

* 如果当作“你懂的” VPN tunnel 使用
  * 修改默认转发策略 "iptables -P FORWARD ACCEPT"
  * 修改nat的源地址改成出口网卡的地址 “iptables -t nat -A POSTROUTING -s [tap的ip]/24 -o enp1s0 -j MASQUERADE”
  * 在出口机器上设置arp代理 "echo 1 >  /proc/sys/net/ipv4/conf/tap0/proxy_arp"
  * 进行必要的 ip route 设置

## 使用
```
ramp <configfile>
```
* configfile是配置文件
* 需要root权限运行

## 配置文件：
参考“[ramp.conf](https://github.com/xboss/ramp/blob/main/ramp.conf)”，内有注释。

## 拓扑图

```
                         +------------------+
                         |                  |
                         |                  |
                         |    ramp sever    |
                         |                  |
                         |  10.10.1.2       |
                         +--------+---------+
                                  |
                     xxxx  xxxxxxx|xxxxxxxxxx
                   xxx        x              xxxxx
                  xx                              xx
                 xx      Internet                  xx
                 xx                                 x
                  xx                               xx
        +------------xxxxxx                     xxx-----------------+
        |                  xxxxxxxxxxxxxxxxxxxxxx                   |
        |                             |                             |
        |                             |                             |
        |                             |                             |
        |                             |                             |
        |                             |                             |
        |                             |                             |
+-------+----------+         +--------+---------+         +---------+--------+
|                  |         |                  |         |                  |
|                  |         |                  |         |                  |
|    ramp client   |         |    ramp client   |         |    ramp client   |  x  x  x   x  x
|                  |         |                  |         |                  |
|tap0:192.168.2.1  |         |tap0:192.168.2.2  |         |tap0:192.168.2.3  |
+------------------+         +------------------+         +------------------+

```