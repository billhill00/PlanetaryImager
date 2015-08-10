/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2015  <copyright holder> <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "displayimage.h"
#include "configuration.h"
#include <fps_counter.h>
#include <QThread>
#include <QElapsedTimer>
#include <QRect>
#include <QImage>

class DisplayImage::Private {
public:
  Private(Configuration &configuration, DisplayImage *q);
  Configuration &configuration;
  fps_counter capture_fps;
  ImageDataPtr imageData;
  int milliseconds_limit = 0;
  QElapsedTimer elapsed;
  QRect imageRect;
  bool running = true;
private:
  DisplayImage *q;
};

DisplayImage::Private::Private(Configuration& configuration, DisplayImage* q) : configuration{configuration}, capture_fps{[=](double fps){ emit q->captureFps(fps);}}, q{q}
{

}


DisplayImage::~DisplayImage()
{

}

DisplayImage::DisplayImage(Configuration& configuration, QObject* parent)
  : QObject(parent), dpointer(configuration, this)
{
  setRecording(false);
}
void DisplayImage::setRecording(bool recording)
{
  int fps = recording ? d->configuration.maxPreviewFPSOnSaving() : 0;
  d->milliseconds_limit = (fps == 0 ? 1000./40. : 1000/fps);
  d->elapsed.restart();
}

void DisplayImage::handle(const ImageDataPtr& imageData)
{
  if( (d->milliseconds_limit > 0 && d->elapsed.elapsed() < d->milliseconds_limit) || !imageData ) {
    return;
  }
  d->elapsed.restart();
  d->imageData = imageData;
}

void DisplayImage::create_qimages()
{
  while(d->running) {
    QThread::msleep(30);
    if(!d->imageData) {
      QThread::msleep(2);
      continue;
    }

    d->capture_fps.frame();
    auto ptrCopy = new ImageDataPtr(d->imageData);
    QImage image{ptrCopy->get()->data(), ptrCopy->get()->width(), ptrCopy->get()->height(), QImage::Format_Grayscale8, [](void *data){ delete reinterpret_cast<ImageDataPtr*>(data); }, ptrCopy};
    d->imageRect = image.rect();
    emit gotImage(image);
  }
  QThread::currentThread()->quit();
}

QRect DisplayImage::imageRect() const
{
  return d->imageRect;
}

void DisplayImage::quit()
{
  d->running = false;
}




#include "displayimage.moc"