#include "camerapage.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadialGradient>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#if defined(USE_OPENCV_CAMERA)
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/videoio/videoio.hpp>
#elif defined(USE_V4L2_CAMERA)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

QString buttonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(255,255,255,0.10);"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 6px;"
        "  color: #EAF6FF;"
        "  font-size: 15px;"
        "  min-height: 38px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:pressed { background-color: rgba(95,178,255,0.28); }"
        "QPushButton:disabled { color: rgba(234,246,255,0.35); background-color: rgba(255,255,255,0.05); }");
}

QString comboStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "  background-color: rgba(7,12,18,0.90);"
        "  border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 6px;"
        "  color: #EAF6FF;"
        "  font-size: 14px;"
        "  min-height: 34px;"
        "  padding-left: 10px;"
        "}"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background-color: #101923; color: #EAF6FF; selection-background-color: #285D89; }");
}

void paintCameraBackground(QPainter &p, const QRect &rect)
{
    p.fillRect(rect, QColor(QStringLiteral("#060a0f")));

    QRadialGradient glow(QPointF(rect.width() * 0.28, rect.height() * 0.26),
                         qreal(qMax(rect.width(), rect.height())) * 0.68);
    glow.setColorAt(0.0, QColor(24, 74, 94, 185));
    glow.setColorAt(0.45, QColor(11, 20, 28, 150));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);
}

} // namespace

CameraPageWidget::CameraPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_frameTimer(new QTimer(this))
{
#if defined(USE_OPENCV_CAMERA)
    m_capture = new cv::VideoCapture;
    connect(m_frameTimer, &QTimer::timeout, this, &CameraPageWidget::readFrame);
#elif defined(USE_V4L2_CAMERA)
    connect(m_frameTimer, &QTimer::timeout, this, &CameraPageWidget::readFrame);
#endif

    setAutoFillBackground(false);
    buildUi();
    scanDevices();
}

CameraPageWidget::~CameraPageWidget()
{
    stopCamera();
#if defined(USE_OPENCV_CAMERA)
    delete m_capture;
    m_capture = nullptr;
#elif defined(USE_V4L2_CAMERA)
    closeV4l2Device();
#endif
}

void CameraPageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintCameraBackground(painter, rect());
}

void CameraPageWidget::hideEvent(QHideEvent *event)
{
    stopCamera();
    QWidget::hideEvent(event);
}

void CameraPageWidget::buildUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    auto *previewPanel = new QWidget(this);
    previewPanel->setAttribute(Qt::WA_StyledBackground, true);
    previewPanel->setStyleSheet(QStringLiteral(
        "background-color: #020305;"
        "border: 1px solid rgba(255,255,255,0.10);"
        "border-radius: 8px;"));

    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    m_previewLabel = new QLabel(QString::fromUtf8("相机预览"), previewPanel);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(480, 320);
    m_previewLabel->setStyleSheet(QStringLiteral("color: #6F8294; font-size: 20px; background: transparent;"));
    previewLayout->addWidget(m_previewLabel);

    auto *ctrlPanel = new QWidget(this);
    ctrlPanel->setFixedWidth(210);
    ctrlPanel->setAttribute(Qt::WA_StyledBackground, true);
    ctrlPanel->setStyleSheet(QStringLiteral(
        "background-color: rgba(12,18,25,0.90);"
        "border: 1px solid rgba(255,255,255,0.10);"
        "border-radius: 8px;"));

    auto *ctrlLayout = new QVBoxLayout(ctrlPanel);
    ctrlLayout->setContentsMargins(14, 14, 14, 14);
    ctrlLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QString::fromUtf8("相机"), ctrlPanel);
    titleLabel->setStyleSheet(QStringLiteral("color: #F2FAFF; font-size: 20px; font-weight: bold; background: transparent;"));

    auto *deviceLabel = new QLabel(QString::fromUtf8("设备"), ctrlPanel);
    deviceLabel->setStyleSheet(QStringLiteral("color: #8EA4B8; font-size: 13px; background: transparent;"));

    m_deviceBox = new QComboBox(ctrlPanel);
    m_deviceBox->setFocusPolicy(Qt::NoFocus);
    m_deviceBox->setStyleSheet(comboStyle());

    m_scanBtn = new QPushButton(QString::fromUtf8("扫描设备"), ctrlPanel);
    m_switchBtn = new QPushButton(QString::fromUtf8("打开相机"), ctrlPanel);
    m_photoBtn = new QPushButton(QString::fromUtf8("拍照保存"), ctrlPanel);
    m_recordBtn = new QPushButton(QString::fromUtf8("开始录像"), ctrlPanel);

    for (auto *btn : {m_scanBtn, m_switchBtn, m_photoBtn, m_recordBtn}) {
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(buttonStyle());
    }

    m_photoBtn->setEnabled(false);
    m_recordBtn->setEnabled(false);

    m_statusLabel = new QLabel(ctrlPanel);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #8EC5FF; font-size: 13px; background: transparent;"));

    ctrlLayout->addWidget(titleLabel);
    ctrlLayout->addSpacing(8);
    ctrlLayout->addWidget(deviceLabel);
    ctrlLayout->addWidget(m_deviceBox);
    ctrlLayout->addWidget(m_scanBtn);
    ctrlLayout->addWidget(m_switchBtn);
    ctrlLayout->addWidget(m_photoBtn);
    ctrlLayout->addWidget(m_recordBtn);
    ctrlLayout->addSpacing(8);
    ctrlLayout->addWidget(m_statusLabel);
    ctrlLayout->addStretch();

    root->addWidget(previewPanel, 1);
    root->addWidget(ctrlPanel, 0);

    connect(m_scanBtn, &QPushButton::clicked, this, &CameraPageWidget::scanDevices);
    connect(m_switchBtn, &QPushButton::clicked, this, &CameraPageWidget::toggleCamera);
    connect(m_photoBtn, &QPushButton::clicked, this, &CameraPageWidget::capturePhoto);
    connect(m_recordBtn, &QPushButton::clicked, this, &CameraPageWidget::toggleRecording);
}

void CameraPageWidget::scanDevices()
{
    if (m_running)
        stopCamera();

    m_deviceBox->clear();

#if defined(USE_OPENCV_CAMERA)
#if defined(Q_OS_WIN)
    for (int i = 0; i < 4; ++i)
        m_deviceBox->addItem(QString::fromUtf8("摄像头 %1").arg(i), i);
#else
    for (int i = 0; i < 6; ++i) {
        const QString path = QStringLiteral("/dev/video%1").arg(i);
        if (QFileInfo::exists(path))
            m_deviceBox->addItem(path, i);
    }
#endif

    if (m_deviceBox->count() == 0) {
        setStatus(QString::fromUtf8("未检测到 USB 摄像头"));
        m_switchBtn->setEnabled(false);
    } else {
        setStatus(QString::fromUtf8("已检测到 %1 个相机设备").arg(m_deviceBox->count()));
        m_switchBtn->setEnabled(true);
    }
#elif defined(USE_V4L2_CAMERA)
    for (int i = 0; i < 6; ++i) {
        const QString path = QStringLiteral("/dev/video%1").arg(i);
        if (QFileInfo::exists(path) && isUsableV4l2Device(path)) {
            m_deviceBox->addItem(QStringLiteral("%1 - %2 [%3]")
                                     .arg(path, v4l2DeviceDisplayName(path), v4l2FormatSummary(path)),
                                 path);
        }
    }

    if (m_deviceBox->count() == 0) {
        setStatus(QString::fromUtf8("未检测到 USB 摄像头"));
        m_switchBtn->setEnabled(false);
    } else {
        setStatus(QString::fromUtf8("已检测到 %1 个相机设备").arg(m_deviceBox->count()));
        m_switchBtn->setEnabled(true);
    }
#else
    m_deviceBox->addItem(QString::fromUtf8("相机后端未启用"), -1);
    m_switchBtn->setEnabled(false);
    setStatus(QString::fromUtf8("当前构建未启用相机后端"));
#endif
}

void CameraPageWidget::toggleCamera()
{
    if (m_running)
        stopCamera();
    else
        startCamera();
}

void CameraPageWidget::startCamera()
{
#if defined(USE_OPENCV_CAMERA)
    if (!m_capture || m_deviceBox->count() == 0) {
        setStatus(QString::fromUtf8("没有可用相机"));
        return;
    }

    const int deviceIndex = m_deviceBox->currentData().toInt();
    if (m_capture->isOpened())
        m_capture->release();

    if (!m_capture->open(deviceIndex)) {
        setStatus(QString::fromUtf8("相机打开失败"));
        return;
    }

    m_capture->set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    m_running = true;
    m_deviceBox->setEnabled(false);
    m_scanBtn->setEnabled(false);
    m_photoBtn->setEnabled(false);
    m_recordBtn->setEnabled(false);
    m_switchBtn->setText(QString::fromUtf8("关闭相机"));
    m_frameTimer->start(33);
    setStatus(QString::fromUtf8("相机已打开"));
#elif defined(USE_V4L2_CAMERA)
    if (m_deviceBox->count() == 0) {
        setStatus(QString::fromUtf8("没有可用相机"));
        return;
    }

    const QString devicePath = m_deviceBox->currentData().toString();
    if (!openV4l2Device(devicePath)) {
        setStatus(QString::fromUtf8("相机打开失败：%1\n%2").arg(devicePath, m_lastError));
        return;
    }

    m_running = true;
    m_deviceBox->setEnabled(false);
    m_scanBtn->setEnabled(false);
    m_photoBtn->setEnabled(false);
    m_recordBtn->setEnabled(m_pixelFormat == V4L2_PIX_FMT_YUYV);
    m_switchBtn->setText(QString::fromUtf8("关闭相机"));
    m_frameTimer->start(33);
    setStatus(QString::fromUtf8("相机已打开：%1").arg(devicePath));
#else
    setStatus(QString::fromUtf8("当前构建未启用相机后端"));
#endif
}

void CameraPageWidget::stopCamera()
{
    if (m_frameTimer)
        m_frameTimer->stop();

    stopRecording();

#if defined(USE_OPENCV_CAMERA)
    if (m_capture && m_capture->isOpened())
        m_capture->release();
#elif defined(USE_V4L2_CAMERA)
    closeV4l2Device();
#endif

    if (m_running)
        setStatus(QString::fromUtf8("相机已关闭"));

    m_running = false;
    m_currentFrame = QImage();
    if (m_previewLabel) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QString::fromUtf8("相机预览"));
    }
    if (m_deviceBox)
        m_deviceBox->setEnabled(true);
    if (m_scanBtn)
        m_scanBtn->setEnabled(true);
    if (m_switchBtn)
        m_switchBtn->setText(QString::fromUtf8("打开相机"));
    if (m_photoBtn)
        m_photoBtn->setEnabled(false);
    if (m_recordBtn) {
        m_recordBtn->setText(QString::fromUtf8("开始录像"));
        m_recordBtn->setEnabled(false);
    }
}

void CameraPageWidget::capturePhoto()
{
    if (m_currentFrame.isNull()) {
        setStatus(QString::fromUtf8("当前没有可保存的画面"));
        return;
    }

    QDir dir(photoDirPath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        setStatus(QString::fromUtf8("照片目录创建失败"));
        return;
    }

    const QString fileName = QStringLiteral("photo_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString filePath = dir.filePath(fileName);

    if (m_currentFrame.save(filePath, "PNG")) {
        setStatus(QString::fromUtf8("已保存：%1").arg(filePath));
    } else {
        setStatus(QString::fromUtf8("照片保存失败"));
    }
}

void CameraPageWidget::toggleRecording()
{
    if (m_recording)
        stopRecording();
    else
        startRecording();
}

void CameraPageWidget::startRecording()
{
#if defined(USE_V4L2_CAMERA)
    if (!m_running || m_pixelFormat != V4L2_PIX_FMT_YUYV) {
        setStatus(QString::fromUtf8("当前相机格式不支持录像"));
        return;
    }

    QDir dir(videoDirPath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        setStatus(QString::fromUtf8("录像目录创建失败"));
        return;
    }

    const QString fileName = QStringLiteral("video_%1.avi")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString filePath = dir.filePath(fileName);

    m_recordFile.setFileName(filePath);
    if (!m_recordFile.open(QIODevice::WriteOnly)) {
        setStatus(QString::fromUtf8("录像文件打开失败"));
        return;
    }

    m_aviIndex.clear();
    m_recordFrameCount = 0;
    writeAviHeader();

    m_recording = true;
    m_recordBtn->setText(QString::fromUtf8("停止录像"));
    setStatus(QString::fromUtf8("正在录像：%1").arg(filePath));
#else
    setStatus(QString::fromUtf8("当前构建不支持录像"));
#endif
}

void CameraPageWidget::stopRecording()
{
#if defined(USE_V4L2_CAMERA)
    if (!m_recording)
        return;

    m_recording = false;
    finalizeAvi();
    const QString filePath = m_recordFile.fileName();
    m_recordFile.close();
    m_recordBtn->setText(QString::fromUtf8("开始录像"));
    setStatus(QString::fromUtf8("录像已保存：%1").arg(filePath));
#endif
}

void CameraPageWidget::setStatus(const QString &text)
{
    if (m_statusLabel)
        m_statusLabel->setText(text);
}

void CameraPageWidget::showFrame(const QImage &image)
{
    if (image.isNull() || !m_previewLabel)
        return;

    m_currentFrame = image;
    const QPixmap pixmap = QPixmap::fromImage(image);
    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    m_photoBtn->setEnabled(true);
}

QString CameraPageWidget::photoDirPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/photos");
}

QString CameraPageWidget::videoDirPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/videos");
}

#if defined(USE_OPENCV_CAMERA)
void CameraPageWidget::readFrame()
{
    if (!m_capture || !m_capture->isOpened()) {
        stopCamera();
        setStatus(QString::fromUtf8("相机连接已断开"));
        return;
    }

    cv::Mat frame;
    if (!m_capture->read(frame) || frame.empty())
        return;

    showFrame(matToImage(frame));
}

QImage CameraPageWidget::matToImage(const cv::Mat &mat) const
{
    if (mat.empty())
        return QImage();

    if (mat.type() == CV_8UC3) {
        QImage image(mat.data, mat.cols, mat.rows, int(mat.step), QImage::Format_RGB888);
        return image.rgbSwapped().copy();
    }

    if (mat.type() == CV_8UC1) {
        QImage image(mat.data, mat.cols, mat.rows, int(mat.step), QImage::Format_Grayscale8);
        return image.copy();
    }

    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    QImage image(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
    return image.copy();
}
#elif defined(USE_V4L2_CAMERA)
void CameraPageWidget::readFrame()
{
    if (m_v4l2Fd < 0)
        return;

    v4l2_buffer buffer;
    std::memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_v4l2Fd, VIDIOC_DQBUF, &buffer) < 0) {
        if (errno != EAGAIN) {
            stopCamera();
            setStatus(QString::fromUtf8("读取相机画面失败"));
        }
        return;
    }

    QImage image;
    if (buffer.index < static_cast<unsigned int>(m_buffers.size())) {
        const auto &buf = m_buffers.at(int(buffer.index));
        if (m_pixelFormat == V4L2_PIX_FMT_MJPEG)
            image = mjpegToImage(static_cast<const unsigned char *>(buf.start), buffer.bytesused);
        else {
            image = yuyvToImage(static_cast<const unsigned char *>(buf.start), m_frameWidth, m_frameHeight);
            if (m_recording)
                appendAviFrame(static_cast<const unsigned char *>(buf.start), buffer.bytesused);
        }
    }

    if (::ioctl(m_v4l2Fd, VIDIOC_QBUF, &buffer) < 0) {
        stopCamera();
        setStatus(QString::fromUtf8("相机缓冲区入队失败"));
        return;
    }

    if (!image.isNull())
        showFrame(image);
}

bool CameraPageWidget::openV4l2Device(const QString &path)
{
    closeV4l2Device();
    m_lastError.clear();

    const auto fail = [this]() {
        closeV4l2Device();
        return false;
    };

    m_v4l2Fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (m_v4l2Fd < 0) {
        setV4l2Error(QString::fromUtf8("open 失败"));
        return false;
    }

    v4l2_capability cap;
    std::memset(&cap, 0, sizeof(cap));
    if (::ioctl(m_v4l2Fd, VIDIOC_QUERYCAP, &cap) < 0) {
        setV4l2Error(QString::fromUtf8("VIDIOC_QUERYCAP 失败"));
        return fail();
    }

    const unsigned int caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) {
        setV4l2Error(QString::fromUtf8("设备不支持 VIDEO_CAPTURE/STREAMING"));
        return fail();
    }

    if (!setV4l2Format(V4L2_PIX_FMT_YUYV) && !setV4l2Format(V4L2_PIX_FMT_MJPEG)) {
        setV4l2Error(QString::fromUtf8("不支持 YUYV/MJPEG 格式"));
        return fail();
    }

    if (!initV4l2Mmap())
        return fail();

    for (int i = 0; i < m_buffers.size(); ++i) {
        v4l2_buffer buffer;
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = unsigned(i);
        if (::ioctl(m_v4l2Fd, VIDIOC_QBUF, &buffer) < 0)
            return fail();
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_v4l2Fd, VIDIOC_STREAMON, &type) < 0)
        return fail();

    return true;
}

void CameraPageWidget::closeV4l2Device()
{
    if (m_v4l2Fd >= 0) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ::ioctl(m_v4l2Fd, VIDIOC_STREAMOFF, &type);
    }

    for (const auto &buffer : m_buffers) {
        if (buffer.start && buffer.length > 0)
            ::munmap(buffer.start, buffer.length);
    }
    m_buffers.clear();

    if (m_v4l2Fd >= 0) {
        ::close(m_v4l2Fd);
        m_v4l2Fd = -1;
    }
}

bool CameraPageWidget::initV4l2Mmap()
{
    v4l2_requestbuffers request;
    std::memset(&request, 0, sizeof(request));
    request.count = 4;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_v4l2Fd, VIDIOC_REQBUFS, &request) < 0) {
        setV4l2Error(QString::fromUtf8("VIDIOC_REQBUFS 失败"));
        return false;
    }

    if (request.count < 2) {
        setV4l2Error(QString::fromUtf8("缓冲区数量不足"));
        return false;
    }

    m_buffers.resize(int(request.count));
    for (unsigned int i = 0; i < request.count; ++i) {
        v4l2_buffer buffer;
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (::ioctl(m_v4l2Fd, VIDIOC_QUERYBUF, &buffer) < 0) {
            setV4l2Error(QString::fromUtf8("VIDIOC_QUERYBUF 失败"));
            return false;
        }

        void *start = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, m_v4l2Fd, buffer.m.offset);
        if (start == MAP_FAILED) {
            setV4l2Error(QString::fromUtf8("mmap 失败"));
            return false;
        }

        m_buffers[int(i)].start = start;
        m_buffers[int(i)].length = buffer.length;
    }

    return true;
}

bool CameraPageWidget::isUsableV4l2Device(const QString &path) const
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return false;

    v4l2_capability cap;
    std::memset(&cap, 0, sizeof(cap));
    const bool hasCap = (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0);
    if (!hasCap) {
        ::close(fd);
        return false;
    }

    const unsigned int caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) {
        ::close(fd);
        return false;
    }

    bool hasSupportedFormat = false;
    v4l2_fmtdesc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (desc.index = 0; ::ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0; ++desc.index) {
        if (desc.pixelformat == V4L2_PIX_FMT_YUYV || desc.pixelformat == V4L2_PIX_FMT_MJPEG) {
            hasSupportedFormat = true;
            break;
        }
    }

    ::close(fd);
    return hasSupportedFormat;
}

QString CameraPageWidget::v4l2DeviceDisplayName(const QString &path) const
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return QString::fromUtf8("未知设备");

    v4l2_capability cap;
    std::memset(&cap, 0, sizeof(cap));
    QString name = QString::fromUtf8("未知设备");
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        name = QString::fromLocal8Bit(reinterpret_cast<const char *>(cap.card));
        if (name.trimmed().isEmpty())
            name = QString::fromLocal8Bit(reinterpret_cast<const char *>(cap.driver));
    }

    ::close(fd);
    return name;
}

QString CameraPageWidget::v4l2FormatSummary(const QString &path) const
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return QString::fromUtf8("格式未知");

    QStringList formats;
    v4l2_fmtdesc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (desc.index = 0; ::ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0; ++desc.index) {
        char fourcc[5] = {
            char(desc.pixelformat & 0xff),
            char((desc.pixelformat >> 8) & 0xff),
            char((desc.pixelformat >> 16) & 0xff),
            char((desc.pixelformat >> 24) & 0xff),
            '\0'
        };
        const QString item = QString::fromLatin1(fourcc);
        if (!formats.contains(item))
            formats << item;
    }

    ::close(fd);
    return formats.isEmpty() ? QString::fromUtf8("格式未知") : formats.join(QStringLiteral(", "));
}

bool CameraPageWidget::setV4l2Format(unsigned int pixelFormat)
{
    if (!isSupportedV4l2Format(pixelFormat))
        return false;

    v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = pixelFormat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (::ioctl(m_v4l2Fd, VIDIOC_S_FMT, &fmt) < 0)
        return false;

    if (fmt.fmt.pix.pixelformat != pixelFormat)
        return false;

    m_frameWidth = int(fmt.fmt.pix.width);
    m_frameHeight = int(fmt.fmt.pix.height);
    m_pixelFormat = fmt.fmt.pix.pixelformat;
    return true;
}

bool CameraPageWidget::isSupportedV4l2Format(unsigned int pixelFormat) const
{
    v4l2_fmtdesc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (desc.index = 0; ::ioctl(m_v4l2Fd, VIDIOC_ENUM_FMT, &desc) == 0; ++desc.index) {
        if (desc.pixelformat == pixelFormat)
            return true;
    }

    return false;
}

void CameraPageWidget::setV4l2Error(const QString &message)
{
    const int err = errno;
    if (err == 0) {
        m_lastError = message;
        return;
    }
    m_lastError = QStringLiteral("%1: %2").arg(message, QString::fromLocal8Bit(std::strerror(err)));
}

void CameraPageWidget::writeAviHeader()
{
    const quint32 frameBytes = quint32(m_frameWidth * m_frameHeight * 2);
    const quint32 bytesPerSecond = frameBytes * quint32(m_recordFps);

    writeFourcc("RIFF");
    m_riffSizePos = m_recordFile.pos();
    writeLe32(0);
    writeFourcc("AVI ");

    writeFourcc("LIST");
    writeLe32(192);
    writeFourcc("hdrl");

    writeFourcc("avih");
    writeLe32(56);
    writeLe32(quint32(1000000 / m_recordFps));
    writeLe32(bytesPerSecond);
    writeLe32(0);
    writeLe32(0x10);
    m_avihFrameCountPos = m_recordFile.pos();
    writeLe32(0);
    writeLe32(0);
    writeLe32(1);
    writeLe32(frameBytes);
    writeLe32(quint32(m_frameWidth));
    writeLe32(quint32(m_frameHeight));
    writeLe32(0);
    writeLe32(0);
    writeLe32(0);
    writeLe32(0);

    writeFourcc("LIST");
    writeLe32(116);
    writeFourcc("strl");

    writeFourcc("strh");
    writeLe32(56);
    writeFourcc("vids");
    writeFourcc("YUY2");
    writeLe32(0);
    writeLe16(0);
    writeLe16(0);
    writeLe32(0);
    writeLe32(1);
    writeLe32(quint32(m_recordFps));
    writeLe32(0);
    m_strhFrameCountPos = m_recordFile.pos();
    writeLe32(0);
    writeLe32(frameBytes);
    writeLe32(0xFFFFFFFFu);
    writeLe32(0);
    writeLe16(0);
    writeLe16(0);
    writeLe16(quint16(m_frameWidth));
    writeLe16(quint16(m_frameHeight));

    writeFourcc("strf");
    writeLe32(40);
    writeLe32(40);
    writeLe32(quint32(m_frameWidth));
    writeLe32(quint32(m_frameHeight));
    writeLe16(1);
    writeLe16(16);
    writeFourcc("YUY2");
    writeLe32(frameBytes);
    writeLe32(0);
    writeLe32(0);
    writeLe32(0);
    writeLe32(0);

    writeFourcc("LIST");
    m_moviSizePos = m_recordFile.pos();
    writeLe32(0);
    writeFourcc("movi");
    m_moviDataStart = m_recordFile.pos();
}

void CameraPageWidget::finalizeAvi()
{
    if (!m_recordFile.isOpen())
        return;

    const qint64 idxStart = m_recordFile.pos();
    writeFourcc("idx1");
    writeLe32(quint32(m_aviIndex.size() * 16));
    for (const auto &entry : m_aviIndex) {
        writeFourcc("00db");
        writeLe32(0x10);
        writeLe32(entry.offset);
        writeLe32(entry.size);
    }

    const qint64 fileEnd = m_recordFile.pos();

    m_recordFile.seek(m_riffSizePos);
    writeLe32(quint32(fileEnd - 8));

    m_recordFile.seek(m_avihFrameCountPos);
    writeLe32(m_recordFrameCount);

    m_recordFile.seek(m_strhFrameCountPos);
    writeLe32(m_recordFrameCount);

    m_recordFile.seek(m_moviSizePos);
    writeLe32(quint32(idxStart - m_moviSizePos - 4));

    m_recordFile.seek(fileEnd);
}

void CameraPageWidget::appendAviFrame(const unsigned char *data, unsigned int bytesUsed)
{
    if (!m_recordFile.isOpen() || !data || bytesUsed == 0)
        return;

    const qint64 chunkStart = m_recordFile.pos();
    writeFourcc("00db");
    writeLe32(bytesUsed);
    m_recordFile.write(reinterpret_cast<const char *>(data), bytesUsed);
    if (bytesUsed & 1)
        m_recordFile.putChar('\0');

    AviIndexEntry entry;
    entry.offset = quint32(chunkStart - m_moviDataStart);
    entry.size = bytesUsed;
    m_aviIndex.push_back(entry);
    ++m_recordFrameCount;
}

void CameraPageWidget::writeFourcc(const char *fourcc)
{
    m_recordFile.write(fourcc, 4);
}

void CameraPageWidget::writeLe16(quint16 value)
{
    char data[2] = {
        char(value & 0xff),
        char((value >> 8) & 0xff)
    };
    m_recordFile.write(data, 2);
}

void CameraPageWidget::writeLe32(quint32 value)
{
    char data[4] = {
        char(value & 0xff),
        char((value >> 8) & 0xff),
        char((value >> 16) & 0xff),
        char((value >> 24) & 0xff)
    };
    m_recordFile.write(data, 4);
}

QImage CameraPageWidget::yuyvToImage(const unsigned char *data, int width, int height) const
{
    QImage image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y) {
        unsigned char *line = image.scanLine(y);
        const unsigned char *src = data + y * width * 2;
        for (int x = 0; x < width; x += 2) {
            const int y0 = src[0];
            const int u = src[1] - 128;
            const int y1 = src[2];
            const int v = src[3] - 128;

            const int r0 = y0 + ((91881 * v) >> 16);
            const int g0 = y0 - ((46802 * v + 23396 * u) >> 16);
            const int b0 = y0 + ((116130 * u) >> 16);
            const int r1 = y1 + ((91881 * v) >> 16);
            const int g1 = y1 - ((46802 * v + 23396 * u) >> 16);
            const int b1 = y1 + ((116130 * u) >> 16);

            line[x * 3 + 0] = clampColor(r0);
            line[x * 3 + 1] = clampColor(g0);
            line[x * 3 + 2] = clampColor(b0);
            if (x + 1 < width) {
                line[(x + 1) * 3 + 0] = clampColor(r1);
                line[(x + 1) * 3 + 1] = clampColor(g1);
                line[(x + 1) * 3 + 2] = clampColor(b1);
            }
            src += 4;
        }
    }

    return image;
}

QImage CameraPageWidget::mjpegToImage(const unsigned char *data, unsigned int length) const
{
    QImage image;
    image.loadFromData(data, int(length), "JPG");
    return image.convertToFormat(QImage::Format_RGB888);
}

unsigned char CameraPageWidget::clampColor(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return static_cast<unsigned char>(value);
}
#endif
