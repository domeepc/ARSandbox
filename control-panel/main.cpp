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
