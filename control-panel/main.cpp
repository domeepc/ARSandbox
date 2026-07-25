/***********************************************************************
SARndbox control panel - a small Qt Quick front end that drives a running
Augmented Reality Sandbox through its existing control FIFO.

The sandbox needs no modification to be driven this way: it already parses
newline-terminated command lines from the pipe named by its -cp option.
Keeping the panel in a separate process means it cannot destabilise the
sandbox's Vrui main loop or OpenGL context, and either program can be
restarted independently of the other.
***********************************************************************/

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QDir>
#include <QUrl>

#include "pipe.h"
#include "calibration.h"

int main(int argc,char* argv[])
	{
	QGuiApplication app(argc,argv);
	app.setApplicationName("SARndbox Control Panel");

	QCommandLineParser parser;
	parser.setApplicationDescription("Control panel for the Augmented Reality Sandbox");
	parser.addHelpOption();
	QCommandLineOption pipeOption(QStringList()<<"p"<<"pipe",
		"Path of the SARndbox control FIFO (must match the sandbox's -cp option).",
		"path","/tmp/sarndbox.pipe");
	parser.addOption(pipeOption);
	QCommandLineOption dirOption(QStringList()<<"d"<<"sandbox-dir",
		"Directory the sandbox was built in.",
		"path",QDir::homePath()+"/src/SARndbox-2.8");
	parser.addOption(dirOption);
	parser.process(app);

	Pipe pipe(parser.value(pipeOption));
	Calibration calibration(parser.value(dirOption));

	QQmlApplicationEngine engine;
	engine.rootContext()->setContextProperty("pipe",&pipe);
	engine.rootContext()->setContextProperty("calibration",&calibration);
	engine.rootContext()->setContextProperty("pipePath",parser.value(pipeOption));
	/* Load the QML straight out of the binary's own resources rather than through
	   the module import path, which resolves against the build directory and so
	   only works when the executable is run from there. The sandbox starts this
	   program by name from its menu, so it has to work from anywhere. */
	engine.load(QUrl("qrc:/SandboxControl/Main.qml"));
	if(engine.rootObjects().isEmpty())
		return -1;

	return app.exec();
	}
