#ifndef CAMERAPAGE_H
#define CAMERAPAGE_H

#include <QFile>
#include <QImage>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

#if defined(USE_OPENCV_CAMERA)
namespace cv {
class VideoCapture;
class Mat;
}
#endif

class CameraPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraPageWidget(QWidget *parent = nullptr);
    ~CameraPageWidget() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void buildUi();
    void scanDevices();
    void toggleCamera();
    void startCamera();
    void stopCamera();
    void capturePhoto();
    void toggleRecording();
    void startRecording();
    void stopRecording();
    void setStatus(const QString &text);
    void showFrame(const QImage &image);
    QString photoDirPath() const;
    QString videoDirPath() const;
    QString m_lastError;

#if defined(USE_OPENCV_CAMERA)
    void readFrame();
    QImage matToImage(const cv::Mat &mat) const;
#elif defined(USE_V4L2_CAMERA)
    struct V4l2Buffer {
        void *start = nullptr;
        unsigned int length = 0;
    };

    void readFrame();
    bool openV4l2Device(const QString &path);
    void closeV4l2Device();
    bool initV4l2Mmap();
    bool isUsableV4l2Device(const QString &path) const;
    QString v4l2DeviceDisplayName(const QString &path) const;
    QString v4l2FormatSummary(const QString &path) const;
    bool setV4l2Format(unsigned int pixelFormat);
    bool isSupportedV4l2Format(unsigned int pixelFormat) const;
    void setV4l2Error(const QString &message);
    void writeAviHeader();
    void finalizeAvi();
    void appendAviFrame(const unsigned char *data, unsigned int bytesUsed);
    void writeFourcc(const char *fourcc);
    void writeLe16(quint16 value);
    void writeLe32(quint32 value);
    QImage yuyvToImage(const unsigned char *data, int width, int height) const;
    QImage mjpegToImage(const unsigned char *data, unsigned int length) const;
    static unsigned char clampColor(int value);

    struct AviIndexEntry {
        quint32 offset = 0;
        quint32 size = 0;
    };
#endif

    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_deviceBox = nullptr;
    QPushButton *m_scanBtn = nullptr;
    QPushButton *m_switchBtn = nullptr;
    QPushButton *m_photoBtn = nullptr;
    QPushButton *m_recordBtn = nullptr;
    QTimer *m_frameTimer = nullptr;
    QImage m_currentFrame;
    bool m_running = false;
    bool m_recording = false;

#if defined(USE_OPENCV_CAMERA)
    cv::VideoCapture *m_capture = nullptr;
#elif defined(USE_V4L2_CAMERA)
    QVector<V4l2Buffer> m_buffers;
    QVector<AviIndexEntry> m_aviIndex;
    QFile m_recordFile;
    int m_v4l2Fd = -1;
    int m_frameWidth = 640;
    int m_frameHeight = 480;
    int m_recordFps = 15;
    unsigned int m_pixelFormat = 0;
    quint32 m_recordFrameCount = 0;
    qint64 m_riffSizePos = 0;
    qint64 m_avihFrameCountPos = 0;
    qint64 m_strhFrameCountPos = 0;
    qint64 m_moviSizePos = 0;
    qint64 m_moviDataStart = 0;
#endif
};

#endif // CAMERAPAGE_H
