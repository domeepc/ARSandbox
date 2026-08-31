# 3.10 rather than something newer: it's what Ubuntu 18.04 (still targeted by
# the Qt 5 fallback branch below) actually ships, and nothing here needs more.
cmake_minimum_required(VERSION 3.10)

project(SandboxControl VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt is installed through the Qt online installer and is not on the default
# search path; point CMAKE_PREFIX_PATH at it, or pass -DCMAKE_PREFIX_PATH on the
# command line to use a different Qt.
if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "$ENV{HOME}/Qt/6.11.1/gcc_64")
	set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.1/gcc_64")
endif()

# Qt 6 is preferred; some still-supported distributions (Ubuntu 20.04 and
# older) never packaged it, so fall back to Qt 5 there - see install.sh's own
# qt6-base-dev/qtbase5-dev fallback. Detection has to happen here rather than
# being passed in, since packaging/build.sh runs unmodified against whichever
# apt packages the caller installed.
find_package(Qt6 6.4 QUIET COMPONENTS Quick)
if(Qt6_FOUND)
	qt_standard_project_setup(REQUIRES 6.4)

	qt_add_executable(sandbox-control
		main.cpp
		pipe.cpp
		pipe.h
		calibration.cpp
		calibration.h
	)

	qt_add_qml_module(sandbox-control
		URI SandboxControl
		VERSION 1.0
		QML_FILES Main.qml ControlsPage.qml WaterPage.qml CalibrationPage.qml DepthPicker.qml
			Setting.qml Section.qml Heading.qml Note.qml Step.qml Check.qml
	)

	target_link_libraries(sandbox-control PRIVATE Qt6::Quick)
else()
	# 5.9 rather than something newer: it's what Ubuntu 18.04 ships, and the
	# QML no longer uses anything past it now that inline `component` blocks
	# (needing 5.15) have been split out into their own files below.
	find_package(Qt5 5.9 REQUIRED COMPONENTS Quick)

	# Qt 5's CMake API predates qt_add_qml_module (that build system arrived
	# with Qt 6), so the QML files are embedded through a plain .qrc instead.
	# Its resource paths mirror qt_add_qml_module's own SandboxControl/ prefix,
	# so main.cpp's qrc:/SandboxControl/Main.qml load works unmodified either
	# way.
	qt5_add_resources(QML_RESOURCES qml.qrc)

	add_executable(sandbox-control
		main.cpp
		pipe.cpp
		pipe.h
		calibration.cpp
		calibration.h
		${QML_RESOURCES}
	)

	target_link_libraries(sandbox-control PRIVATE Qt5::Quick)
endif()
