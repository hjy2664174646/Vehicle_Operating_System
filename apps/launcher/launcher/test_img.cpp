#include <QApplication>
#include <QImage>
#include <QImageReader>
#include <QDebug>
int main(int argc,char**argv){
  QApplication a(argc,argv);
  Q_INIT_RESOURCE(pic);
  QImageReader r(":/picture/background.png");
  qDebug() << "res" << r.size() << r.read().isNull() << r.errorString();
  QImage img;
  qDebug() << "disk load" << img.load("picture/background.png") << img.size();
  QImage img2;
  QByteArray d; QFile f("picture/background.png"); f.open(QIODevice::ReadOnly); d=f.readAll();
  qDebug() << "fromData" << img2.loadFromData(d) << img2.size();
  return 0;
}
