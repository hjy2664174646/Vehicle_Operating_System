#include "mainwindow.h"

#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QFile>
#include <QTextCodec>
#include <QTimer>

#if defined(__arm__)
// 板子 KEY0 在 gpio_keys@0；勿用 snvs-powerkey / 触摸屏
static QString readInputDeviceName(int eventIndex)
{
    const QString namePath =
        QStringLiteral("/sys/class/input/event%1/device/name").arg(eventIndex);
    QFile nameFile(namePath);
    if (!nameFile.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(nameFile.readAll()).trimmed();
}

static void setupLinuxFbDisplay()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_LINUXFB_DEPTH"))
        qputenv("QT_QPA_LINUXFB_DEPTH", "32");

    // 触摸屏/无鼠标场景：隐藏 linuxfb 默认箭头光标
    if (qEnvironmentVariableIsEmpty("QT_QPA_FB_HIDECURSOR"))
        qputenv("QT_QPA_FB_HIDECURSOR", "1");

    const QByteArray platform = qgetenv("QT_QPA_PLATFORM");
    if (platform.isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "linuxfb:size=800x480");
        return;
    }
    if (!platform.contains("size="))
        qputenv("QT_QPA_PLATFORM", platform + ":size=800x480");
}

static void setupEvdevKeyboard()
{
    if (!qEnvironmentVariableIsEmpty("QT_QPA_EVDEV_KEYBOARD_PARAMETERS"))
        return;

    QString gpioKeysPath;

    for (int i = 0; i < 16; ++i) {
        const QString eventPath = QStringLiteral("/dev/input/event%1").arg(i);
        if (!QFile::exists(eventPath))
            continue;

        const QString deviceName = readInputDeviceName(i);
        if (deviceName.isEmpty())
            continue;

        qDebug() << "input" << eventPath << deviceName;

        if (deviceName.contains(QStringLiteral("gpio_keys"), Qt::CaseInsensitive)) {
            gpioKeysPath = eventPath;
            break;
        }
    }

    if (gpioKeysPath.isEmpty()) {
        qWarning() << "未找到 gpio_keys 输入设备，KEY0 可能仍走 TTY 转义串";
        return;
    }

    qputenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS", QFile::encodeName(gpioKeysPath));
    qDebug() << "使用 evdev 键盘:" << gpioKeysPath;
}
#endif

int main(int argc, char *argv[])
{
    // 显式注册 pic.qrc 资源（须在 QApplication 之前；MinGW 下避免 :/picture/xxx 加载失败）
    Q_INIT_RESOURCE(pic);

#if defined(__arm__)
    setupLinuxFbDisplay();
    setupEvdevKeyboard();
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif

    // 创建 Qt 应用程序对象（每个 Qt GUI 程序必须有且仅有一个）
    QApplication a(argc, argv);

#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    qDebug() << "launcher build: GStreamer appsink video backend";
#elif defined(USE_QT_MULTIMEDIA_BACKEND)
    qDebug() << "launcher build: QtMultimedia video backend";
#elif defined(USE_MPLAYER_BACKEND)
    qDebug() << "launcher build: mplayer video backend";
#endif

#if defined(__arm__)
    QApplication::setOverrideCursor(Qt::BlankCursor);
#endif

    MainWindow w;
#if defined(__arm__)
    w.showFullScreen();
    QTimer::singleShot(300, &w, [&w]() {
        w.resize(800, 480);
        w.showFullScreen();
        w.updateGeometry();
        w.update();
    });
#else
    w.show();
#endif

    return a.exec();    // 进入事件循环，等待用户操作
}
