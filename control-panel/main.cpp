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

#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "pipe.h"
#include "calibration.h"

/**
	 * @brief Starts the SARndbox control-panel application.
	 *
	 * Processes command-line options, displays an error dialog when requested, or
	 * loads the main QML control panel.
	 *
	 * @return The application exit status, or -1 if the QML interface fails to load.
	 */
	int main(int argc,char* argv[])
	{
	/* Writing to the command FIFO after the sandbox has exited raises SIGPIPE,
	   whose default disposition kills this process outright -- so quitting the
	   sandbox and then touching any control killed the panel instead of just
	   failing that one command. The sandbox ignores SIGPIPE in its own startup
	   for the same reason. With it ignored, write() returns EPIPE and Pipe::send
	   can do what it already intends: drop the descriptor and reconnect. */
	signal(SIGPIPE,SIG_IGN);

	QGuiApplication app(argc,argv);
	app.setApplicationName("SARndbox Control Panel");
	/* QSettings (behind QML's Settings type, used to persist slider positions)
	   needs an organization identifier to pick a config file path: */
	app.setOrganizationName("SARndbox");

	QCommandLineParser parser;
	parser.setApplicationDescription("Control panel for the Augmented Reality Sandbox");
	parser.addHelpOption();
	QCommandLineOption pipeOption(QStringList()<<"p"<<"pipe",
		"Path of the SARndbox control FIFO (must match the sandbox's -cp option).",
		"path","/tmp/sarndbox.pipe");
	parser.addOption(pipeOption);
	/* The panel reads the sandbox's own etc/ directory, so it has to look where
	   the sandbox actually is: $SANDBOX_DIR if the launcher set it, otherwise
	   the tree this binary was installed into (bin/sandbox-control -> ..).
	   qEnvironmentVariable() would do this in one call, but needs Qt 5.10;
	   qgetenv() has been there since Qt 4. */
	QString sandboxDir=QString::fromLocal8Bit(qgetenv("SANDBOX_DIR"));
	if(sandboxDir.isEmpty())
		sandboxDir=QDir::cleanPath(app.applicationDirPath()+"/..");
	QCommandLineOption dirOption(QStringList()<<"d"<<"sandbox-dir",
		"Directory the sandbox was built in.",
		"path",sandboxDir);
	parser.addOption(dirOption);
	QCommandLineOption errorOption("error",
		"Show the given message in an error dialog and exit, instead of starting the panel normally. "
		"Used by run-sandbox.sh to report a missing camera without a terminal to print to.",
		"message");
	parser.addOption(errorOption);
	parser.process(app);

	if(parser.isSet(errorOption))
		{
		/* Reuse QtQuick.Controls' own Popup type instead of pulling in QtWidgets
		   for a single message box - it is already a working, already-packaged
		   dependency of this app. Popup rather than Dialog: Dialog needs QQC2
		   2.3 (Qt 5.10+), which Ubuntu 18.04's Qt 5.9 does not have. Popup
		   itself goes back to QQC2's first version, but its own `anchors`
		   convenience property is also 5.10+, so it is centred with plain x/y
		   instead, which every version supports. */
		QQmlApplicationEngine engine;
		engine.rootContext()->setContextProperty("errorMessage",parser.value(errorOption));
		engine.loadData(R"(
			import QtQuick 2.9
			import QtQuick.Controls 2.2
			import QtQuick.Layouts 1.3

			ApplicationWindow {
				id: win
				width: 400
				height: 150
				visible: true
				title: "Augmented Reality Sandbox"

				Popup {
					x: (win.width - width) / 2
					y: (win.height - height) / 2
					modal: true
					closePolicy: Popup.CloseOnEscape

					ColumnLayout {
						width: parent.width
						height: parent.height
						spacing: 12
						Label {
							Layout.fillWidth: true
							text: "Augmented Reality Sandbox"
							font.bold: true
						}
						Label {
							Layout.fillWidth: true
							text: errorMessage
							wrapMode: Text.WordWrap
						}
						Button {
							Layout.alignment: Qt.AlignRight
							text: "OK"
							onClicked: close()
						}
					}

					onClosed: Qt.quit()
					Component.onCompleted: open()
				}
			}
			)");
		if(engine.rootObjects().isEmpty())
			return -1;
		return app.exec();
		}

	/* One panel per pipe, enforced here rather than by the callers. Readers of a
	   FIFO compete for its bytes rather than each seeing a copy, so a second
	   panel on the same pipe steals roughly half the sandbox's status lines from
	   the first: both then sit out the 2.5 s silence timeout at random and flap
	   between Connected and Waiting, which disables every control bound to
	   pipe.connected. They accumulate easily -- run-sandbox.sh starts a panel on
	   every launch, the sandbox's right-click menu forks one whenever it thinks
	   none is listening, and a panel outlives the sandbox it was started with
	   while showing no window until asked -- so an invisible leftover from an
	   earlier session is the normal way this happens.
	   The lock is released by the kernel when this process exits, so lockFd is
	   deliberately left open for the lifetime of the program. O_CLOEXEC keeps the
	   helpers started with QProcess (RawKinectViewer and friends) from holding
	   the lock after the panel is gone. If the lock file cannot be opened at all,
	   carry on unguarded rather than refusing to start. */
	const QString lockPath=parser.value(pipeOption)+".lock";
	const int lockFd=::open(lockPath.toLocal8Bit().constData(),O_RDWR|O_CREAT|O_CLOEXEC,0666);
	if(lockFd>=0&&::flock(lockFd,LOCK_EX|LOCK_NB)<0)
		{
		/* A panel is already listening on this pipe. It answers the sandbox's
		   showPanel event, so exiting quietly leaves the user with a working
		   panel rather than two half-fed ones. */
		return 0;
		}

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
