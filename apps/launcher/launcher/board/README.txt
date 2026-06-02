板端开机自启动：网络 + 校时
============================

与电脑网线直连时：电脑网口 192.168.10.1，板子 192.168.10.2。
先起网（S50），再 NTP 校时（S99）。

文件说明
--------
  S50network   - eth0 up、静态 IP、默认网关、/etc/resolv.conf
  S99timesync  - 时间不对时 busybox ntpd + hwclock -w

一、复制到板子

  S50network  -> /etc/init.d/S50network
  S98launcher -> /etc/init.d/S98launcher
  S99timesync -> /etc/init.d/S99timesync

  （U 盘 / scp / adb push，路径按你环境改）

二、在板子串口执行（只需做一次）

  chmod 755 /etc/init.d/S50network
  chmod 755 /etc/init.d/S98launcher
  chmod 755 /etc/init.d/S99timesync

  /etc/init.d/S50network start
  /etc/init.d/S98launcher start
  /etc/init.d/S99timesync start

三、验证

  ip link show eth0
  # 期望含 UP

  ifconfig eth0
  # 期望 192.168.10.2

  ping -c 2 192.168.10.1
  ping -c 2 www.baidu.com

  date

  reboot
  # 重启后重复上面检查，无需再手工 ip/ifconfig

四、改 IP 时

  编辑 /etc/init.d/S50network 顶部变量：
  BOARD_IP、GATEWAY、DNS_PC 等，保存后：
  /etc/init.d/S50network restart

五、时区（可选）

  echo 'export TZ=CST-8' >> /etc/profile

六、Buildroot 永久集成

  将 S50network、S99timesync 放入：
  board/xxx/rootfs_overlay/etc/init.d/
  重新打包 rootfs 烧录。

七、启动 launcher

  网络/时间就绪后：
  /path/to/run_launcher.sh

  开机自启动：
  1. 将 launcher、run_launcher.sh 等运行文件放到 /root
     目录里需要有 launcher、run_launcher.sh、plugins、lib 等运行文件
  2. 复制 S98launcher 到 /etc/init.d/S98launcher
  3. chmod 755 /etc/init.d/S98launcher
  4. /etc/init.d/S98launcher start
  5. reboot 验证

  如果程序目录不是 /root，修改 S98launcher 顶部 APP_DIR。
  日志位置：/var/log/launcher.log
