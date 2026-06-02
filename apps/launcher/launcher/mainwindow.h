#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QKeyEvent>
#include <QHash>
#include <QMainWindow>
#include <QShortcut>
#include <QStackedWidget>

/**
 * @brief 车载系统主窗口
 *
 * 采用单窗口 + QStackedWidget 多页面结构：
 * - 第 0 页：主页（7 个 App 入口按钮）
 * - 第 1~7 页：各功能模块占位页（后续逐步替换为真实功能）
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showPage(int index);   // 切换到指定功能页
    void showHome();            // 返回主页

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupBackKeyHandling();
    bool handleBackKey(QKeyEvent *event);
    void onBackKeyActivated();
    void forceFramebufferRefresh();
    void showPageWithOpenAnimation(int index, QWidget *source);
    void showHomeWithCloseAnimation();
    void startBootAnimation();
    // 页面编号，与 QStackedWidget 的索引一一对应
    enum PageIndex {
        PageHome = 0,   // 主页
        PageMusic,      // 音乐播放
        PageVideo,      // 视频播放
        PageLighting,   // 灯光控制
        PageSensor,     // 传感器中心
        PageMap,        // 百度地图
        PageWeather,    // 天气预报
        PageCamera,     // 相机
        PageCalculator, // 计算器
        PageCount       // 页面总数（用于边界检查）
    };

    QStackedWidget *m_stack;    // 页面容器，负责切换显示哪一页
    QString m_key0EscapeBuffer; // 拼接 KEY0 的 TTY 转义序列 \x1b[26~
    QHash<int, QWidget *> m_pageTiles;
    QWidget *m_bootOverlay = nullptr;

    QWidget *createHomePage();                         // 创建主页（图标+文字按钮）
    QWidget *createPlaceholderPage(const QString &title);  // 创建功能占位页
    QWidget *createLightingPage();                     //灯控界面
    QWidget *createSensorPage();                       // 传感器中心
    QWidget *createCalculatorPage();                   // 计算器
    QWidget *createMusicPage();                        // 音乐播放
    QWidget *createVideoPage();
    QWidget *createMapPage();
    QWidget *createWeatherPage();
    QWidget *createCameraPage();
};

#endif // MAINWINDOW_H
