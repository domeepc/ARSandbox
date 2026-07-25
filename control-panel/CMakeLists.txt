cmake_minimum_required(VERSION 3.21)

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

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick)

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
	QML_FILES Main.qml ControlsPage.qml CalibrationPage.qml
)

target_link_libraries(sandbox-control PRIVATE Qt6::Quick)
