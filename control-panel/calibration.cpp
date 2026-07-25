#include "calibration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QTextStream>

#include <cmath>

namespace {

const QString kinectConfigDir="/usr/local/etc/Vrui-8.0/Kinect-3.10";

double dot(const double a[3],const double b[3])
	{
	return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
	}

double distance(const double a[3],const double b[3])
	{
	double d=0.0;
	for(int i=0;i<3;++i)
		d+=(a[i]-b[i])*(a[i]-b[i]);
	return std::sqrt(d);
	}

}

Calibration::Calibration(const QString& sSandboxDir,QObject* parent)
	:QObject(parent),sandboxDir(sSandboxDir),
	 planeOk(false),offset(0.0),tilt(0.0),width(0.0),height(0.0),residual(0.0),mismatch(0.0)
	{
	normal[0]=normal[1]=normal[2]=0.0;
	readBoxLayout();
	}

bool Calibration::intrinsicsDone() const
	{
	/* Vrui's Kinect package writes one file per camera serial number, so any
	   match counts rather than a fixed name: */
	QDir dir(kinectConfigDir);
	return !dir.entryList(QStringList()<<"IntrinsicParameters-*.dat",QDir::Files).isEmpty();
	}

QString Calibration::intrinsicsSerial() const
	{
	QDir dir(kinectConfigDir);
	const QStringList files=dir.entryList(QStringList()<<"IntrinsicParameters-*.dat",QDir::Files);
	if(files.isEmpty())
		return QString();

	/* The serial number is part of the file name, and is worth surfacing because
	   the file is specific to one camera and does not transfer to another. */
	QString serial=files.first();
	serial.remove("IntrinsicParameters-");
	serial.remove(".dat");
	return serial;
	}

bool Calibration::projectorDone() const
	{
	return QFileInfo::exists(sandboxDir+"/etc/SARndbox-2.8/ProjectorMatrix.dat");
	}

QString Calibration::projectorDate() const
	{
	QFileInfo info(sandboxDir+"/etc/SARndbox-2.8/ProjectorMatrix.dat");
	if(!info.exists())
		return QString();
	return info.lastModified().toString("yyyy-MM-dd HH:mm");
	}

void Calibration::readBoxLayout()
	{
	planeOk=false;
	offset=tilt=width=height=residual=mismatch=0.0;

	QFile file(sandboxDir+"/etc/SARndbox-2.8/BoxLayout.txt");
	if(!file.open(QIODevice::ReadOnly|QIODevice::Text))
		return;

	/* The file is a plane equation "(nx, ny, nz) , d" followed by four corner
	   points "(x, y, z)". Pulling the numbers out directly is more robust to the
	   irregular whitespace RawKinectViewer writes than field-by-field parsing. */
	static const QRegularExpression number("[-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?");

	QVector<double> values;
	QTextStream in(&file);
	while(!in.atEnd())
		{
		QRegularExpressionMatchIterator it=number.globalMatch(in.readLine());
		while(it.hasNext())
			values<<it.next().captured().toDouble();
		}

	/* 4 for the plane, 3 for each of 4 corners: */
	if(values.size()<16)
		return;

	for(int i=0;i<3;++i)
		normal[i]=values[i];
	offset=values[3];

	const double length=std::sqrt(dot(normal,normal));
	if(length<=0.0)
		return;
	for(int i=0;i<3;++i)
		normal[i]/=length;
	offset/=length;

	double corner[4][3];
	for(int c=0;c<4;++c)
		for(int i=0;i<3;++i)
			corner[c][i]=values[4+c*3+i];

	/* The camera looks nearly straight down the z axis, so a correct base plane
	   normal is nearly parallel to it: */
	tilt=std::acos(std::min(1.0,std::fabs(normal[2])))*180.0/M_PI;

	/* Corners lie in the sand surface, so their distance to the plane measures
	   how well the two measurements agree: */
	residual=0.0;
	for(int c=0;c<4;++c)
		residual=std::max(residual,std::fabs(dot(normal,corner[c])-offset));

	/* Corner order is lower left, lower right, upper left, upper right: */
	const double bottom=distance(corner[0],corner[1]);
	const double top=distance(corner[2],corner[3]);
	const double left=distance(corner[0],corner[2]);
	const double right=distance(corner[1],corner[3]);
	width=(bottom+top)*0.5;
	height=(left+right)*0.5;

	const double widthMismatch=width>0.0?std::fabs(bottom-top)/width*100.0:0.0;
	const double heightMismatch=height>0.0?std::fabs(left-right)/height*100.0:0.0;
	mismatch=std::max(widthMismatch,heightMismatch);

	planeOk=true;
	}

QVariantList Calibration::planeNormal() const
	{
	QVariantList list;
	for(int i=0;i<3;++i)
		list<<normal[i];
	return list;
	}

QVariantList Calibration::screens() const
	{
	QVariantList list;
	const QList<QScreen*> all=QGuiApplication::screens();
	for(QScreen* screen:all)
		{
		const QSize s=screen->size();
		QVariantMap entry;
		entry["label"]=QString("%1  %2 x %3").arg(screen->name()).arg(s.width()).arg(s.height());
		entry["width"]=s.width();
		entry["height"]=s.height();
		list<<entry;
		}
	return list;
	}

void Calibration::refresh()
	{
	readBoxLayout();
	emit changed();
	}

bool Calibration::launch(const QString& program,const QStringList& arguments)
	{
	/* Detached, because these are interactive tools the user works with for
	   minutes at a time and the panel must stay responsive meanwhile: */
	if(QProcess::startDetached(program,arguments,sandboxDir))
		return true;

	emit launchFailed(QString("Could not start %1").arg(program));
	return false;
	}

bool Calibration::runKinectUtil()
	{
	return launch("KinectUtil",QStringList()<<"getCalib"<<"0");
	}

bool Calibration::runRawKinectViewer()
	{
	return launch("RawKinectViewer",QStringList()<<"-compress"<<"0");
	}

bool Calibration::runCalibrateProjector(int width,int height)
	{
	return launch(sandboxDir+"/bin/CalibrateProjector",
	              QStringList()<<"-s"<<QString::number(width)<<QString::number(height));
	}
