/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 1990-2021. All rights reserved.
* @projectName   GlowText
* @brief         glowtext.cpp
* @author        Deng Zhimao
* @email         1252699831@qq.com
* @net           www.openedv.com
* @date          2021-05-21
*******************************************************************/
#include "glowtext.h"
#include <QDebug>
#include <QGraphicsBlurEffect>

GlowText::GlowText(QWidget *parent)
    : QWidget(parent),
      textColor("#4bf3f9"),
      fontSize(18),
      textData("100")
{
    QFont font;
    font.setPixelSize(fontSize);
    QPalette pal;
    pal.setColor(QPalette::WindowText, textColor);

#if defined(__arm__)
    // linuxfb 下模糊/透明常导致文字不显示或残影
    textLabelbg = nullptr;
    textLabel = new QLabel(this);
    textLabel->setPalette(pal);
    textLabel->setFont(font);
    textLabel->setText(textData);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->adjustSize();
    resize(textLabel->size());
#else
    textLabelbg = new QLabel(this);
    textLabelbg->setAttribute(Qt::WA_TranslucentBackground, true);
    textLabelbg->setPalette(pal);
    textLabelbg->setFont(font);
    textLabelbg->setText(textData);
    textLabelbg->setAlignment(Qt::AlignCenter);

    QGraphicsBlurEffect *ef = new QGraphicsBlurEffect();
    ef->setBlurRadius(25);
    ef->setBlurHints(QGraphicsBlurEffect::QualityHint);
    textLabelbg->setGraphicsEffect(ef);

    textLabel = new QLabel(this);
    textLabel->setAttribute(Qt::WA_TranslucentBackground, true);
    textLabel->setPalette(pal);
    textLabel->setFont(font);
    textLabel->setText(textData);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabelbg->adjustSize();
    textLabel->adjustSize();

    resize(textLabel->size().width() + 10, textLabel->size().height() + 10);
    setAttribute(Qt::WA_TranslucentBackground, true);
#endif
}

GlowText::~GlowText()
{
}

void GlowText::setTextColor(QColor color)
{
    textColor = color;
    QPalette pal;
    pal.setColor(QPalette::WindowText, color);
    if (textLabelbg)
        textLabelbg->setPalette(pal);
    textLabel->setPalette(pal);
}

void GlowText::setFontSize(int size)
{
    fontSize = size;
    QFont font;
    font.setPixelSize(size);

    if (textLabelbg)
        textLabelbg->setFont(font);
    textLabel->setFont(font);

    textLabel->adjustSize();
    if (textLabelbg)
        textLabelbg->adjustSize();
#if defined(__arm__)
    resize(textLabel->size());
#else
    resize(textLabel->size().width() + 10, textLabel->size().height() + 10);
#endif
}

void GlowText::setTextData(QString text)
{
    textData = text;
    if (textLabelbg)
        textLabelbg->setText(text);
    textLabel->setText(text);

    textLabel->adjustSize();
    if (textLabelbg)
        textLabelbg->adjustSize();
#if defined(__arm__)
    resize(textLabel->size());
#else
    resize(textLabel->size().width() + 10, textLabel->size().height() + 10);
#endif
}
