/***********************************************************************
Pipe - Writes newline-terminated commands to the SARndbox control FIFO.

SARndbox opens its control pipe with O_RDONLY|O_NONBLOCK and polls it from
its frame loop. It does not create the FIFO, so one must exist before the
sandbox is launched, and the writer must hold its end open for the whole
session: opening and closing per command would make the reader see repeated
end-of-file.

Communication is one-way. The sandbox never reports its state back, so this
panel can only show what it last sent.
ponytail: the ceiling is that the panel's values drift from the sandbox's
actual state if someone also uses the in-application GLMotif dialogs. The
upgrade path is a second FIFO carrying state back, which is not worth
building until that drift actually becomes a problem in use.
***********************************************************************/

#ifndef PIPE_H
#define PIPE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QElapsedTimer>

class QTimer;
class QSocketNotifier;

class Pipe:public QObject
	{
	Q_OBJECT
	Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

	public:
	explicit Pipe(const QString& path,QObject* parent=nullptr);
	~Pipe() override;

	/* Connected means the sandbox is actually alive, judged by whether a status
	   line has arrived recently -- not by whether a descriptor is open. A FIFO
	   write descriptor stays valid after the reader exits, so an open descriptor
	   is not evidence of a peer; the sandbox pushes status twice a second, so
	   silence is. */
	bool connected() const {return alive;}

	/* Sends a command line to the sandbox. Returns false if it could not be
	   delivered, in which case the panel drops back to reconnecting. */
	Q_INVOKABLE bool send(const QString& command);

	signals:
	void connectedChanged();

	/* One "<key> <value>" status line from the sandbox. */
	void status(const QString& key,const QString& value);

	/* The sandbox asked the panel to raise itself. */
	void showRequested();

	/* The sandbox asked the panel to open the calibration dialog. */
	void showCalibrationRequested();

	/* The sandbox's right click asked for the context menu. */
	void showMenuRequested();

	private slots:
	void tryOpen();
	void readStatus();
	void checkAlive();

	private:
	void close();
	void closeStatus();
	void setAlive(bool newAlive);

	QString path;
	int fd;
	QTimer* retryTimer;

	bool alive; // Whether a status line has arrived recently
	QElapsedTimer sinceStatus; // Time since the last status line

	/* The sandbox reports back on a second FIFO alongside the command one, so
	   neither side needs a separate option for its path. */
	int statusFd;
	QSocketNotifier* statusNotifier;
	QByteArray statusBuffer;
	};

#endif
