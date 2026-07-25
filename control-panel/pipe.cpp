#include "pipe.h"

#include <QTimer>
#include <QDebug>
#include <QSocketNotifier>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

namespace {

/* How often to retry opening the FIFO while the sandbox is not running: */
constexpr int retryIntervalMs=1000;

}

Pipe::Pipe(const QString& sPath,QObject* parent)
	:QObject(parent),path(sPath),fd(-1),retryTimer(new QTimer(this)),
	 statusFd(-1),statusNotifier(nullptr)
	{
	connect(retryTimer,&QTimer::timeout,this,&Pipe::tryOpen);
	retryTimer->start(retryIntervalMs);

	/* Attempt an immediate connection so a panel started after the sandbox is
	   usable straight away: */
	tryOpen();
	}

Pipe::~Pipe()
	{
	close();
	closeStatus();
	}

void Pipe::tryOpen()
	{
	/* The status pipe is independent of the command pipe: the sandbox may be
	   reporting state before the panel has anything to send, and either
	   direction can drop without the other. */
	if(statusFd<0)
		{
		/* O_RDONLY|O_NONBLOCK on a FIFO succeeds even with no writer, so this
		   also covers the sandbox not being started yet. */
		statusFd=::open((path+".status").toLocal8Bit().constData(),O_RDONLY|O_NONBLOCK);
		if(statusFd>=0)
			{
			statusNotifier=new QSocketNotifier(statusFd,QSocketNotifier::Read,this);
			connect(statusNotifier,&QSocketNotifier::activated,this,&Pipe::readStatus);
			}
		}

	if(fd>=0)
		return;

	/* O_NONBLOCK matters here: opening a FIFO for writing blocks until a reader
	   exists, which would freeze the panel's event loop whenever the sandbox is
	   not running. With O_NONBLOCK the call fails with ENXIO instead, which is
	   the normal "sandbox not started yet" case and not worth logging. */
	int newFd=::open(path.toLocal8Bit().constData(),O_WRONLY|O_NONBLOCK);
	if(newFd<0)
		{
		if(errno!=ENXIO&&errno!=ENOENT)
			qWarning()<<"Pipe: cannot open"<<path<<":"<<strerror(errno);
		return;
		}

	fd=newFd;
	emit connectedChanged();
	}

void Pipe::close()
	{
	if(fd<0)
		return;

	::close(fd);
	fd=-1;
	emit connectedChanged();
	}

void Pipe::closeStatus()
	{
	if(statusFd<0)
		return;

	delete statusNotifier;
	statusNotifier=nullptr;
	::close(statusFd);
	statusFd=-1;
	statusBuffer.clear();
	}

void Pipe::readStatus()
	{
	char chunk[1024];
	ssize_t got=::read(statusFd,chunk,sizeof(chunk));
	if(got==0)
		{
		/* End of file: the sandbox closed its end. Reopen so a restarted sandbox
		   is picked up again, rather than leaving a notifier spinning on EOF. */
		closeStatus();
		return;
		}
	if(got<0)
		{
		if(errno!=EAGAIN)
			closeStatus();
		return;
		}

	statusBuffer.append(chunk,got);

	/* Process whole lines only; a write can be split across reads. */
	int newline;
	while((newline=statusBuffer.indexOf('\n'))>=0)
		{
		const QString line=QString::fromLocal8Bit(statusBuffer.left(newline)).trimmed();
		statusBuffer.remove(0,newline+1);
		if(line.isEmpty())
			continue;

		if(line=="showPanel")
			{
			emit showRequested();
			continue;
			}
		if(line=="showCalibration")
			{
			emit showCalibrationRequested();
			continue;
			}

		const int space=line.indexOf(' ');
		if(space>0)
			emit status(line.left(space),line.mid(space+1));
		}
	}

bool Pipe::send(const QString& command)
	{
	if(fd<0)
		return false;

	const QByteArray line=command.toLocal8Bit()+'\n';
	ssize_t written=::write(fd,line.constData(),line.size());
	if(written==line.size())
		return true;

	/* A full pipe (EAGAIN) means the sandbox is not draining commands; a broken
	   pipe (EPIPE) means it exited. Either way, drop back to reconnecting rather
	   than pretending the command landed. */
	if(written<0&&errno==EAGAIN)
		{
		qWarning()<<"Pipe: sandbox is not reading commands; dropped:"<<command;
		return false;
		}

	close();
	return false;
	}
