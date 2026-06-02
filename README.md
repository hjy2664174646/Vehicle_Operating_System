# Vehicle Operating System

基于正点原子 i.MX6ULL 开发板和 Qt Widgets 实现的车载系统演示工程。系统采用单个 Qt 主程序承载多个功能页面，包含音乐播放、视频播放、灯光控制、传感器显示、地图导航、天气预报、USB 相机和开机动画等功能。

## 1. 整体架构

工程主程序位于：

```text
apps/launcher/launcher/
```

整体采用“主界面 + 多页面模块”的结构：

```text
Vehicle_Operating_System/
  README.md
  apps/
    launcher/
      launcher/        Qt 主工程源码
      video/           测试视频和开机视频素材
```

Qt 主程序使用 `QStackedWidget` 管理不同页面，每个功能模块对应一个独立的 QWidget 页面类。主界面负责应用入口展示、页面切换、打开/关闭动画和开机动画调度。

核心文件：

```text
main.cpp                  程序入口
mainwindow.h/.cpp         主界面、页面切换、应用打开关闭动画
bootanimationpage.h/.cpp  开机动画页面
musicpage.h/.cpp          音乐页面
videopage.h/.cpp          视频页面
videoplayer.h/.cpp        GStreamer 视频播放封装
camerapage.h/.cpp         USB 相机预览、拍照、录像
mappage.h/.cpp            地图、搜索、路线规划
weatherpage.h/.cpp        天气预报页面
ap3216c.h/.cpp            AP3216C 传感器读取
calculator.h/.cpp         计算器页面
pic.qrc                   图片资源文件
run_launcher.sh           开发板运行脚本
board/                    开发板自启动和网络脚本
```

部署到开发板后，推荐目录结构：

```text
/root/launcher
/root/run_launcher.sh
/root/fonts/msyh.ttc
/root/videos/load.mp4
/etc/init.d/S50network
/etc/init.d/S98launcher
```

## 2. 各个模块主要实现方法

### 主界面与页面动画

主界面由 `MainWindow` 管理，使用图标入口进入不同应用页面。页面打开和关闭时使用截图覆盖层配合缩放、透明度动画，实现类似手机应用弹出和收回的效果。

### 开机动画

开机动画由 `BootAnimationPage` 实现。程序启动后优先播放 `/root/videos/load.mp4`，视频播放使用工程内已有的 `VideoPlayer` / GStreamer appsink 路径。为了适配 i.MX6ULL，开机视频建议转换为：

```text
H.264 Baseline/Main
800x480
25 fps
yuv420p
无音频或低码率音频
```

页面底部实现了冰蓝和白色风格的进度条，并使用 `picture/cat.png` 作为装饰元素。进度条在视频首帧出现后同步显示，随开机动画结束一起消失。

### 音乐播放

音乐模块负责本地音乐列表、播放、暂停、上一首、下一首、进度显示和歌词显示。音频播放依赖开发板系统的音频输出链路，运行前需要确认 ALSA 声卡、音量和音频文件路径正常。

### 视频播放

视频模块使用 GStreamer appsink 方式解码视频帧，再交给 Qt 页面显示。该方式比直接依赖 Qt Multimedia 更容易在当前开发板环境中控制兼容性。

推荐视频格式：

```text
H.264 Baseline/Main
分辨率不高于 800x480
帧率 25 fps 左右
像素格式 yuv420p
```

如果视频播放黑屏，优先检查视频编码格式和开发板 GStreamer 解码插件。

### 灯光控制

灯光模块当前使用开发板 sysfs LED 接口控制开关：

```text
/sys/class/leds/sys-led/trigger
/sys/class/leds/sys-led/brightness
```

实测当前板载 LED 虽然 `max_brightness` 为 255，但写入不同亮度值后实际亮度不变，因此界面保留为开关控制。如果需要真正 PWM 调光，需要硬件 LED 接到 PWM 输出，并在设备树中改为 `pwm-leds` 或自行通过 PWM sysfs 控制。

### 传感器中心

传感器模块基于 AP3216C 三合一传感器，读取环境光、接近距离和红外数据。界面通过定时器周期刷新数据，并使用图形化控件展示当前状态。

### USB 相机

相机模块使用 V4L2 直接访问 USB 摄像头，不依赖 OpenCV。主要功能包括：

```text
设备枚举
YUYV/MJPEG 格式采集
实时预览
拍照保存 PNG/JPEG
AVI 录像保存
```

开发板需要加载 UVC 驱动：

```sh
modprobe uvcvideo
```

常见设备节点为 `/dev/video0` 或 `/dev/video1`，实际使用时以界面枚举结果为准。

### 地图导航

地图模块使用高德地图 HTTP API 和瓦片地图方案，不依赖 WebEngine。主要功能包括：

```text
地图瓦片加载
手指拖动地图改变中心坐标
起始地址和目标地址输入
地址联想
路线规划
路线自动适配缩放
```

默认位置设置为河海大学常州新校区附近。地图和路线接口需要开发板能联网，并且高德 API Key 可用。

### 天气预报

天气模块使用高德 IP 定位和天气接口。默认城市为常州金坛区，支持切换城市、摄氏度/华氏度切换、未来天气卡片、温湿度曲线和时间轴滑动。

天气图标放在：

```text
apps/launcher/launcher/picture/
```

并通过 `pic.qrc` 加入 Qt 资源。当前使用的天气图标命名包括：

```text
weather_sunny.png
weather_cloudy.png
weather_overcast.png
weather_rain.png
weather_heavy_rain.png
weather_snow.png
weather_thunder.png
weather_fog.png
```

## 3. 开发板外接模块选型的注意事项

### USB 摄像头

建议选择 UVC 免驱 USB 摄像头。购买或更换前重点确认：

```text
支持 Linux UVC
支持 YUYV 或 MJPEG
分辨率不要过高，推荐 640x480 或 800x600
帧率 15-30 fps
开发板 USB 供电足够
```

如果摄像头接入后没有 `/dev/video*`，优先检查 `uvcvideo.ko` 是否存在并执行 `modprobe uvcvideo`。

### LED / 灯光模块

普通 GPIO LED 只能稳定做开关控制，不一定支持亮度调节。若要实现亮度调节，需要选择可 PWM 调光的 LED 模块，并确认：

```text
LED 正极/负极接法匹配开发板电平
PWM 引脚没有被其他外设占用
设备树中对应 PWM 节点已启用
内核启用了 PWM 或 pwm-leds 支持
```

### AP3216C 传感器

AP3216C 使用 I2C 通信。更换传感器或模块时需要确认：

```text
I2C 总线编号
设备地址
供电电压
中断脚是否需要使用
当前内核是否已有对应驱动或是否采用用户态 I2C 读取
```

### 网络

地图和天气都依赖网络。当前开发板常用静态 IP 配置为：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0
route add default gw 192.168.10.1
echo "nameserver 192.168.10.1" > /etc/resolv.conf
echo "nameserver 114.114.114.114" >> /etc/resolv.conf
```

如果电脑和开发板直连，需要电脑网口设置为同网段地址，例如 `192.168.10.1`。

### 屏幕与触摸

本工程按 800x480 触摸屏设计。更换屏幕后需要关注：

```text
实际分辨率
linuxfb 显示设备
触摸设备节点
Qt evdev 输入参数
字体大小和页面布局
```

## 4. 使用不同外接模块需要修改的地方

### 更换 USB 摄像头

需要重点检查：

```sh
ls /dev/video*
v4l2-ctl --list-devices
v4l2-ctl --list-formats-ext -d /dev/video0
```

如果设备节点变成 `/dev/video1` 或其他编号，程序界面通常可以自动枚举；如果格式不支持，需要在 `camerapage.cpp` 中调整采集格式、分辨率或帧率。

### 更换灯光模块

如果仍然是 GPIO LED，只需要修改 sysfs 路径：

```text
/sys/class/leds/xxx/brightness
/sys/class/leds/xxx/trigger
```

如果换成 PWM 调光模块，需要新增 PWM 控制逻辑，或修改设备树将 LED 绑定为 `pwm-leds`，再由程序写入新的亮度接口。

### 更换传感器

如果不是 AP3216C，需要修改：

```text
ap3216c.h/.cpp
传感器初始化流程
数据读取寄存器
单位换算
界面显示字段
```

不同 I2C 设备还需要确认 `/dev/i2c-*` 节点和设备地址。

### 更换地图或天气 API

当前地图和天气使用高德接口。如果换成其他服务，需要修改：

```text
mappage.cpp      地址搜索、路线规划、瓦片 URL、坐标系处理
weatherpage.cpp  城市定位、天气 JSON 解析、天气图标映射
```

高德地图使用 GCJ-02 坐标系。如果换成 OpenStreetMap 或其他 WGS-84 坐标服务，需要特别注意坐标偏移问题。

### 更换屏幕分辨率

如果不是 800x480，需要检查：

```text
mainwindow.cpp
mappage.cpp
weatherpage.cpp
bootanimationpage.cpp
各页面固定宽高、字体大小、图片缩放逻辑
```

建议优先将固定尺寸改为基于 `width()`、`height()` 的比例布局。

## 5. 源码编译相关

### 开发环境

本工程面向 Qt 5.12.9 和 i.MX6ULL 交叉编译环境。桌面端可用于语法检查和界面预览，最终运行环境为 ARM Linux 开发板。

主工程文件：

```text
apps/launcher/launcher/launcher.pro
```

### 桌面端检查编译

在 Windows Qt MinGW 环境下可执行：

```powershell
cd C:\Users\meizihuang123\Desktop\03\Vehicle_Operating_System\apps\launcher
mkdir build-desktop-check
cd build-desktop-check
D:\Qt\5.12.9\mingw73_32\bin\qmake.exe ..\launcher\launcher.pro
D:\Qt\Tools\mingw730_32\bin\mingw32-make.exe
```

桌面端主要用于检查 C++ 和 Qt 代码是否能正常编译，不代表所有硬件功能都能在电脑上运行。

### 开发板交叉编译

在 Ubuntu 虚拟机中使用 ARM Qt 的 qmake 编译，示例：

```sh
cd ~/build-launcher-Arm_Qt5_12_9-Debug
/path/to/arm-qt/bin/qmake /path/to/Vehicle_Operating_System/apps/launcher/launcher/launcher.pro
make
```

实际 qmake 路径以自己的 Qt 交叉编译环境为准。

### 运行脚本

开发板运行脚本为：

```text
apps/launcher/launcher/run_launcher.sh
```

部署后建议放到：

```text
/root/run_launcher.sh
```

并赋予执行权限：

```sh
chmod 755 /root/run_launcher.sh
```

脚本中需要设置 Qt 运行环境，例如：

```sh
QT_QPA_PLATFORM=linuxfb
QT_QPA_FONTDIR=/root/fonts
LD_LIBRARY_PATH=/lib:/usr/lib
```

为了正常显示中文，建议将微软雅黑或其他中文字体放到：

```text
/root/fonts/msyh.ttc
```

### 开机自启动

工程提供开发板脚本：

```text
apps/launcher/launcher/board/S50network
apps/launcher/launcher/board/S98launcher
apps/launcher/launcher/board/S99timesync
```

复制到开发板：

```sh
cp S50network /etc/init.d/S50network
cp S98launcher /etc/init.d/S98launcher
chmod 755 /etc/init.d/S50network
chmod 755 /etc/init.d/S98launcher
```

如果当前系统的 `/etc/init.d/rcS` 不会自动扫描 `/etc/init.d/Sxx` 脚本，需要在 `rcS` 中手动加入：

```sh
[ -x /etc/init.d/S50network ] && /etc/init.d/S50network start
[ -x /etc/init.d/S98launcher ] && /etc/init.d/S98launcher start
```

### 常用排查命令

```sh
ps | grep launcher
cat /var/log/launcher.log
ls /dev/video*
cat /sys/class/leds/sys-led/brightness
ifconfig eth0
route -n
cat /etc/resolv.conf
```

如果 Qt 程序开机后文字不显示，优先检查字体路径：

```sh
ls -l /root/fonts
grep QT_QPA_FONTDIR /root/run_launcher.sh
cat /var/log/launcher.log
```
