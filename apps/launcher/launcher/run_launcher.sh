#!/bin/sh
# 板端启动 launcher：自动绑定 gpio_keys（KEY0），避免走 TTY 打出 ^[[26~

# 中国时区（无 /usr/share/zoneinfo 时用 POSIX TZ 写法）
export TZ=${TZ:-CST-8}

# 能 ping 通不等于系统时钟已校时；未同步时 Qt 会显示 1970 年
sync_time_if_needed() {
    year=$(date +%Y 2>/dev/null) || year=1970
    [ "$year" -ge 2020 ] 2>/dev/null && return 0

    echo "warn: 系统时间为 ${year} 年，尝试 NTP 校时..." >&2
    if command -v ntpdate >/dev/null 2>&1; then
        ntpdate -u -t 10 cn.pool.ntp.org 2>/dev/null \
            || ntpdate -u -t 10 pool.ntp.org 2>/dev/null \
            || ntpdate -u -t 10 ntp.aliyun.com 2>/dev/null
    elif command -v busybox >/dev/null 2>&1 && busybox ntpd -h 2>&1 | grep -q ntpd; then
        busybox ntpd -q -p cn.pool.ntp.org 2>/dev/null \
            || busybox ntpd -q -p pool.ntp.org 2>/dev/null
    else
        echo "warn: 未找到 ntpdate/busybox ntpd，请手动 date -s 或配置开机校时" >&2
        return 1
    fi

    if command -v hwclock >/dev/null 2>&1; then
        hwclock -w 2>/dev/null
    fi
    date >&2
}

sync_time_if_needed

# Load USB camera driver before launcher scans /dev/video*.
if command -v modprobe >/dev/null 2>&1; then
    modprobe uvcvideo 2>/dev/null || true
fi

# 固定 800x480 + 32 位色，避免亮蓝底、控件被裁切
export QT_QPA_PLATFORM=linuxfb:size=800x480
export QT_QPA_LINUXFB_DEPTH=32
APP_DIR=$(cd "$(dirname "$0")" && pwd)
export HOME=${HOME:-/root}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/runtime-root}
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true
export LD_LIBRARY_PATH="$APP_DIR:$APP_DIR/lib:/lib:/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$APP_DIR/plugins:/plugins:${QT_PLUGIN_PATH:-}"
export QML2_IMPORT_PATH="$APP_DIR/qml:${QML2_IMPORT_PATH:-}"
for FONT_DIR in "$APP_DIR/fonts" /usr/lib/fonts /usr/share/fonts /lib/fonts; do
    if [ -d "$FONT_DIR" ] && ls "$FONT_DIR"/*.ttf "$FONT_DIR"/*.ttc >/dev/null 2>&1; then
        export QT_QPA_FONTDIR="$FONT_DIR"
        echo "qt font dir: $QT_QPA_FONTDIR"
        break
    fi
done
export GST_PLUGIN_PATH="/usr/lib/gstreamer-1.0:$APP_DIR/gstreamer-1.0:$APP_DIR/lib/gstreamer-1.0:${GST_PLUGIN_PATH:-}"
export GST_PLUGIN_SYSTEM_PATH="/usr/lib/gstreamer-1.0:$APP_DIR/gstreamer-1.0:$APP_DIR/lib/gstreamer-1.0:${GST_PLUGIN_SYSTEM_PATH:-}"
export GST_REGISTRY="/tmp/gst-registry.bin"
export GST_REGISTRY_REUSE_PLUGIN_SCANNER=0

# 触摸屏场景隐藏鼠标箭头（linuxfb 检测到输入设备时会默认显示）
export QT_QPA_FB_HIDECURSOR=1

GPIO_EVENT=""
for e in /sys/class/input/event*; do
    name=$(cat "$e/device/name" 2>/dev/null) || continue
    case $name in
        *gpio_keys*)
            GPIO_EVENT="/dev/input/${e##*/}"
            break
            ;;
    esac
done

if [ -n "$GPIO_EVENT" ]; then
    export QT_QPA_EVDEV_KEYBOARD_PARAMETERS="$GPIO_EVENT"
    echo "evdev keyboard: $GPIO_EVENT"
else
    echo "warn: gpio_keys not found, KEY0 may not work" >&2
fi

cd "$APP_DIR" || exit 1
killall -q mplayer 2>/dev/null
exec ./launcher "$@"
