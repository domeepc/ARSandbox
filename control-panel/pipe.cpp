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

/* Silence longer than this means the sandbox is gone. The sandbox sends status
   every 500 ms, so this allows a couple of missed updates before reporting a
   disconnection. */
constexpr int statusTimeoutMs=2500;

}

Pipe::Pipe(const QString& sPath,QObject* parent)
	:QObject(parent),path(sPath),fd(-1),retryTimer(new QTimer(this)),
	 statusFd(-1),statusNotifier(nullptr),alive(false)
	{
	connect(retryTimer,&QTimer::timeout,this,&Pipe::tryOpen);
	connect(retryTimer,&QTimer::timeout,this,&Pipe::checkAlive);
	retryTimer->start(retryIntervalMs);
	sinceStatus.start();

	/* Attempt an immediate connection so a panel started after the sandbox is
	   usable straight away: */
	tryOpen();
	}

void Pipe::setAlive(bool newAlive)
	{
	if(alive==newAlive)
		return;
	alive=newAlive;
	emit connectedChanged();
	}

void Pipe::checkAlive()
	{
	/* The sandbox sends status twice a second, so a couple of seconds of silence
	   means it is gone -- whatever the descriptors say. */
	if(alive&&sinceStatus.elapsed()>statusTimeoutMs)
		{
		setAlive(false);

		/* Drop the write side too, so the next send reopens against a restarted
		   sandbox rather than writing into a dead pipe. */
		close();
		}
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
		/* O_CLOEXEC matters: the panel launches RawKinectViewer and friends with
		   QProcess, and without it every one of those children inherits these
		   descriptors and keeps the FIFO open after the panel exits. The sandbox
		   then sees a reader on its status pipe, concludes a panel is listening,
		   and silently stops launching one. */
		statusFd=::open((path+".status").toLocal8Bit().constData(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);
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
	int newFd=::open(path.toLocal8Bit().constData(),O_WRONLY|O_NONBLOCK|O_CLOEXEC);
	if(newFd<0)
		{
		if(errno!=ENXIO&&errno!=ENOENT)
			qWarning()<<"Pipe: cannot open"<<path<<":"<<strerror(errno);
		return;
		}

	/* Opening the write side says nothing about whether the sandbox is alive, so
	   it does not change the connected state; only arriving status does. */
	fd=newFd;
	}

void Pipe::close()
	{
	if(fd<0)
		return;

	::close(fd);
	fd=-1;
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

	/* Traffic is the only reliable evidence that the sandbox is running: */
	sinceStatus.restart();
	setAlive(true);

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
		if(line=="showMenu")
			{
			emit showMenuRequested();
			continue;
			}

		/* A status line may be a bare word with no value -- layoutReset is one.
		   Dropping those silently meant the panel never heard about them. */
		const int space=line.indexOf(' ');
		if(space>0)
			emit status(line.left(space),line.mid(space+1));
		else
			emit status(line,QString());
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
