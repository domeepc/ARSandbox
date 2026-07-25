/***********************************************************************
Calibration - Reports the state of the three calibration steps, checks the
measured base plane for plausibility, and launches the existing Vrui tools
that perform the measurements.

This deliberately does NOT reimplement the calibration capture itself.
CalibrateProjector has to run full screen on the projector, read the Kinect
depth stream, detect a physical disk target by blob extraction and solve for
the projective transform; rebuilding that in QML would mean rebuilding the
precision-critical part of the system for no gain. What is actually awkward
about calibration is remembering the step order, supplying the correct
projector pixel size, and noticing when a measurement came out wrong -- so
that is what this covers.
***********************************************************************/

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class Calibration:public QObject
	{
	Q_OBJECT

	/* Step completion, taken from whether each step's output file exists --
	   which is also what the sandbox itself checks at startup. */
	Q_PROPERTY(bool intrinsicsDone READ intrinsicsDone NOTIFY changed)
	Q_PROPERTY(bool boxLayoutDone READ boxLayoutDone NOTIFY changed)
	Q_PROPERTY(bool projectorDone READ projectorDone NOTIFY changed)
	Q_PROPERTY(QString intrinsicsSerial READ intrinsicsSerial NOTIFY changed)
	Q_PROPERTY(QString projectorDate READ projectorDate NOTIFY changed)

	/* Measured base plane and its plausibility checks. */
	Q_PROPERTY(bool planeValid READ planeValid NOTIFY changed)
	Q_PROPERTY(QVariantList planeNormal READ planeNormal NOTIFY changed)
	Q_PROPERTY(double planeOffset READ planeOffset NOTIFY changed)
	Q_PROPERTY(double planeTilt READ planeTilt NOTIFY changed)
	Q_PROPERTY(double boxWidth READ boxWidth NOTIFY changed)
	Q_PROPERTY(double boxHeight READ boxHeight NOTIFY changed)
	Q_PROPERTY(double maxResidual READ maxResidual NOTIFY changed)
	Q_PROPERTY(double sideMismatch READ sideMismatch NOTIFY changed)

	Q_PROPERTY(QVariantList screens READ screens CONSTANT)

	public:
	explicit Calibration(const QString& sandboxDir,QObject* parent=nullptr);

	bool intrinsicsDone() const;
	bool boxLayoutDone() const {return planeOk;}
	bool projectorDone() const;
	QString intrinsicsSerial() const;
	QString projectorDate() const;

	bool planeValid() const {return planeOk;}
	QVariantList planeNormal() const;
	double planeOffset() const {return offset;}

	/* Angle in degrees between the base plane normal and the camera's optical
	   axis. The camera looks almost straight down at the sand, so a correct
	   measurement is a few degrees; a large value means the plane fit caught a
	   wall of the box or some other surface that is not the sand. */
	double planeTilt() const {return tilt;}

	double boxWidth() const {return width;}
	double boxHeight() const {return height;}

	/* Largest distance of a measured corner from the measured plane, in cm.
	   The corners lie in the sand surface by definition, so this should be
	   within the noise of hand-picking points on a Kinect v1 depth image. */
	double maxResidual() const {return residual;}

	/* Relative difference between opposite sides of the corner quadrilateral,
	   as a percentage. The sandbox is rectangular, so a large value means at
	   least one corner was picked inaccurately. */
	double sideMismatch() const {return mismatch;}

	QVariantList screens() const;

	Q_INVOKABLE void refresh();
	Q_INVOKABLE bool runKinectUtil();
	Q_INVOKABLE bool runRawKinectViewer();
	Q_INVOKABLE bool runCalibrateProjector(int width,int height);

	signals:
	void changed();
	void launchFailed(const QString& message);

	private:
	bool launch(const QString& program,const QStringList& arguments);
	void readBoxLayout();

	QString sandboxDir;

	bool planeOk;
	double normal[3];
	double offset;
	double tilt;
	double width;
	double height;
	double residual;
	double mismatch;
	};

#endif
