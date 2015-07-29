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

#include "qhyccdimager.h"
#include "qhydriver.h"
#include "qhyccd.h"
#include <QDebug>

#include <libusb.h>
#include <QThread>
#include "utils.h"
#include <QImage>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>

using namespace std;


class ImagingWorker : public QObject {
  Q_OBJECT
public:
  ImagingWorker(qhyccd_handle *handle, QHYCCDImager *imager, QObject* parent = 0);
  void convert_image_data(const QByteArray &data, int w, int h, int bpp, int channels);
public slots:
  void start_live();
  void stop();
private:
  qhyccd_handle *handle;
  bool enabled = true;
  QHYCCDImager *imager;
};

class QHYCCDImager::Private {
public:
  Private(const QString &name, const QString &id, QHYCCDImager *q);
  qhyccd_handle *handle;
  QString name;
  QString id;
  Chip chip;
  Settings settings;
  void load_settings();
  QThread imaging_thread;
  ImagingWorker *worker;
private:
  QHYCCDImager *q;
};


QHYCCDImager::Private::Private(const QString &name, const QString &id, QHYCCDImager* q) : name(name), id(id), q{q}
{
}

QHYCCDImager::QHYCCDImager(QHYDriver::Camera camera) : dpointer(camera.name(), camera.id, this)
{
  d->handle = OpenQHYCCD(camera.id);
  if(d->handle < QHYCCD_SUCCESS) {
    throw QHYDriver::error("Initializing Camera %1"_q % camera.id, (long)(d->handle));
  }
  if(int result = InitQHYCCD(d->handle) != QHYCCD_SUCCESS) {
    throw QHYDriver::error("Initializing Camera %1"_q % camera.id, result);
  }
  qDebug() << "Camera " << camera.id << "initialized correctly";
  qDebug() << "gain: " << GetQHYCCDParam(d->handle, CONTROL_GAIN) << ", gamma: " << GetQHYCCDParam(d->handle, CONTROL_GAMMA) << ", exposure: " << GetQHYCCDParam(d->handle, CONTROL_EXPOSURE);
  char buf[1024];
  uint8_t st[1024];
  // Not implemented in QHY Library
//   GetQHYCCDCFWStatus(d->handle, buf);
//   qDebug() << "firmware status: " << QByteArray{buf};
  GetQHYCCDFWVersion(d->handle, st);
  copy(begin(st), end(st), begin(buf));
  qDebug() << "firmware version: " << QByteArray{buf};
  GetQHYCCDChipInfo(d->handle, &d->chip.width, &d->chip.height, &d->chip.xres, &d->chip.yres, &d->chip.pixelwidth, &d->chip.pixelwidth, &d->chip.bpp);
  qDebug() << d->chip;
  d->load_settings();
  qDebug() << d->settings;
//   GetQHYCCDCameraStatus(d->handle, st); // NOT IMPLEMENTED IN QHY Library
}


QHYCCDImager::~QHYCCDImager()
{
  CloseQHYCCD(d->handle);
}

QHYCCDImager::Chip QHYCCDImager::chip() const
{
  return d->chip;
}

QString QHYCCDImager::id() const
{
  return d->id;
}

QString QHYCCDImager::name() const
{
  return d->name;
}

QList< QHYCCDImager::Setting > QHYCCDImager::settings() const
{
  return d->settings;
}

void QHYCCDImager::Private::load_settings()
{
  settings.clear();
  for(auto control: QList<QPair<QString,CONTROL_ID>>{
    { "control_brightness", CONTROL_BRIGHTNESS },
    { "control_contrast", CONTROL_CONTRAST },
    { "control_wbr", CONTROL_WBR },
    { "control_wbb", CONTROL_WBB },
    { "control_wbg", CONTROL_WBG },
    { "control_gamma", CONTROL_GAMMA },
    { "control_gain", CONTROL_GAIN },
    { "control_offset", CONTROL_OFFSET },
    { "control_exposure", CONTROL_EXPOSURE },
    { "control_speed", CONTROL_SPEED },
    { "control_transferbit", CONTROL_TRANSFERBIT },
    { "control_channels", CONTROL_CHANNELS },
    { "control_usbtraffic", CONTROL_USBTRAFFIC },
    { "control_rownoisere", CONTROL_ROWNOISERE },
    { "control_curtemp", CONTROL_CURTEMP },
    { "control_curpwm", CONTROL_CURPWM },
    { "control_manulpwm", CONTROL_MANULPWM },
    { "control_cfwport", CONTROL_CFWPORT },
    { "control_cooler", CONTROL_COOLER },
    { "control_st4port", CONTROL_ST4PORT },
    { "cam_color", CAM_COLOR },
    { "cam_bin1x1mode", CAM_BIN1X1MODE },
    { "cam_bin2x2mode", CAM_BIN2X2MODE },
    { "cam_bin3x3mode", CAM_BIN3X3MODE },
    { "cam_bin4x4mode", CAM_BIN4X4MODE },
    { "CAM_MECHANICALSHUTTER", CAM_MECHANICALSHUTTER },
    { "CAM_TRIGER_INTERFACE", CAM_TRIGER_INTERFACE },
    { "CAM_TECOVERPROTECT_INTERFACE", CAM_TECOVERPROTECT_INTERFACE },
    { "CAM_SINGNALCLAMP_INTERFACE", CAM_SINGNALCLAMP_INTERFACE },
    { "CAM_FINETONE_INTERFACE", CAM_FINETONE_INTERFACE },
    { "CAM_SHUTTERMOTORHEATING_INTERFACE", CAM_SHUTTERMOTORHEATING_INTERFACE },
    { "CAM_CALIBRATEFPN_INTERFACE", CAM_CALIBRATEFPN_INTERFACE },
    { "CAM_CHIPTEMPERATURESENSOR_INTERFACE", CAM_CHIPTEMPERATURESENSOR_INTERFACE },
    { "CAM_USBREADOUTSLOWEST_INTERFACE", CAM_USBREADOUTSLOWEST_INTERFACE },

  }) {
    int result = IsQHYCCDControlAvailable(handle, control.second);
    if(result == QHYCCD_ERROR_NOTSUPPORT) {
      qDebug() << "control " << control.first << "not supported, skipping";
      continue;
    }
    Setting setting{control.second, control.first};
    result = GetQHYCCDParamMinMaxStep(handle, control.second, &setting.min, &setting.max, &setting.step);
    if(result != QHYCCD_SUCCESS) {
      qCritical() << "error retrieving control " << control.first << ":" << QHYDriver::error_name(result) << "(" << result << ")";
      continue;
    }
    setting.value = GetQHYCCDParam(handle, control.second);
    qDebug() << setting;
    settings << setting;
  }
  emit q->settingsLoaded(settings);
}

void QHYCCDImager::setSetting(const QHYCCDImager::Setting& setting)
{
  auto result = SetQHYCCDParam(d->handle, static_cast<CONTROL_ID>(setting.id), setting.value);
  if(result != QHYCCD_SUCCESS) {
    qCritical() << "error setting" << setting.name << ":" << QHYDriver::error_name(result) << "(" << result << ")";
    return;
  }
  d->load_settings();
}

void QHYCCDImager::startLive()
{
  d->worker = new ImagingWorker{d->handle, this};
  d->worker->moveToThread(&d->imaging_thread);
  connect(&d->imaging_thread, SIGNAL(started()), d->worker, SLOT(start_live()));
  d->imaging_thread.start();
  qDebug() << "Live started correctly";
}

ImagingWorker::ImagingWorker(qhyccd_handle* handle, QHYCCDImager* imager, QObject* parent): QObject(parent), handle(handle), imager{imager}
{
}

void ImagingWorker::start_live()
{
  auto size = GetQHYCCDMemLength(handle);
  auto result =  SetQHYCCDStreamMode(handle,1);
  if(result != QHYCCD_SUCCESS) {
    qCritical() << "Unable to set live mode stream";
    return;
  }
  result =  SetQHYCCDResolution(handle, 0, 0, 1280, 960);
  if(result != QHYCCD_SUCCESS) {
    qCritical() << "Unable to set resolution";
    return;
  }
  result = BeginQHYCCDLive(handle);
  if(result != QHYCCD_SUCCESS) {
    qCritical() << "Unable to start live mode";
    return;
  }

  uint8_t buffer[size];
  qDebug() << "capturing thread started, image size: " << size;
  long frames = 0;
  QElapsedTimer timer;
  timer.start();
  int w, h, bpp, channels;
  timer.start();
  while(enabled){
    //qDebug() << "progress: " << GetQHYCCDReadingProgress(handle) << ", remainingExposure: " << GetQHYCCDExposureRemaining(handle);
    result = GetQHYCCDLiveFrame(handle,&w,&h,&bpp,&channels,buffer);
    if(result != QHYCCD_SUCCESS) {
      qCritical() << "Error capturing live frame: " << result;
    } else {
      QtConcurrent::run(bind(&ImagingWorker::convert_image_data, this, QByteArray{reinterpret_cast<const char*>(buffer), w*h}, w, h, bpp, channels));
      frames++;
    }
    if(timer.elapsed() > 5000) {
      double elapsed = timer.elapsed();
      double fps = static_cast<double>(frames) / (elapsed / 1000);
      qDebug() << "frames:" << frames << ", elapsed: " << elapsed << "ms, fps: " << fps << ": " << w << "x" << h << "@" << bpp << ":" << channels;
      timer.restart();
      frames = 0;
    }
  };
}


void ImagingWorker::convert_image_data ( const QByteArray& data, int w, int h, int bpp, int channels )
{
  static list<double> elapsed;
  QElapsedTimer timer;
  timer.start();
  
  uchar *imgdata = new uchar[w*h*channels];
  memcpy(imgdata, data.constData(), data.size());
  QImage image(imgdata, w, h, QImage::Format_Grayscale8, [](void *data){ delete [] reinterpret_cast<uchar*>(data); }, imgdata);
  emit imager->gotImage(image);
  
  elapsed.push_back(timer.elapsed());
  if(elapsed.size() % 25 == 0) {
    double avg = accumulate(begin(elapsed), end(elapsed), 0);
    avg /= elapsed.size();
    qDebug() << "average image elab time: " << avg;
  }
}


void ImagingWorker::stop()
{
  enabled = false;
}


void QHYCCDImager::stopLive()
{
  d->worker->stop();
  d->imaging_thread.quit();
//   auto result = StopQHYCCDLive(d->handle);
//   if(result != QHYCCD_SUCCESS) {
//     throw QHYDriver::error("stop live", result);
//     return;
//   }
  qDebug() << "Live stopped correctly";
}


QDebug operator<<(QDebug dbg, const QHYCCDImager::Chip& chip)
{
  dbg.nospace() << "{ size: " << chip.width << "x" << chip.height << ", pixels size: " << chip.pixelwidth << "x" << chip.pixelheight <<
    ", image size: " << chip.xres << "x" << chip.yres << "@" << chip.bpp << "bpp }";
  return dbg.space();
}

QDebug operator<<(QDebug dbg, const QHYCCDImager::Setting& setting)
{
  dbg.nospace() << "{ name: " << setting.name << ", min: " << setting.min << ", max: " << setting.max << ", step: " << setting.step << ", value: " << setting.value << " }";
  return dbg.space();
}

#include "qhyccdimager.moc"

