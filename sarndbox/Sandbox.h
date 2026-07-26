/***********************************************************************
Sandbox - Vrui application to drive an augmented reality sandbox.
Copyright (c) 2012-2019 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef SANDBOX_INCLUDED
#define SANDBOX_INCLUDED

#include <vector>
#include <string>
#include <Threads/Mutex.h>
#include <Threads/TripleBuffer.h>
#include <Geometry/Box.h>
#include <Geometry/PCACalculator.h>
#include <Geometry/Rotation.h>
#include <Geometry/OrthonormalTransformation.h>
#include <Geometry/ProjectiveTransformation.h>
#include <GL/gl.h>
#include <GL/GLColorMap.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/TextFieldSlider.h>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/Application.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>
#include <Kinect/DiskExtractor.h>
#include <Kinect/Config.h>
#include <Kinect/ProjectorType.h>
#include <Kinect/ProjectorHeader.h>

#include "Types.h"

/* Forward declarations: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}
class GLContextData;
namespace GLMotif {
class PopupMenu;
class PopupWindow;
class TextField;
}
namespace Vrui {
class Lightsource;
}
namespace Kinect {
class Camera;
}
class FrameFilter;
class DepthImageRenderer;
class ElevationColorMap;
class DEM;
class SurfaceRenderer;
class WaterTable2;
class HandExtractor;
typedef Misc::FunctionCall<GLContextData&> AddWaterFunction;
class RemoteServer;
class WaterRenderer;

class Sandbox:public Vrui::Application,public GLObject
	{
	/* Embedded classes: */
	private:
	typedef Geometry::Box<Scalar,3> Box; // Type for bounding boxes
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform; // Type for rigid body transformations
	typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Type for per-pixel depth correction factors
	
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		public:
		double waterTableTime; // Simulation time stamp of the water table in this OpenGL context
		GLsizei shadowBufferSize[2]; // Size of the shadow rendering frame buffer
		GLuint shadowFramebufferObject; // Frame buffer object to render shadow maps
		GLuint shadowDepthTextureObject; // Depth texture for the shadow rendering frame buffer

		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	struct GridRequest // Structure representing a request to read back bathymetry and/or water level grids from the GPU
		{
		/* Embedded classes: */
		public:
		typedef void (*CallbackFunction)(GLfloat*,GLfloat*,void*); // Type for callback functions
		
		struct Request // Structure holding a request's parameters
			{
			/* Elements: */
			public:
			GLfloat* bathymetryBuffer; // Pointer to a buffer to hold the requested bathymetry grid if requested
			GLfloat* waterLevelBuffer; // Pointer to a buffer to hold the requested water level grid if requested
			CallbackFunction callback; // Function to call when the grid(s) has/have been read back
			void* callbackData; // Additional data element to pass to callback function
			
			/* Constructors and destructors: */
			Request(void) // Creates an inactive request
				:bathymetryBuffer(0),waterLevelBuffer(0),callback(0),callbackData(0)
				{
				}
			
			/* Methods: */
			bool isActive(void) const // Returns true if there is a pending request
				{
				return callback!=0;
				}
			void complete(void) // Calls the read-back callback
				{
				(*callback)(bathymetryBuffer,waterLevelBuffer,callbackData);
				}
			};
		
		/* Elements: */
		Threads::Mutex mutex; // Mutex serializing access to the request structure
		Request currentRequest; // The currently pending grid request
		
		/* Constructors and destructors: */
		GridRequest(void) // Creates an inactive grid request
			{
			}
		
		/* Methods: */
		bool requestGrids(GLfloat* newBathymetryBuffer,GLfloat* newWaterLevelBuffer,CallbackFunction newCallback,void* newCallbackData) // Requests a grid read-back; returns true if request has been granted
			{
			Threads::Mutex::Lock lock(mutex);
			if(currentRequest.callback==0)
				{
				currentRequest.bathymetryBuffer=newBathymetryBuffer;
				currentRequest.waterLevelBuffer=newWaterLevelBuffer;
				currentRequest.callback=newCallback;
				currentRequest.callbackData=newCallbackData;
				return true;
				}
			else
				return false;
			}
		Request getRequest(void) // Returns the current grid request and deactivates it
			{
			Threads::Mutex::Lock lock(mutex);
			Request result=currentRequest;
			currentRequest.callback=0;
			return result;
			}
		};
	
	struct RenderSettings // Structure to hold per-window rendering settings
		{
		/* Elements: */
		public:
		bool fixProjectorView; // Flag whether to allow viewpoint navigation or always render from the projector's point of view
		PTransform projectorTransform; // The calibrated projector transformation matrix for fixed-projection rendering
		bool projectorTransformValid; // Flag whether the projector transformation is valid
		bool hillshade; // Flag whether to use augmented reality hill shading
		GLMaterial surfaceMaterial; // Material properties to render the surface in hill shading mode
		bool useShadows; // Flag whether to use shadows in augmented reality hill shading
		ElevationColorMap* elevationColorMap; // Pointer to an elevation color map
		bool useContourLines; // Flag whether to draw elevation contour lines
		GLfloat contourLineSpacing; // Spacing between adjacent contour lines in cm
		GLfloat contourLineWidth; // Width of contour lines in screen pixels
		GLfloat reliefStrength; // Strength of relief shading when hill shading is enabled
		bool renderWaterSurface; // Flag whether to render the water surface as a geometric surface
		GLfloat waterOpacity; // Opacity factor for water when rendered as texture
		SurfaceRenderer* surfaceRenderer; // Surface rendering object for this window
		WaterRenderer* waterRenderer; // A renderer to render the water surface as geometry
		
		/* Constructors and destructors: */
		RenderSettings(void); // Creates default rendering settings
		RenderSettings(const RenderSettings& source); // Copy constructor
		~RenderSettings(void); // Destroys rendering settings
		
		/* Methods: */
		void loadProjectorTransform(const char* projectorTransformName); // Loads a projector transformation from the given file
		void loadHeightMap(const char* heightMapName); // Loads the selected height map
		};
	
	friend class GlobalWaterTool;
	friend class LocalWaterTool;
	friend class DEMTool;
	friend class BathymetrySaverTool;
	friend class RemoteServer;
	
	/* Elements: */
	private:
	RemoteServer* remoteServer; // A server to stream bathymetry and water level grids to remote clients
	Kinect::FrameSource* camera; // The Kinect camera device
	unsigned int frameSize[2]; // Width and height of the camera's depth frames
	PixelDepthCorrection* pixelDepthCorrection; // Buffer of per-pixel depth correction coefficients
	Kinect::FrameSource::IntrinsicParameters cameraIps; // Intrinsic parameters of the Kinect camera
	FrameFilter* frameFilter; // Processing object to filter raw depth frames from the Kinect camera
	bool pauseUpdates; // Pauses updates of the topography
	Threads::TripleBuffer<Kinect::FrameBuffer> filteredFrames; // Triple buffer for incoming filtered depth frames
	DepthImageRenderer* depthImageRenderer; // Object managing the current filtered depth image
	Point basePlaneCorners[4]; // Corners of the sandbox area, as read from the layout file
	ONTransform boxTransform; // Transformation from camera space to baseplane space (x along long sandbox axis, z up)
	Scalar boxSize; // Radius of sphere around sandbox area
	Box bbox; // Bounding box around all potential surfaces
	WaterTable2* waterTable; // Water flow simulation object
	double waterSpeed; // Relative speed of water flow simulation
	unsigned int waterMaxSteps; // Maximum number of water simulation steps per frame
	GLfloat rainStrength; // Amount of water deposited by rain tools and objects on each water simulation step
	HandExtractor* handExtractor; // Object to detect splayed hands above the sand surface to make rain
	const AddWaterFunction* addWaterFunction; // Render function registered with the water table
	bool addWaterFunctionRegistered; // Flag if the water adding function is currently registered with the water table
	mutable GridRequest gridRequest; // Structure holding pending grid read-back requests
	std::vector<RenderSettings> renderSettings; // List of per-window rendering settings
	Vrui::Lightsource* sun; // An external fixed light source
	GLfloat sunAzimuth; // Azimuth of the fixed light source in degrees
	GLfloat sunElevation; // Elevation of the fixed light source in degrees above the horizon
	void setSunDirection(GLfloat azimuth,GLfloat elevation); // Points the fixed light source from the given azimuth and elevation
	DEM* activeDem; // The currently active DEM
	GLMotif::PopupMenu* mainMenu;
	GLMotif::ToggleButton* pauseUpdatesToggle;
	GLMotif::PopupWindow* waterControlDialog;
	GLMotif::TextFieldSlider* waterSpeedSlider;
	GLMotif::TextFieldSlider* waterMaxStepsSlider;
	GLMotif::TextField* frameRateTextField;
	GLMotif::TextFieldSlider* waterAttenuationSlider;
	int controlPipeFd; // File descriptor of an optional named pipe to send control commands to a running AR Sandbox
	std::string controlPipeName; // Name of the command pipe, kept so the panel can be started with it
	std::string controlPanelCommand; // Command used to start the control panel when it is not running
	std::string statusPipeName; // Name of an optional named pipe reporting state back to a control panel
	int statusPipeFd; // File descriptor of the status pipe, or -1 while no panel is listening
	double nextStatusTime; // Application time at which to send the next status update
	void sendStatus(void); // Writes the current state to the status pipe, if a panel is listening
	/* Base plane calibration. The sandbox measures its own geometry rather than
	   relying on RawKinectViewer and a hand-edited layout file: it fits a plane
	   to the sand surface over a run of filtered depth frames and writes the
	   result to the sandbox layout file. */
	Kinect::FrameBuffer lastFilteredFrame; // Most recent filtered depth frame, reused for the extents pass
	/* Sand extent capture. The four corners are measured by placing the disk
	   target on each corner of the box in turn, which is the only way to know
	   where the sand actually ends: fitting to the depth image cannot tell the
	   sandbox from the rest of the room. The base plane is then fitted to those
	   four points, so it describes the sand surface by construction. */
	Plane pendingPlane; // Plane fitted to a selected region, awaiting the corners
	bool pendingPlaneValid; // Flag whether a plane has been fitted since the last write
	Point pendingCorners[4]; // Corners extracted so far
	unsigned int numPendingCorners; // How many corners have been extracted
	std::string sandboxLayoutFileName; // Path of the layout file to read and write
	std::string calibrationMirrorDir; // Directory to copy written calibration files into, or empty
	void mirrorCalibrationFile(const std::string& fileName) const; // Copies a written calibration file into the mirror directory
	bool unprojectPixel(unsigned int x,unsigned int y,Point& result) const; // Turns a depth image pixel into a 3D camera space point
	void grabDepthImage(const char* fileName); // Writes the current filtered depth frame as a greyscale image for the panel to show
	void fitPlaneToRegion(unsigned int x0,unsigned int y0,unsigned int x1,unsigned int y1); // Fits the base plane to a rectangle of the depth image
	void setExtentsFromRegion(unsigned int x0,unsigned int y0,unsigned int x1,unsigned int y1); // Takes the four sand corners from a region, keeping the fitted plane
	void extractPoint(unsigned int x,unsigned int y); // Reports the 3D position of one depth image pixel and keeps it as a corner
	void writeSandboxLayout(void); // Writes the fitted plane and extracted corners to the layout file
	void resetLayoutCapture(void); // Discards a partly captured layout

	/* Projector calibration. Establishes the projective transform between camera
	   space and the projected image by showing a target at a known projector
	   position and finding a physical disk placed on it in the depth stream.
	   Ported from the standalone CalibrateProjector so the sandbox can be
	   calibrated in place, driven from the control panel. */
	struct TiePoint // A correspondence between projector image and camera space
		{
		public:
		Geometry::Point<double,2> p; // Position in projector image space
		Geometry::Point<double,3> o; // Position in 3D camera space
		};

	Kinect::DiskExtractor* diskExtractor; // Finds the calibration target in the depth stream
	Kinect::ProjectorType* calibrationFacade; // Renders the raw 3D video as the backdrop of the calibration view
	bool calibratingProjector; // Flag whether a projector calibration is in progress
	unsigned int projectorImageSize[2]; // Projector image size in pixels
	unsigned int tiePointIndex; // Index of the target currently being shown
	unsigned int numTiePoints; // Number of targets in the sequence
	std::vector<TiePoint> tiePoints; // Correspondences collected so far
	Threads::TripleBuffer<Kinect::DiskExtractor::Disk> lastDisk; // Most recently extracted disk
	bool haveDisk; // Flag whether a disk is currently visible
	bool diskEverSeenThisCalibration; // Flag whether a disk has been detected at least once since the current calibration started
	double lastDiskTime; // Application time at which a disk was last extracted
	std::string projectionMatrixFileName; // Path of the projector matrix file to write

	/* Depth frames fed to the disk extractor are averaged over a short ring
	   buffer to cut single-frame sensor noise, since the disk has to be held
	   still to be captured anyway: */
	static const unsigned int diskAveragingWindow=3;
	Kinect::FrameBuffer diskAveragingRing[diskAveragingWindow];
	unsigned int diskAveragingRingSize;
	unsigned int diskAveragingRingNext;
	Kinect::FrameBuffer averageDepthFrames(const Kinect::FrameBuffer& newFrame); // Pushes newFrame into the ring and returns the pixelwise average for disk extraction

	void diskExtractionCallback(const Kinect::DiskExtractor::DiskList& disks); // Receives extracted disks from the extractor thread
	#if !KINECT_CONFIG_USE_SHADERPROJECTOR
	void facadeMeshCallback(const Kinect::MeshBuffer& meshBuffer); // Wakes the main thread when the facade has a new mesh
	#endif
	Geometry::Point<double,2> getTiePointTarget(unsigned int index) const; // Returns the projector-space position of the given target
	void startProjectorCalibration(unsigned int width,unsigned int height,unsigned int tiePointCount); // Begins a projector calibration
	void captureTiePoint(void); // Records the currently visible disk against the current target
	void finishProjectorCalibration(void); // Solves for the projection matrix and writes it
	void abortProjectorCalibration(void); // Cancels a calibration in progress
	void restoreDepthStream(void); // Turns background removal back off after a calibration
	void drawCalibrationView(GLContextData& contextData,const int viewport[4]) const; // Draws the sandbox area and the extracted disk in place of the topography during calibration

	bool sendEvent(const char* event); // Sends a one-word event to the control panel; returns whether it was delivered
	void showPanelCallback(Misc::CallbackData* cbData); // Asks the control panel to raise itself
	void showCalibrationCallback(Misc::CallbackData* cbData); // Asks the control panel to open the calibration dialog
	
	/* Private methods: */
	void rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer); // Callback receiving raw depth frames from the Kinect camera; forwards them to the frame filter and rain maker objects
	void receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer); // Callback receiving filtered depth frames from the filter object
	void toggleDEM(DEM* dem); // Sets or toggles the currently active DEM
	void addWater(GLContextData& contextData) const; // Function to render geometry that adds water to the water table
	void pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void showWaterControlDialogCallback(Misc::CallbackData* cbData);
	void waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	GLMotif::PopupMenu* createMainMenu(void);
	GLMotif::PopupWindow* createWaterControlDialog(void);
	
	/* Constructors and destructors: */
	public:
	Sandbox(int& argc,char**& argv);
	virtual ~Sandbox(void);
	
	/* Methods from Vrui::Application: */
	virtual void toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	virtual void eventCallback(EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData);
	
	/* Methods from GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	};

#endif
