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

#include "Sandbox.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>
#include <sstream>
#include <fstream>
#include <Math/Matrix.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <Misc/SizedTypes.h>
#include <Misc/SelfDestructPointer.h>
#include <Misc/FixedArray.h>
#include <Misc/FunctionCalls.h>
#include <Misc/FileTests.h>
#include <Misc/MessageLogger.h>
#include <Misc/FileNameExtensions.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ArrayValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/File.h>
#include <IO/ValueSource.h>
#include <IO/OpenFile.h>
#include <Comm/OpenPipe.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Interval.h>
#include <Math/MathValueCoders.h>
#include <Geometry/Point.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/HVector.h>
#include <Geometry/Plane.h>
#include <Geometry/LinearUnit.h>
#include <Geometry/GeometryValueCoders.h>
#include <Geometry/OutputOperators.h>
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLColorMap.h>
#include <GL/GLLightTracker.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBDepthTexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/GLContextData.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/Menu.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/Margin.h>
#include <GLMotif/Label.h>
#include <GLMotif/TextField.h>
#include <Vrui/Vrui.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Lightsource.h>
#include <Vrui/LightsourceManager.h>
#include <Vrui/Viewer.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>
#include <Kinect/FileFrameSource.h>
#include <Kinect/MultiplexedFrameSource.h>
#include <Kinect/DirectFrameSource.h>
#include <Kinect/OpenDirectFrameSource.h>

#define SAVEDEPTH 0

#if SAVEDEPTH
#include <Images/RGBImage.h>
#include <Images/WriteImageFile.h>
#endif

#include "FrameFilter.h"
#include "DepthImageRenderer.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "SurfaceRenderer.h"
#include "WaterTable2.h"
#include "HandExtractor.h"
#include "RemoteServer.h"
#include "WaterRenderer.h"
#include "GlobalWaterTool.h"
#include "LocalWaterTool.h"
#include "DEMTool.h"
#include "BathymetrySaverTool.h"

#include "Config.h"

/**********************************
Methods of class Sandbox::DataItem:
**********************************/

Sandbox::DataItem::DataItem(void)
	:waterTableTime(0.0),
	 shadowFramebufferObject(0),shadowDepthTextureObject(0)
	{
	/* Check if all required extensions are supported: */
	bool supported=GLEXTFramebufferObject::isSupported();
	supported=supported&&GLARBTextureRectangle::isSupported();
	supported=supported&&GLARBTextureFloat::isSupported();
	supported=supported&&GLARBTextureRg::isSupported();
	supported=supported&&GLARBDepthTexture::isSupported();
	supported=supported&&GLARBShaderObjects::isSupported();
	supported=supported&&GLARBVertexShader::isSupported();
	supported=supported&&GLARBFragmentShader::isSupported();
	supported=supported&&GLARBMultitexture::isSupported();
	if(!supported)
		Misc::throwStdErr("Sandbox: Not all required extensions are supported by local OpenGL");
	
	/* Initialize all required extensions: */
	GLEXTFramebufferObject::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRg::initExtension();
	GLARBDepthTexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBVertexShader::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	}

Sandbox::DataItem::~DataItem(void)
	{
	/* Delete all shaders, buffers, and texture objects: */
	glDeleteFramebuffersEXT(1,&shadowFramebufferObject);
	glDeleteTextures(1,&shadowDepthTextureObject);
	}

/****************************************
Methods of class Sandbox::RenderSettings:
****************************************/

Sandbox::RenderSettings::RenderSettings(void)
	:fixProjectorView(false),projectorTransform(PTransform::identity),projectorTransformValid(false),
	 hillshade(false),surfaceMaterial(GLMaterial::Color(1.0f,1.0f,1.0f)),
	 useShadows(false),
	 elevationColorMap(0),
	 useContourLines(true),contourLineSpacing(0.75f),contourLineWidth(1.6f),reliefStrength(0.35f),
	 renderWaterSurface(false),waterOpacity(2.0f),
	 surfaceRenderer(0),waterRenderer(0)
	{
	/* Load the default projector transformation: */
	loadProjectorTransform(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);
	}

Sandbox::RenderSettings::RenderSettings(const Sandbox::RenderSettings& source)
	:fixProjectorView(source.fixProjectorView),projectorTransform(source.projectorTransform),projectorTransformValid(source.projectorTransformValid),
	 hillshade(source.hillshade),surfaceMaterial(source.surfaceMaterial),
	 useShadows(source.useShadows),
	 elevationColorMap(source.elevationColorMap!=0?new ElevationColorMap(*source.elevationColorMap):0),
	 useContourLines(source.useContourLines),contourLineSpacing(source.contourLineSpacing),contourLineWidth(source.contourLineWidth),reliefStrength(source.reliefStrength),
	 renderWaterSurface(source.renderWaterSurface),waterOpacity(source.waterOpacity),
	 surfaceRenderer(0),waterRenderer(0)
	{
	}

Sandbox::RenderSettings::~RenderSettings(void)
	{
	delete surfaceRenderer;
	delete waterRenderer;
	delete elevationColorMap;
	}

void Sandbox::RenderSettings::loadProjectorTransform(const char* projectorTransformName)
	{
	std::string fullProjectorTransformName;
	try
		{
		/* Open the projector transformation file: */
		if(projectorTransformName[0]=='/')
			{
			/* Use the absolute file name directly: */
			fullProjectorTransformName=projectorTransformName;
			}
		else
			{
			/* Assemble a file name relative to the configuration file directory: */
			fullProjectorTransformName=CONFIG_CONFIGDIR;
			fullProjectorTransformName.push_back('/');
			fullProjectorTransformName.append(projectorTransformName);
			}
		IO::FilePtr projectorTransformFile=IO::openFile(fullProjectorTransformName.c_str(),IO::File::ReadOnly);
		projectorTransformFile->setEndianness(Misc::LittleEndian);
		
		/* Read the projector transformation matrix from the binary file: */
		Misc::Float64 pt[16];
		projectorTransformFile->read(pt,16);
		projectorTransform=PTransform::fromRowMajor(pt);
		
		projectorTransformValid=true;
		}
	catch(const std::runtime_error& err)
		{
		/* Not having a projector calibration yet is the normal state of a new
		   sandbox, not a fault: the application simply falls back to the default
		   projection. Say so plainly rather than printing an exception, which
		   reads like something broke. */
		if(!Misc::doesPathExist(fullProjectorTransformName.c_str()))
			std::cout<<"No projector calibration yet; using the default projection. "
			         <<"Run the projector calibration from the control panel to create "
			         <<fullProjectorTransformName<<"."<<std::endl;
		else
			std::cerr<<"Unable to load projector transformation from file "<<fullProjectorTransformName<<" due to exception "<<err.what()<<std::endl;
		projectorTransformValid=false;
		}
	}

void Sandbox::RenderSettings::loadHeightMap(const char* heightMapName)
	{
	try
		{
		/* Load the elevation color map of the given name: */
		ElevationColorMap* newElevationColorMap=new ElevationColorMap(heightMapName);
		
		/* Delete the previous elevation color map and assign the new one: */
		delete elevationColorMap;
		elevationColorMap=newElevationColorMap;
		}
	catch(const std::runtime_error& err)
		{
		std::cerr<<"Ignoring height map due to exception "<<err.what()<<std::endl;
		}
	}

/************************
Methods of class Sandbox:
************************/

namespace {

/* Set by writeSandboxLayout() and consumed by restartIfRequested(), which is
   registered with atexit() so it runs after Sandbox's destructor already has
   -- camera closed, threads stopped -- making it safe to replace the process
   image. A plain flag rather than calling execv() directly from
   writeSandboxLayout(): the water table (and everything sized from it, like
   the tool factories that cache its pointer and grid dimensions at startup)
   can only safely pick up a new domain by being rebuilt from scratch, and the
   normal Vrui::shutdown()/app-object-destruction path is what already does
   that cleanly, once, for every member -- duplicating it by hand here would
   be the same list of teardown steps going stale independently. */
bool restartRequested=false;

void restartIfRequested(void)
	{
	if(!restartRequested)
		return;

	/* Read the running binary's path and original arguments from /proc rather
	   than anything captured earlier: Vrui::Application's constructor may have
	   already stripped its own arguments from the argc/argv Sandbox was given. */
	char exePath[4096];
	ssize_t exeLen=readlink("/proc/self/exe",exePath,sizeof(exePath)-1);
	if(exeLen<0)
		{
		std::cerr<<"Unable to restart: readlink(/proc/self/exe) failed: "<<strerror(errno)<<std::endl;
		return;
		}
	exePath[exeLen]='\0';

	std::ifstream cmdlineFile("/proc/self/cmdline",std::ios::binary);
	std::string cmdline((std::istreambuf_iterator<char>(cmdlineFile)),std::istreambuf_iterator<char>());

	std::vector<std::string> argStrings;
	std::string::size_type start=0;
	while(start<cmdline.size())
		{
		std::string::size_type end=cmdline.find('\0',start);
		if(end==std::string::npos)
			end=cmdline.size();
		argStrings.push_back(cmdline.substr(start,end-start));
		start=end+1;
		}

	std::vector<char*> execArgs;
	for(std::string& a:argStrings)
		execArgs.push_back(&a[0]);
	execArgs.push_back(0);

	execv(exePath,execArgs.data());

	/* Only reached if execv itself failed to start: */
	std::cerr<<"Unable to restart: execv failed: "<<strerror(errno)<<std::endl;
	}

}

void Sandbox::rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Feed the calibration target extractor, which needs raw depth rather than
	   the filtered surface. Only while calibrating: the extractor thread only
	   exists between startStreaming/stopStreaming anyway, so this is purely an
	   efficiency check. The frame is averaged over a short window first since a
	   single Kinect v1 frame is noisy enough to fragment a flat disk into
	   multiple blobs, and the disk has to be held still to be captured anyway. */
	if(diskExtractor!=0&&calibratingProjector)
		{
		diskExtractor->submitFrame(averageDepthFrames(frameBuffer));
		calibrationFacade->setDepthFrame(frameBuffer);
		}

	/* Pass the received frame to the frame filter and the hand extractor. Not
	   while calibrating: the frames are background-removed then, and feeding
	   blanked sand into the elevation history would leave the surface to
	   re-converge afterwards. The topography is not on screen during a
	   calibration anyway. */
	if(frameFilter!=0&&!pauseUpdates&&!calibratingProjector)
		frameFilter->receiveRawFrame(frameBuffer);
	if(handExtractor!=0)
		handExtractor->receiveRawFrame(frameBuffer);
	}

Kinect::FrameBuffer Sandbox::averageDepthFrames(const Kinect::FrameBuffer& newFrame)
	{
	typedef Kinect::FrameSource::DepthPixel DepthPixel;

	/* Push the new frame into the ring, overwriting the oldest entry: */
	diskAveragingRing[diskAveragingRingNext]=newFrame;
	diskAveragingRingNext=(diskAveragingRingNext+1)%diskAveragingWindow;
	if(diskAveragingRingSize<diskAveragingWindow)
		++diskAveragingRingSize;

	/* Average the currently held frames pixel by pixel, skipping invalid
	   readings rather than blending them into a bogus average: */
	Kinect::FrameBuffer result(frameSize[0],frameSize[1],size_t(frameSize[1])*size_t(frameSize[0])*sizeof(DepthPixel));
	DepthPixel* rPtr=result.getData<DepthPixel>();
	unsigned int numPixels=frameSize[1]*frameSize[0];
	for(unsigned int p=0;p<numPixels;++p,++rPtr)
		{
		unsigned int sum=0,count=0;
		for(unsigned int i=0;i<diskAveragingRingSize;++i)
			{
			DepthPixel d=diskAveragingRing[i].getData<const DepthPixel>()[p];
			if(d!=Kinect::FrameSource::invalidDepth)
				{
				sum+=d;
				++count;
				}
			}
		*rPtr=count>0?DepthPixel((sum+count/2)/count):Kinect::FrameSource::invalidDepth;
		}
	return result;
	}

void Sandbox::receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Put the new frame into the frame input buffer: */
	filteredFrames.postNewValue(frameBuffer);

	/* Wake up the foreground thread: */
	Vrui::requestUpdate();
	}

void Sandbox::toggleDEM(DEM* dem)
	{
	/* Check if this is the active DEM: */
	if(activeDem==dem)
		{
		/* Deactivate the currently active DEM: */
		activeDem=0;
		}
	else
		{
		/* Activate this DEM: */
		activeDem=dem;
		}
	
	/* Enable DEM matching in all surface renderers that use a fixed projector matrix, i.e., in all physical sandboxes: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setDem(activeDem);
	}

void Sandbox::addWater(GLContextData& contextData) const
	{
	/* Check if the most recent rain object list is not empty: */
	if(handExtractor!=0&&!handExtractor->getLockedExtractedHands().empty())
		{
		/* Render all rain objects into the water table: */
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		
		/* Create a local coordinate frame to render rain disks: */
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		
		glVertexAttrib1fARB(1,rainStrength/waterSpeed);
		for(HandExtractor::HandList::const_iterator hIt=handExtractor->getLockedExtractedHands().begin();hIt!=handExtractor->getLockedExtractedHands().end();++hIt)
			{
			/* Render a rain disk approximating the hand: */
			glBegin(GL_POLYGON);
			for(int i=0;i<32;++i)
				{
				Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(32);
				glVertex(hIt->center+x*(Math::cos(angle)*hIt->radius*0.75)+y*(Math::sin(angle)*hIt->radius*0.75));
				}
			glEnd();
			}
		
		glPopAttrib();
		}
	}

void Sandbox::pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	pauseUpdates=cbData->set;
	}

void Sandbox::showWaterControlDialogCallback(Misc::CallbackData* cbData)
	{
	Vrui::popupPrimaryWidget(waterControlDialog);
	}

void Sandbox::waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterSpeed=cbData->value;
	}

void Sandbox::sendStatus(void)
	{
	/* Connect lazily. The panel may be started and stopped any number of times
	   while the sandbox runs, so a missing reader is a normal state and not an
	   error worth reporting. O_NONBLOCK is required: opening a FIFO for writing
	   otherwise blocks until a reader appears, which would freeze the sandbox. */
	if(statusPipeFd<0)
		{
		statusPipeFd=open(statusPipeName.c_str(),O_WRONLY|O_NONBLOCK);
		if(statusPipeFd<0)
			return;
		}

	/* Send the settings a user can change from inside the application, so a
	   panel cannot drift out of step with the sandbox's actual state. Sending
	   the whole set every tick rather than tracking changes also means a panel
	   started late is correct as soon as it connects. */
	std::ostringstream status;
	status<<"frameRate "<<1.0/Vrui::getCurrentFrameTime()<<"\n";
	status<<"pauseUpdates "<<(pauseUpdates?"on":"off")<<"\n";
	status<<"waterSpeed "<<waterSpeed<<"\n";
	status<<"waterMaxSteps "<<waterMaxSteps<<"\n";
	status<<"projectorView "<<(!renderSettings.empty()&&renderSettings.front().fixProjectorView?"on":"off")<<"\n";

	const std::string line=status.str();
	ssize_t result=write(statusPipeFd,line.data(),line.size());
	if(result<0)
		{
		/* EAGAIN means the panel is not draining the pipe; skip this update and
		   try again next tick. Anything else means the reader is gone, so drop
		   the descriptor and reconnect later. SIGPIPE is ignored in main(), so a
		   panel exiting cannot take the sandbox down with it. */
		if(errno!=EAGAIN)
			{
			close(statusPipeFd);
			statusPipeFd=-1;
			}
		}
	}

bool Sandbox::unprojectPixel(unsigned int x,unsigned int y,Point& result) const
	{
	const unsigned int* size=depthImageRenderer->getDepthImageSize();
	if(x>=size[0]||y>=size[1]||lastFilteredFrame.getData<GLfloat>()==0)
		return false;

	const GLfloat depth=lastFilteredFrame.getData<GLfloat>()[y*size[0]+x];

	/* The frame filter emits this for pixels it could not stabilise: */
	if(depth==0.0f)
		return false;

	/* Same unprojection the surface vertex shader does, at the pixel centre: */
	PTransform::HVector v(Scalar(x)+Scalar(0.5),Scalar(y)+Scalar(0.5),Scalar(depth),Scalar(1));
	v=depthImageRenderer->getDepthProjection().transform(v);
	if(v[3]==Scalar(0))
		return false;

	result=Point(v[0]/v[3],v[1]/v[3],v[2]/v[3]);
	return true;
	}

void Sandbox::grabDepthImage(const char* fileName)
	{
	const unsigned int* size=depthImageRenderer->getDepthImageSize();
	const GLfloat* dPtr=lastFilteredFrame.getData<GLfloat>();
	if(dPtr==0)
		{
		sendEvent("depthImageFailed noFrame");
		return;
		}

	/* Scale to the range actually present rather than a fixed one, so the sand
	   fills the greyscale and the user can see what they are selecting: */
	GLfloat lo=0.0f,hi=0.0f;
	bool first=true;
	for(unsigned int i=0;i<size[0]*size[1];++i)
		if(dPtr[i]!=0.0f)
			{
			if(first)
				{
				lo=hi=dPtr[i];
				first=false;
				}
			else
				{
				if(dPtr[i]<lo) lo=dPtr[i];
				if(dPtr[i]>hi) hi=dPtr[i];
				}
			}
	if(first||hi<=lo)
		{
		sendEvent("depthImageFailed noRange");
		return;
		}

	/* Plain binary PGM: no image library needed on either side, and Qt reads it. */
	FILE* f=fopen(fileName,"wb");
	if(f==0)
		{
		sendEvent("depthImageFailed writeError");
		return;
		}
	fprintf(f,"P5\n%u %u\n255\n",size[0],size[1]);

	/* Rows go out bottom-up. The depth image follows the OpenGL convention with
	   row 0 at the bottom, while PGM's first row is the top, so writing them in
	   order shows the scene upside down. The panel flips its y mapping to match,
	   and the two have to agree: a click that lands on the wrong depth row
	   silently measures the wrong part of the sandbox. */
	for(int y=int(size[1])-1;y>=0;--y)
		for(unsigned int x=0;x<size[0];++x)
			{
			const GLfloat d=dPtr[(unsigned int)(y)*size[0]+x];
			int v=d==0.0f?0:int((d-lo)*254.0f/(hi-lo))+1;
			fputc(v<0?0:(v>255?255:v),f);
			}
	fclose(f);

	std::ostringstream done;
	done<<"depthImage "<<size[0]<<" "<<size[1];
	sendEvent(done.str().c_str());
	}

void Sandbox::fitPlaneToRegion(unsigned int x0,unsigned int y0,unsigned int x1,unsigned int y1)
	{
	/* Fitting to a region the user selected is the whole point: a fit to the
	   entire depth image cannot tell the sand from the rest of the room, and
	   lands on whatever surface happens to dominate the view. */
	if(x1<x0) std::swap(x0,x1);
	if(y1<y0) std::swap(y0,y1);

	Geometry::PCACalculator<3> pca;
	unsigned int numPoints=0;
	Point p;
	for(unsigned int y=y0;y<=y1;++y)
		for(unsigned int x=x0;x<=x1;++x)
			if(unprojectPixel(x,y,p))
				{
				pca.accumulatePoint(Geometry::PCACalculator<3>::Point(p[0],p[1],p[2]));
				++numPoints;
				}

	if(numPoints<100)
		{
		sendEvent("planeFailed tooFewPoints");
		return;
		}

	pca.calcCovariance();
	double evs[3];
	if(pca.calcEigenvalues(evs)<3)
		{
		sendEvent("planeFailed degenerate");
		return;
		}

	Geometry::PCACalculator<3>::Point centroid=pca.calcCentroid();
	Geometry::PCACalculator<3>::Vector normal=pca.calcEigenvector(evs[2]);
	normal.normalize();

	/* Point the normal back towards the camera at the origin, matching the sign
	   convention RawKinectViewer prints: */
	if(normal[2]<0.0)
		normal=-normal;

	pendingPlane=Plane(Plane::Vector(normal[0],normal[1],normal[2]),
	                   Plane::Point(centroid[0],centroid[1],centroid[2]));
	pendingPlane.normalize();
	pendingPlaneValid=true;

	/* Report it in the same form RawKinectViewer does, so the two can be compared: */
	std::ostringstream result;
	result<<"planeFitted "<<pendingPlane.getNormal()[0]<<" "<<pendingPlane.getNormal()[1]<<" "
	      <<pendingPlane.getNormal()[2]<<" "<<pendingPlane.getOffset()<<" "<<numPoints;
	sendEvent(result.str().c_str());
	std::cout<<"Camera-space plane equation: x * ("<<pendingPlane.getNormal()[0]<<", "
	         <<pendingPlane.getNormal()[1]<<", "<<pendingPlane.getNormal()[2]<<") = "
	         <<pendingPlane.getOffset()<<std::endl;
	}

void Sandbox::setExtentsFromRegion(unsigned int x0,unsigned int y0,unsigned int x1,unsigned int y1)
	{
	/* The plane and the extents are measured in two passes: the plane off a flat
	   plate laid on the sand, which gives a clean reference the loose sand
	   surface cannot, and the extents off the bare sand once the plate is out of
	   the way. So this takes corners only and leaves the fitted plane alone. */
	if(!pendingPlaneValid)
		{
		sendEvent("extentsFailed noPlane");
		return;
		}

	if(x1<x0) std::swap(x0,x1);
	if(y1<y0) std::swap(y0,y1);

	/* Order is the one the layout file wants: bottom left, bottom right, upper
	   left, upper right, where bottom is the low depth row -- the bottom of the
	   image as the panel draws it. */
	const unsigned int cx[4]={x0,x1,x0,x1};
	const unsigned int cy[4]={y0,y0,y1,y1};

	Point corners[4];
	for(int i=0;i<4;++i)
		if(!unprojectPixel(cx[i],cy[i],corners[i]))
			{
			/* A corner over a hole in the depth image would be silently wrong, so
			   refuse rather than measure the wrong part of the box. */
			sendEvent("extentsFailed noDepthAtCorner");
			return;
			}

	numPendingCorners=0;
	for(int i=0;i<4;++i)
		pendingCorners[numPendingCorners++]=corners[i];

	std::ostringstream result;
	result<<"extentsSet "<<Geometry::dist(pendingPlane.project(corners[0]),pendingPlane.project(corners[1]))
	      <<" "<<Geometry::dist(pendingPlane.project(corners[0]),pendingPlane.project(corners[2]));
	sendEvent(result.str().c_str());
	}

void Sandbox::extractPoint(unsigned int x,unsigned int y)
	{
	Point p;
	if(!unprojectPixel(x,y,p))
		{
		sendEvent("pointFailed noDepth");
		return;
		}

	/* Wrap rather than ignore, so clicking after the region filled the corners in
	   replaces them one by one instead of doing nothing. */
	if(numPendingCorners>=4)
		numPendingCorners=0;
	pendingCorners[numPendingCorners++]=p;

	std::ostringstream result;
	result<<"pointExtracted "<<numPendingCorners<<" "<<p[0]<<" "<<p[1]<<" "<<p[2];
	sendEvent(result.str().c_str());
	std::cout<<"3D position: ("<<p[0]<<", "<<p[1]<<", "<<p[2]<<")"<<std::endl;
	}

void Sandbox::resetLayoutCapture(void)
	{
	pendingPlaneValid=false;
	numPendingCorners=0;
	sendEvent("layoutReset");
	}

void Sandbox::mirrorCalibrationFile(const std::string& fileName) const
	{
	/* Keep a second copy of every calibration file the sandbox writes, so the
	   version-controlled config directory cannot drift from the one the running
	   binary actually reads. Silent no-op when no mirror is configured. */
	if(calibrationMirrorDir.empty())
		return;

	std::string base=fileName;
	std::string::size_type slash=base.rfind('/');
	if(slash!=std::string::npos)
		base=base.substr(slash+1);
	const std::string target=calibrationMirrorDir+"/"+base;

	std::ifstream in(fileName.c_str(),std::ios::binary);
	std::ofstream out(target.c_str(),std::ios::binary);
	if(!in||!out)
		{
		std::cerr<<"Unable to mirror "<<fileName<<" to "<<target<<std::endl;
		return;
		}
	out<<in.rdbuf();
	}

void Sandbox::updateBoxTransform(const Plane& basePlane)
	{
	/* Calculate the transformation from camera space to sandbox space. Assumes
	   basePlaneCorners is already current. */
	ONTransform::Vector z=basePlane.getNormal();
	ONTransform::Vector x=(basePlaneCorners[1]-basePlaneCorners[0])+(basePlaneCorners[3]-basePlaneCorners[2]);
	ONTransform::Vector y=z^x;
	boxTransform=ONTransform::rotate(Geometry::invert(ONTransform::Rotation::fromBaseVectors(x,y)));
	ONTransform::Point center=Geometry::mid(Geometry::mid(basePlaneCorners[0],basePlaneCorners[1]),Geometry::mid(basePlaneCorners[2],basePlaneCorners[3]));
	boxTransform*=ONTransform::translateToOriginFrom(center);

	/* Calculate the size of the sandbox area: */
	boxSize=Geometry::dist(center,basePlaneCorners[0]);
	for(int i=1;i<4;++i)
		boxSize=Math::max(boxSize,Geometry::dist(center,basePlaneCorners[i]));
	}

void Sandbox::writeSandboxLayout(void)
	{
	if(!pendingPlaneValid)
		{
		sendEvent("layoutFailed noPlane");
		return;
		}
	if(numPendingCorners<4)
		{
		std::ostringstream failure;
		failure<<"layoutFailed needCorners "<<numPendingCorners;
		sendEvent(failure.str().c_str());
		return;
		}

	/* Project the corners onto the fitted plane so the two agree exactly, which
	   is what the layout file reader assumes. The corner order is the order they
	   were extracted in: bottom left, bottom right, upper left, upper right. */
	Point corners[4];
	for(int i=0;i<4;++i)
		corners[i]=pendingPlane.project(pendingCorners[i]);

	try
		{
		rename(sandboxLayoutFileName.c_str(),(sandboxLayoutFileName+".bak").c_str());

		std::ofstream layout(sandboxLayoutFileName.c_str());
		layout<<Misc::ValueCoder<Plane>::encode(pendingPlane)<<std::endl;
		for(int i=0;i<4;++i)
			layout<<Misc::ValueCoder<Point>::encode(corners[i])<<std::endl;
		layout.close();
		}
	catch(const std::runtime_error& err)
		{
		std::cerr<<"Unable to write sandbox layout file "<<sandboxLayoutFileName<<": "<<err.what()<<std::endl;
		sendEvent("layoutFailed writeError");
		return;
		}

	mirrorCalibrationFile(sandboxLayoutFileName);

	/* Apply the plane and corners at once so elevation colouring and the
	   calibration view's box outline are right without a restart. The water
	   table is sized from the layout at startup, so the simulation domain
	   still needs one. */
	depthImageRenderer->setBasePlane(pendingPlane);
	for(int i=0;i<4;++i)
		basePlaneCorners[i]=corners[i];
	updateBoxTransform(pendingPlane);

	std::ostringstream done;
	done<<"layoutWritten "<<Geometry::dist(corners[0],corners[1])<<" "
	    <<Geometry::dist(corners[0],corners[2]);
	sendEvent(done.str().c_str());
	std::cout<<"Sandbox layout written; restarting to resize the water simulation domain"<<std::endl;

	/* The water table's domain is fixed at construction, so picking up the new
	   layout means rebuilding the process, not just this object. Ask Vrui's main
	   loop to stop; restartIfRequested() actually re-execs once run() returns and
	   ~Sandbox() has cleanly torn everything down. */
	restartRequested=true;
	Vrui::shutdown();
	}

#if !KINECT_CONFIG_USE_SHADERPROJECTOR

void Sandbox::facadeMeshCallback(const Kinect::MeshBuffer&)
	{
	Vrui::requestUpdate();
	}

#endif

void Sandbox::diskExtractionCallback(const Kinect::DiskExtractor::DiskList& disks)
	{
	/* Called from the disk extractor's own thread, so the result is handed to the
	   main thread through a triple buffer rather than touched directly. Every
	   entry already passed the shape test; prefer the largest blob rather than
	   discarding the frame when more than one appears, since a spurious patch of
	   clutter is reliably smaller than the target the user is deliberately
	   holding still. */
	if(!disks.empty())
		{
		const Kinect::DiskExtractor::Disk* best=&disks.front();
		for(Kinect::DiskExtractor::DiskList::const_iterator dIt=disks.begin()+1;dIt!=disks.end();++dIt)
			if(dIt->numPixels>best->numPixels)
				best=&*dIt;

		lastDisk.startNewValue()=*best;
		lastDisk.postNewValue();
		diskEverSeenThisCalibration=true;

		/* Wake the main thread: nothing else drives the display while calibrating,
		   since the frame filter is skipped then. */
		Vrui::requestUpdate();
		}
	}

Geometry::Point<double,2> Sandbox::getTiePointTarget(unsigned int index) const
	{
	/* Lay the targets out on a grid inset from the edges of the projected image.
	   Targets right at the border would put the disk half off the sand, and a
	   homography fitted only to the middle of the image extrapolates badly. */
	const unsigned int cols=3;
	const unsigned int rows=(numTiePoints+cols-1)/cols;
	const unsigned int col=index%cols;
	const unsigned int row=(index/cols)%(rows>0?rows:1);

	const double insetX=double(projectorImageSize[0])*0.15;
	const double insetY=double(projectorImageSize[1])*0.15;
	const double spanX=double(projectorImageSize[0])-2.0*insetX;
	const double spanY=double(projectorImageSize[1])-2.0*insetY;

	return Geometry::Point<double,2>(insetX+spanX*double(col)/double(cols-1),
	                                 insetY+spanY*(rows>1?double(row)/double(rows-1):0.5));
	}

void Sandbox::startProjectorCalibration(unsigned int width,unsigned int height,unsigned int tiePointCount)
	{
	if(diskExtractor==0)
		{
		sendEvent("calibrationFailed projector noExtractor");
		return;
		}

	projectorImageSize[0]=width;
	projectorImageSize[1]=height;
	numTiePoints=tiePointCount<6?6:tiePointCount;
	tiePoints.clear();
	tiePointIndex=0;
	haveDisk=false;
	diskEverSeenThisCalibration=false;
	diskAveragingRingSize=0;
	diskAveragingRingNext=0;
	calibratingProjector=true;

	/* Blank the sand out of the depth stream for the duration. The extractor
	   separates blobs by depth steps alone, so a target held close to the sand,
	   or anywhere over a mound, merges into the surface and fails the shape test
	   -- which is why detection works in some spots and not others. Removal
	   makes the held target the only foreground, as the standalone
	   CalibrateProjector does. Costs the first ~2s to capture the sand, and
	   means the target has to be held above the surface, not laid on it. */
	Kinect::DirectFrameSource* directCamera=dynamic_cast<Kinect::DirectFrameSource*>(camera);
	if(directCamera!=0)
		{
		directCamera->captureBackground(60,true);
		directCamera->setRemoveBackground(true);
		}

	diskExtractor->startStreaming(Misc::createFunctionCall(this,&Sandbox::diskExtractionCallback));

	std::ostringstream started;
	started<<"calibrationStarted projector "<<numTiePoints;
	sendEvent(started.str().c_str());
	}

void Sandbox::captureTiePoint(void)
	{
	if(!calibratingProjector)
		return;
	if(!haveDisk)
		{
		sendEvent("calibrationNoTarget projector");
		return;
		}

	TiePoint tp;
	tp.p=getTiePointTarget(tiePointIndex);
	tp.o=lastDisk.getLockedValue().center;
	tiePoints.push_back(tp);

	++tiePointIndex;
	if(tiePointIndex>=numTiePoints)
		finishProjectorCalibration();
	else
		{
		std::ostringstream progress;
		progress<<"calibrationProgress projector "<<tiePointIndex<<" "<<numTiePoints;
		sendEvent(progress.str().c_str());
		}
	}

void Sandbox::restoreDepthStream(void)
	{
	/* Undo the background removal that startProjectorCalibration turned on: */
	Kinect::DirectFrameSource* directCamera=dynamic_cast<Kinect::DirectFrameSource*>(camera);
	if(directCamera!=0)
		directCamera->setRemoveBackground(false);
	}

void Sandbox::abortProjectorCalibration(void)
	{
	if(!calibratingProjector)
		return;
	calibratingProjector=false;
	diskExtractor->stopStreaming();
	restoreDepthStream();
	tiePoints.clear();
	sendEvent("calibrationAborted projector");
	}

void Sandbox::finishProjectorCalibration(void)
	{
	calibratingProjector=false;
	diskExtractor->stopStreaming();
	restoreDepthStream();

	/* A projective camera cannot be recovered from tie points that all lie in one
	   plane: the system is rank deficient, the solution still passes through
	   every tie point -- so the residual looks fine -- and the resulting matrix
	   throws the rendered view off the sand entirely. Capturing all targets at
	   the same height above the surface is the easy way to end up there, so
	   require a real spread in height before trusting the solve. */
	const double minHeightSpread=10.0; // In cm; roughly what a hand varies by without trying
	Math::Interval<double> heightRange=Math::Interval<double>::empty;
	for(std::vector<TiePoint>::iterator tpIt=tiePoints.begin();tpIt!=tiePoints.end();++tpIt)
		heightRange.addValue(tpIt->o[2]); // Camera z: the sensor looks down at the sand, so this is height
	if(heightRange.getSize()<minHeightSpread)
		{
		std::ostringstream flat;
		flat<<"calibrationFailed projector flatCapture "<<heightRange.getSize();
		sendEvent(flat.str().c_str());
		std::cerr<<"Projector calibration rejected: targets spanned only "<<heightRange.getSize()
		         <<"cm in height; hold the disk at clearly different heights"<<std::endl;
		return;
		}

	/* Build the least-squares system for the 3x4 homography mapping camera space
	   to projector image space. Each tie point contributes two equations. */
	Math::Matrix a(12,12,0.0);
	for(std::vector<TiePoint>::iterator tpIt=tiePoints.begin();tpIt!=tiePoints.end();++tpIt)
		{
		double eq[2][12];
		eq[0][0]=tpIt->o[0];  eq[0][1]=tpIt->o[1];  eq[0][2]=tpIt->o[2];  eq[0][3]=1.0;
		eq[0][4]=0.0;         eq[0][5]=0.0;         eq[0][6]=0.0;         eq[0][7]=0.0;
		eq[0][8]=-tpIt->p[0]*tpIt->o[0];
		eq[0][9]=-tpIt->p[0]*tpIt->o[1];
		eq[0][10]=-tpIt->p[0]*tpIt->o[2];
		eq[0][11]=-tpIt->p[0];

		eq[1][0]=0.0;         eq[1][1]=0.0;         eq[1][2]=0.0;         eq[1][3]=0.0;
		eq[1][4]=tpIt->o[0];  eq[1][5]=tpIt->o[1];  eq[1][6]=tpIt->o[2];  eq[1][7]=1.0;
		eq[1][8]=-tpIt->p[1]*tpIt->o[0];
		eq[1][9]=-tpIt->p[1]*tpIt->o[1];
		eq[1][10]=-tpIt->p[1]*tpIt->o[2];
		eq[1][11]=-tpIt->p[1];

		for(int row=0;row<2;++row)
			for(unsigned int i=0;i<12;++i)
				for(unsigned int j=0;j<12;++j)
					a(i,j)+=eq[row][i]*eq[row][j];
		}

	/* The solution is the eigenvector belonging to the smallest eigenvalue: */
	std::pair<Math::Matrix,Math::Matrix> qe=a.jacobiIteration();
	unsigned int minEIndex=0;
	double minE=Math::abs(qe.second(0,0));
	for(unsigned int i=1;i<12;++i)
		if(minE>Math::abs(qe.second(i,0)))
			{
			minEIndex=i;
			minE=Math::abs(qe.second(i,0));
			}

	Math::Matrix hom(3,4);
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			hom(i,j)=qe.first(i*4+j,minEIndex);

	/* Scale so projected weights are a positive distance from the projector.
	   Mixed signs mean the tie points straddle the projector's plane, which
	   cannot happen for a real capture and indicates a bad set. */
	double wLen=Math::sqrt(Math::sqr(hom(2,0))+Math::sqr(hom(2,1))+Math::sqr(hom(2,2)));
	int numNegativeWeights=0;
	for(std::vector<TiePoint>::iterator tpIt=tiePoints.begin();tpIt!=tiePoints.end();++tpIt)
		{
		double w=hom(2,3);
		for(int j=0;j<3;++j)
			w+=hom(2,j)*tpIt->o[j];
		if(w<0.0)
			++numNegativeWeights;
		}
	if(numNegativeWeights!=0&&numNegativeWeights!=int(tiePoints.size()))
		{
		sendEvent("calibrationFailed projector inconsistentWeights");
		return;
		}
	if(numNegativeWeights>0)
		wLen=-wLen;
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			hom(i,j)/=wLen;

	/* Residual in projector pixels: the honest measure of whether the capture was
	   any good, and worth reporting rather than silently accepting. */
	double res=0.0;
	for(std::vector<TiePoint>::iterator tpIt=tiePoints.begin();tpIt!=tiePoints.end();++tpIt)
		{
		Math::Matrix op(4,1);
		for(int i=0;i<3;++i)
			op(i)=tpIt->o[i];
		op(3)=1.0;
		Math::Matrix pp=hom*op;
		for(int i=0;i<2;++i)
			pp(i)/=pp(2);
		res+=Math::sqr(pp(0)-tpIt->p[0])+Math::sqr(pp(1)-tpIt->p[1]);
		}
	res=Math::sqrt(res/double(tiePoints.size()));

	/* Expand the 3x4 homography into a full 4x4 projection: */
	Math::Matrix projection(4,4);
	for(unsigned int i=0;i<2;++i)
		for(unsigned int j=0;j<4;++j)
			projection(i,j)=hom(i,j);
	for(unsigned int j=0;j<3;++j)
		projection(2,j)=0.0;
	projection(2,3)=-1.0;
	for(unsigned int j=0;j<4;++j)
		projection(3,j)=hom(2,j);

	/* Map the tie points' depth range into clip space, with a margin so surfaces
	   somewhat outside the calibrated volume are still drawn: */
	Math::Interval<double> zRange=Math::Interval<double>::empty;
	for(std::vector<TiePoint>::iterator tpIt=tiePoints.begin();tpIt!=tiePoints.end();++tpIt)
		{
		Math::Matrix op(4,1);
		for(int i=0;i<3;++i)
			op(i)=double(tpIt->o[i]);
		op(3)=1.0;
		Math::Matrix pp=projection*op;
		zRange.addValue(pp(2)/pp(3));
		}
	zRange=Math::Interval<double>(zRange.getMin()*2.0,zRange.getMax()*0.5);

	Math::Matrix invViewport(4,4,1.0);
	invViewport(0,0)=2.0/double(projectorImageSize[0]);
	invViewport(0,3)=-1.0;
	invViewport(1,1)=2.0/double(projectorImageSize[1]);
	invViewport(1,3)=-1.0;
	invViewport(2,2)=2.0/(zRange.getSize());
	invViewport(2,3)=-2.0*zRange.getMin()/(zRange.getSize())-1.0;
	projection=invViewport*projection;

	/* Write the matrix in the little-endian row-major layout the renderer reads: */
	try
		{
		/* Keep the previous matrix, as the layout writer does: a calibration that
		   turns out worse than the one it replaced is otherwise unrecoverable. */
		rename(projectionMatrixFileName.c_str(),(projectionMatrixFileName+".bak").c_str());

		IO::FilePtr projFile=IO::openFile(projectionMatrixFileName.c_str(),IO::File::WriteOnly);
		projFile->setEndianness(Misc::LittleEndian);
		for(int i=0;i<4;++i)
			for(int j=0;j<4;++j)
				projFile->write<double>(projection(i,j));
		}
	catch(const std::runtime_error& err)
		{
		std::cerr<<"Unable to write projector matrix "<<projectionMatrixFileName<<": "<<err.what()<<std::endl;
		sendEvent("calibrationFailed projector writeError");
		return;
		}

	mirrorCalibrationFile(projectionMatrixFileName);

	/* Take the new calibration into use straight away rather than making the
	   user restart with -fpv: the transform is per render settings and there is
	   already a loader for it. */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		rsIt->loadProjectorTransform(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);
		rsIt->fixProjectorView=rsIt->projectorTransformValid;
		}

	std::ostringstream done;
	done<<"calibrationDone projector "<<res;
	sendEvent(done.str().c_str());
	std::cout<<"Projector calibration written, RMS residual "<<res<<" pixels"<<std::endl;
	}

bool Sandbox::sendEvent(const char* event)
	{
	/* Ask the panel to do something. Harmless if no panel is listening. */
	if(statusPipeFd<0)
		return false;

	const std::string line=std::string(event)+"\n";
	if(write(statusPipeFd,line.data(),line.size())>=0)
		return true;

	/* The reader is gone. Drop the descriptor so the next attempt reconnects, and
	   report failure so the caller can start a panel instead of talking to a pipe
	   nobody is listening to: an open descriptor is not evidence of a panel. */
	if(errno!=EAGAIN)
		{
		close(statusPipeFd);
		statusPipeFd=-1;
		}
	return false;
	}

void Sandbox::showPanelCallback(Misc::CallbackData* cbData)
	{
	/* If the message actually reached a panel, it will raise itself: */
	if(sendEvent("showPanel"))
		return;

	/* Otherwise start one. Without this the menu entry does nothing whenever the
	   panel happens not to be running, which is exactly when the user reaches for
	   it. Detached via a double fork so the sandbox never has to reap it. */
	pid_t pid=fork();
	if(pid==0)
		{
		if(fork()==0)
			{
			execlp(controlPanelCommand.c_str(),controlPanelCommand.c_str(),
			       "-p",controlPipeName.c_str(),(char*)0);
			std::cerr<<"Unable to start control panel "<<controlPanelCommand<<std::endl;
			_exit(1);
			}
		_exit(0);
		}
	else if(pid>0)
		waitpid(pid,0,0);
	}

void Sandbox::showCalibrationCallback(Misc::CallbackData* cbData)
	{
	sendEvent("showCalibration");
	}

void Sandbox::setSunDirection(GLfloat azimuth,GLfloat elevation)
	{
	/* Store the new direction so control pipe updates compose correctly: */
	sunAzimuth=azimuth;
	sunElevation=elevation;

	if(sun==0)
		return;

	/* Convert azimuth and elevation to a directional light vector. Azimuth is
	   measured clockwise from the far side of the sandbox, matching the
	   cartographic convention where 315 degrees lights the terrain from the
	   upper left: */
	GLfloat az=Math::rad(azimuth);
	GLfloat el=Math::rad(elevation);
	GLfloat ce=Math::cos(el);
	sun->getLight().position=GLLight::Position(Math::sin(az)*ce,Math::cos(az)*ce,Math::sin(el),0.0f);
	}

void Sandbox::waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterMaxSteps=int(Math::floor(cbData->value+0.5));
	}

void Sandbox::waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterTable->setAttenuation(GLfloat(1.0-cbData->value));
	}

GLMotif::PopupMenu* Sandbox::createMainMenu(void)
	{
	/* Create a popup shell to hold the main menu: */
	GLMotif::PopupMenu* mainMenuPopup=new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
	mainMenuPopup->setTitle("AR Sandbox");
	
	/* Create the main menu itself: */
	GLMotif::Menu* mainMenu=new GLMotif::Menu("MainMenu",mainMenuPopup,false);
	
	/* The only entry: raise the external control panel. Everything the old menu
	   and the water control dialog offered now lives there, so duplicating it in
	   GLMotif would just be two interfaces to keep in step.
	   Inert if the sandbox was started without a control pipe. */
	GLMotif::Button* showPanelButton=new GLMotif::Button("ShowPanelButton",mainMenu,"Open Control Panel");
	showPanelButton->getSelectCallbacks().add(this,&Sandbox::showPanelCallback);

	/* Finish building the main menu: */
	mainMenu->manageChild();
	
	return mainMenuPopup;
	}

GLMotif::PopupWindow* Sandbox::createWaterControlDialog(void)
	{
	const GLMotif::StyleSheet& ss=*Vrui::getUiStyleSheet();
	
	/* Create a popup window shell: */
	GLMotif::PopupWindow* waterControlDialogPopup=new GLMotif::PopupWindow("WaterControlDialogPopup",Vrui::getWidgetManager(),"Water Simulation Control");
	waterControlDialogPopup->setCloseButton(true);
	waterControlDialogPopup->setResizableFlags(true,false);
	waterControlDialogPopup->popDownOnClose();
	
	GLMotif::RowColumn* waterControlDialog=new GLMotif::RowColumn("WaterControlDialog",waterControlDialogPopup,false);
	waterControlDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	waterControlDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	waterControlDialog->setNumMinorWidgets(2);
	
	new GLMotif::Label("WaterSpeedLabel",waterControlDialog,"Speed");
	
	waterSpeedSlider=new GLMotif::TextFieldSlider("WaterSpeedSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterSpeedSlider->getTextField()->setFieldWidth(7);
	waterSpeedSlider->getTextField()->setPrecision(4);
	waterSpeedSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterSpeedSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterSpeedSlider->setValueRange(0.001,10.0,0.05);
	waterSpeedSlider->getSlider()->addNotch(0.0f);
	waterSpeedSlider->setValue(waterSpeed);
	waterSpeedSlider->getValueChangedCallbacks().add(this,&Sandbox::waterSpeedSliderCallback);
	
	new GLMotif::Label("WaterMaxStepsLabel",waterControlDialog,"Max Steps");
	
	waterMaxStepsSlider=new GLMotif::TextFieldSlider("WaterMaxStepsSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterMaxStepsSlider->getTextField()->setFieldWidth(7);
	waterMaxStepsSlider->getTextField()->setPrecision(0);
	waterMaxStepsSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	waterMaxStepsSlider->setSliderMapping(GLMotif::TextFieldSlider::LINEAR);
	waterMaxStepsSlider->setValueType(GLMotif::TextFieldSlider::UINT);
	waterMaxStepsSlider->setValueRange(0,200,1);
	waterMaxStepsSlider->setValue(waterMaxSteps);
	waterMaxStepsSlider->getValueChangedCallbacks().add(this,&Sandbox::waterMaxStepsSliderCallback);
	
	new GLMotif::Label("FrameRateLabel",waterControlDialog,"Frame Rate");
	
	GLMotif::Margin* frameRateMargin=new GLMotif::Margin("FrameRateMargin",waterControlDialog,false);
	frameRateMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	frameRateTextField=new GLMotif::TextField("FrameRateTextField",frameRateMargin,8);
	frameRateTextField->setFieldWidth(7);
	frameRateTextField->setPrecision(2);
	frameRateTextField->setFloatFormat(GLMotif::TextField::FIXED);
	frameRateTextField->setValue(0.0);
	
	frameRateMargin->manageChild();
	
	new GLMotif::Label("WaterAttenuationLabel",waterControlDialog,"Attenuation");
	
	waterAttenuationSlider=new GLMotif::TextFieldSlider("WaterAttenuationSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterAttenuationSlider->getTextField()->setFieldWidth(7);
	waterAttenuationSlider->getTextField()->setPrecision(5);
	waterAttenuationSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterAttenuationSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterAttenuationSlider->setValueRange(0.001,1.0,0.01);
	waterAttenuationSlider->getSlider()->addNotch(Math::log10(1.0-double(waterTable->getAttenuation())));
	waterAttenuationSlider->setValue(1.0-double(waterTable->getAttenuation()));
	waterAttenuationSlider->getValueChangedCallbacks().add(this,&Sandbox::waterAttenuationSliderCallback);
	
	waterControlDialog->manageChild();
	
	return waterControlDialogPopup;
	}

namespace {

/****************
Helper functions:
****************/

void printUsage(void)
	{
	std::cout<<"Usage: SARndbox [option 1] ... [option n]"<<std::endl;
	std::cout<<"  Options:"<<std::endl;
	std::cout<<"  -h"<<std::endl;
	std::cout<<"     Prints this help message"<<std::endl;
	std::cout<<"  -remote [<listening port ID>]"<<std::endl;
	std::cout<<"     Creates a data streaming server listening on TCP port <listening port ID>"<<std::endl;
	std::cout<<"     Default listening port ID: 26000"<<std::endl;
	std::cout<<"  -c <camera index>"<<std::endl;
	std::cout<<"     Selects the local 3D camera of the given index (0: first camera"<<std::endl;
	std::cout<<"     on USB bus)"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -f <frame file name prefix>"<<std::endl;
	std::cout<<"     Reads a pre-recorded 3D video stream from a pair of color/depth"<<std::endl;
	std::cout<<"     files of the given file name prefix"<<std::endl;
	std::cout<<"  -s <scale factor>"<<std::endl;
	std::cout<<"     Scale factor from real sandbox to simulated terrain"<<std::endl;
	std::cout<<"     Default: 100.0 (1:100 scale, 1cm in sandbox is 1m in terrain"<<std::endl;
	std::cout<<"  -slf <sandbox layout file name>"<<std::endl;
	std::cout<<"     Loads the sandbox layout file of the given name"<<std::endl;
	std::cout<<"     Default: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTBOXLAYOUTFILENAME<<std::endl;
	std::cout<<"  -er <min elevation> <max elevation>"<<std::endl;
	std::cout<<"     Sets the range of valid sand surface elevations relative to the"<<std::endl;
	std::cout<<"     ground plane in cm"<<std::endl;
	std::cout<<"     Default: Range of elevation color map"<<std::endl;
	std::cout<<"  -hmp <x> <y> <z> <offset>"<<std::endl;
	std::cout<<"     Sets an explicit base plane equation to use for height color mapping"<<std::endl;
	std::cout<<"  -nas <num averaging slots>"<<std::endl;
	std::cout<<"     Sets the number of averaging slots in the frame filter; latency is"<<std::endl;
	std::cout<<"     <num averaging slots> * 1/30 s"<<std::endl;
	std::cout<<"     Default: 30"<<std::endl;
	std::cout<<"  -sp <min num samples> <max variance>"<<std::endl;
	std::cout<<"     Sets the frame filter parameters minimum number of valid samples"<<std::endl;
	std::cout<<"     and maximum sample variance before convergence"<<std::endl;
	std::cout<<"     Default: 10 2"<<std::endl;
	std::cout<<"  -he <hysteresis envelope>"<<std::endl;
	std::cout<<"     Sets the size of the hysteresis envelope used for jitter removal"<<std::endl;
	std::cout<<"     Default: 0.1"<<std::endl;
	std::cout<<"  -wts <water grid width> <water grid height>"<<std::endl;
	std::cout<<"     Sets the width and height of the water flow simulation grid"<<std::endl;
	std::cout<<"     Default: 640 480"<<std::endl;
	std::cout<<"  -ws <water speed> <water max steps>"<<std::endl;
	std::cout<<"     Sets the relative speed of the water simulation and the maximum"<<std::endl;
	std::cout<<"     number of simulation steps per frame"<<std::endl;
	std::cout<<"     Default: 1.0 30"<<std::endl;
	std::cout<<"  -rer <min rain elevation> <max rain elevation>"<<std::endl;
	std::cout<<"     Sets the elevation range of the rain cloud level relative to the"<<std::endl;
	std::cout<<"     ground plane in cm"<<std::endl;
	std::cout<<"     Default: Above range of elevation color map"<<std::endl;
	std::cout<<"  -rs <rain strength>"<<std::endl;
	std::cout<<"     Sets the strength of global or local rainfall in cm/s"<<std::endl;
	std::cout<<"     Default: 0.25"<<std::endl;
	std::cout<<"  -evr <evaporation rate>"<<std::endl;
	std::cout<<"     Water evaporation rate in cm/s"<<std::endl;
	std::cout<<"     Default: 0.0"<<std::endl;
	std::cout<<"  -dds <DEM distance scale>"<<std::endl;
	std::cout<<"     DEM matching distance scale factor in cm"<<std::endl;
	std::cout<<"     Default: 1.0"<<std::endl;
	std::cout<<"  -wi <window index>"<<std::endl;
	std::cout<<"     Sets the zero-based index of the display window to which the"<<std::endl;
	std::cout<<"     following rendering settings are applied"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -fpv [projector transform file name]"<<std::endl;
	std::cout<<"     Fixes the navigation transformation so that Kinect camera and"<<std::endl;
	std::cout<<"     projector are aligned, as defined by the projector transform file"<<std::endl;
	std::cout<<"     of the given name"<<std::endl;
	std::cout<<"     Default projector transform file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTPROJECTIONMATRIXFILENAME<<std::endl;
	std::cout<<"  -nhs"<<std::endl;
	std::cout<<"     Disables hill shading"<<std::endl;
	std::cout<<"  -uhs"<<std::endl;
	std::cout<<"     Enables hill shading"<<std::endl;
	std::cout<<"  -ns"<<std::endl;
	std::cout<<"     Disables shadows"<<std::endl;
	std::cout<<"  -us"<<std::endl;
	std::cout<<"     Enables shadows"<<std::endl;
	std::cout<<"  -nhm"<<std::endl;
	std::cout<<"     Disables elevation color mapping"<<std::endl;
	std::cout<<"  -uhm [elevation color map file name]"<<std::endl;
	std::cout<<"     Enables elevation color mapping and loads the elevation color map from"<<std::endl;
	std::cout<<"     the file of the given name"<<std::endl;
	std::cout<<"     Default elevation color  map file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME<<std::endl;
	std::cout<<"  -ncl"<<std::endl;
	std::cout<<"     Disables topographic contour lines"<<std::endl;
	std::cout<<"  -ucl [contour line spacing]"<<std::endl;
	std::cout<<"     Enables topographic contour lines and sets the elevation distance between"<<std::endl;
	std::cout<<"     adjacent contour lines to the given value in cm"<<std::endl;
	std::cout<<"  -clw <contour line width>"<<std::endl;
	std::cout<<"     Sets the width of topographic contour lines in screen pixels"<<std::endl;
	std::cout<<"  -rst <relief strength>"<<std::endl;
	std::cout<<"     Sets the strength of relief shading from 0.0 to 1.0 when hill"<<std::endl;
	std::cout<<"     shading is enabled via -uhs"<<std::endl;
	std::cout<<"     Default contour line spacing: 0.75"<<std::endl;
	std::cout<<"  -rws"<<std::endl;
	std::cout<<"     Renders water surface as geometric surface"<<std::endl;
	std::cout<<"  -rwt"<<std::endl;
	std::cout<<"     Renders water surface as texture"<<std::endl;
	std::cout<<"  -wo <water opacity>"<<std::endl;
	std::cout<<"     Sets the water depth at which water appears opaque in cm"<<std::endl;
	std::cout<<"     Default: 2.0"<<std::endl;
	std::cout<<"  -cp <control pipe name>"<<std::endl;
	std::cout<<"     Sets the name of a named POSIX pipe from which to read control commands"<<std::endl;
	}

}

Sandbox::Sandbox(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 remoteServer(0),
	 camera(0),pixelDepthCorrection(0),
	 frameFilter(0),pauseUpdates(false),
	 depthImageRenderer(0),
	 waterTable(0),drainWaterRequested(false),
	 handExtractor(0),addWaterFunction(0),addWaterFunctionRegistered(false),
	 sun(0),sunAzimuth(315.0f),sunElevation(45.0f),
	 activeDem(0),
	 mainMenu(0),pauseUpdatesToggle(0),waterControlDialog(0),
	 waterSpeedSlider(0),waterMaxStepsSlider(0),frameRateTextField(0),waterAttenuationSlider(0),
	 controlPipeFd(-1),statusPipeFd(-1),nextStatusTime(0.0),
	 pendingPlaneValid(false),numPendingCorners(0),
	 diskExtractor(0),calibrationFacade(0),calibratingProjector(false),tiePointIndex(0),numTiePoints(0),haveDisk(false),
	 diskEverSeenThisCalibration(false),lastDiskTime(0.0),
	 diskAveragingRingSize(0),diskAveragingRingNext(0)
	{
	/* Runs restartIfRequested() after this object is fully destroyed, whether
	   run() returns normally or an exception unwinds past it: */
	atexit(restartIfRequested);

	/* Read the sandbox's default configuration parameters: */
	std::string sandboxConfigFileName=CONFIG_CONFIGDIR;
	sandboxConfigFileName.push_back('/');
	sandboxConfigFileName.append(CONFIG_DEFAULTCONFIGFILENAME);
	Misc::ConfigurationFile sandboxConfigFile(sandboxConfigFileName.c_str());
	Misc::ConfigurationFileSection cfg=sandboxConfigFile.getSection("/SARndbox");
	unsigned int cameraIndex=cfg.retrieveValue<int>("./cameraIndex",0);
	std::string cameraConfiguration=cfg.retrieveString("./cameraConfiguration","Camera");
	double scale=cfg.retrieveValue<double>("./scaleFactor",100.0);
	sandboxLayoutFileName=CONFIG_CONFIGDIR;
	sandboxLayoutFileName.push_back('/');
	sandboxLayoutFileName.append(CONFIG_DEFAULTBOXLAYOUTFILENAME);
	sandboxLayoutFileName=cfg.retrieveString("./sandboxLayoutFileName",sandboxLayoutFileName);
	Math::Interval<double> elevationRange=cfg.retrieveValue<Math::Interval<double> >("./elevationRange",Math::Interval<double>(-1000.0,1000.0));
	bool haveHeightMapPlane=cfg.hasTag("./heightMapPlane");
	Plane heightMapPlane;
	if(haveHeightMapPlane)
		heightMapPlane=cfg.retrieveValue<Plane>("./heightMapPlane");
	unsigned int numAveragingSlots=cfg.retrieveValue<unsigned int>("./numAveragingSlots",30);
	unsigned int minNumSamples=cfg.retrieveValue<unsigned int>("./minNumSamples",10);
	unsigned int maxVariance=cfg.retrieveValue<unsigned int>("./maxVariance",2);
	float hysteresis=cfg.retrieveValue<float>("./hysteresis",0.1f);
	Misc::FixedArray<unsigned int,2> wtSize;
	wtSize[0]=640;
	wtSize[1]=480;
	wtSize=cfg.retrieveValue<Misc::FixedArray<unsigned int,2> >("./waterTableSize",wtSize);
	waterSpeed=cfg.retrieveValue<double>("./waterSpeed",1.0);
	waterMaxSteps=cfg.retrieveValue<unsigned int>("./waterMaxSteps",30U);
	Math::Interval<double> rainElevationRange=cfg.retrieveValue<Math::Interval<double> >("./rainElevationRange",Math::Interval<double>(-1000.0,1000.0));
	rainStrength=cfg.retrieveValue<GLfloat>("./rainStrength",0.25f);
	double evaporationRate=cfg.retrieveValue<double>("./evaporationRate",0.0);
	float demDistScale=cfg.retrieveValue<float>("./demDistScale",1.0f);
	/* Default to a well-known pipe rather than nothing, and create it below if it
	   does not exist. Requiring the user to mkfifo and pass -cp meant the control
	   panel could not be opened from the sandbox's menu at all unless they
	   remembered both, which is the whole point of having the menu entry. */
	controlPipeName=cfg.retrieveString("./controlPipeName","/tmp/sarndbox.pipe");
	controlPanelCommand=cfg.retrieveString("./controlPanelCommand","sandbox-control");
	calibrationMirrorDir=cfg.retrieveString("./calibrationMirrorDir","");
	
	/* Process command line parameters: */
	bool printHelp=false;
	const char* frameFilePrefix=0;
	const char* kinectServerName=0;
	bool useRemoteServer=false;
	int remoteServerPortId=26000;
	int windowIndex=0;
	renderSettings.push_back(RenderSettings());
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"h")==0)
				printHelp=true;
			else if(strcasecmp(argv[i]+1,"remote")==0)
				{
				/* Check if there is an optional port number: */
				if(i+1<argc&&argv[i+1][0]>='0'&&argv[i+1][0]<='9')
					{
					++i;
					remoteServerPortId=atoi(argv[i]);
					}
				
				useRemoteServer=true;
				}
			else if(strcasecmp(argv[i]+1,"c")==0)
				{
				++i;
				cameraIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"f")==0)
				{
				++i;
				frameFilePrefix=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"p")==0)
				{
				++i;
				kinectServerName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"s")==0)
				{
				++i;
				scale=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"slf")==0)
				{
				++i;
				sandboxLayoutFileName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"er")==0)
				{
				++i;
				double elevationMin=atof(argv[i]);
				++i;
				double elevationMax=atof(argv[i]);
				elevationRange=Math::Interval<double>(elevationMin,elevationMax);
				}
			else if(strcasecmp(argv[i]+1,"hmp")==0)
				{
				/* Read height mapping plane coefficients: */
				haveHeightMapPlane=true;
				double hmp[4];
				for(int j=0;j<4;++j)
					{
					++i;
					hmp[j]=atof(argv[i]);
					}
				heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
				heightMapPlane.normalize();
				}
			else if(strcasecmp(argv[i]+1,"nas")==0)
				{
				++i;
				numAveragingSlots=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"sp")==0)
				{
				++i;
				minNumSamples=atoi(argv[i]);
				++i;
				maxVariance=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"he")==0)
				{
				++i;
				hysteresis=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wts")==0)
				{
				for(int j=0;j<2;++j)
					{
					++i;
					wtSize[j]=(unsigned int)(atoi(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"ws")==0)
				{
				++i;
				waterSpeed=atof(argv[i]);
				++i;
				waterMaxSteps=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"rer")==0)
				{
				++i;
				double rainElevationMin=atof(argv[i]);
				++i;
				double rainElevationMax=atof(argv[i]);
				rainElevationRange=Math::Interval<double>(rainElevationMin,rainElevationMax);
				}
			else if(strcasecmp(argv[i]+1,"rs")==0)
				{
				++i;
				rainStrength=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"evr")==0)
				{
				++i;
				evaporationRate=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"dds")==0)
				{
				++i;
				demDistScale=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wi")==0)
				{
				++i;
				windowIndex=atoi(argv[i]);
				
				/* Extend the list of render settings if an index beyond the end is selected: */
				while(int(renderSettings.size())<=windowIndex)
					renderSettings.push_back(renderSettings.back());
				
				/* Disable fixed projector view on the new render settings: */
				renderSettings.back().fixProjectorView=false;
				}
			else if(strcasecmp(argv[i]+1,"fpv")==0)
				{
				renderSettings.back().fixProjectorView=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the projector transformation file specified in the next argument: */
					++i;
					renderSettings.back().loadProjectorTransform(argv[i]);
					}
				}
			else if(strcasecmp(argv[i]+1,"nhs")==0)
				renderSettings.back().hillshade=false;
			else if(strcasecmp(argv[i]+1,"uhs")==0)
				renderSettings.back().hillshade=true;
			else if(strcasecmp(argv[i]+1,"ns")==0)
				renderSettings.back().useShadows=false;
			else if(strcasecmp(argv[i]+1,"us")==0)
				renderSettings.back().useShadows=true;
			else if(strcasecmp(argv[i]+1,"nhm")==0)
				{
				delete renderSettings.back().elevationColorMap;
				renderSettings.back().elevationColorMap=0;
				}
			else if(strcasecmp(argv[i]+1,"uhm")==0)
				{
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the height color map file specified in the next argument: */
					++i;
					renderSettings.back().loadHeightMap(argv[i]);
					}
				else
					{
					/* Load the default height color map: */
					renderSettings.back().loadHeightMap(CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME);
					}
				}
			else if(strcasecmp(argv[i]+1,"ncl")==0)
				renderSettings.back().useContourLines=false;
			else if(strcasecmp(argv[i]+1,"ucl")==0)
				{
				renderSettings.back().useContourLines=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Read the contour line spacing: */
					++i;
					renderSettings.back().contourLineSpacing=GLfloat(atof(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"clw")==0)
				{
				++i;
				renderSettings.back().contourLineWidth=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"rst")==0)
				{
				++i;
				renderSettings.back().reliefStrength=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"rws")==0)
				renderSettings.back().renderWaterSurface=true;
			else if(strcasecmp(argv[i]+1,"rwt")==0)
				renderSettings.back().renderWaterSurface=false;
			else if(strcasecmp(argv[i]+1,"wo")==0)
				{
				++i;
				renderSettings.back().waterOpacity=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"cp")==0)
				{
				++i;
				controlPipeName=argv[i];
				}
			else
				std::cerr<<"Ignoring unrecognized command line switch "<<argv[i]<<std::endl;
			}
		}
	
	/* Print usage help if requested: */
	if(printHelp)
		printUsage();
	
	if(frameFilePrefix!=0)
		{
		/* Open the selected pre-recorded 3D video files: */
		std::string colorFileName=frameFilePrefix;
		colorFileName.append(".color");
		std::string depthFileName=frameFilePrefix;
		depthFileName.append(".depth");
		camera=new Kinect::FileFrameSource(IO::openFile(colorFileName.c_str()),IO::openFile(depthFileName.c_str()));
		}
	else if(kinectServerName!=0)
		{
		/* Split the server name into host name and port: */
		const char* colonPtr=0;
		for(const char* snPtr=kinectServerName;*snPtr!='\0';++snPtr)
			if(*snPtr==':')
				colonPtr=snPtr;
		std::string hostName;
		int port;
		if(colonPtr!=0)
			{
			/* Extract host name and port: */
			hostName=std::string(kinectServerName,colonPtr);
			port=atoi(colonPtr+1);
			}
		else
			{
			/* Use complete host name and default port: */
			hostName=kinectServerName;
			port=26000;
			}
		
		/* Open a multiplexed frame source for the given server host name and port number: */
		Kinect::MultiplexedFrameSource* source=Kinect::MultiplexedFrameSource::create(Comm::openTCPPipe(hostName.c_str(),port));
		
		/* Use the server's first component stream as the camera device: */
		camera=source->getStream(0);
		}
	else
		{
		/* Open the 3D camera device of the selected index: */
		Kinect::DirectFrameSource* realCamera=Kinect::openDirectFrameSource(cameraIndex,false);
		Misc::ConfigurationFileSection cameraConfigurationSection=cfg.getSection(cameraConfiguration.c_str());
		realCamera->configure(cameraConfigurationSection);
		camera=realCamera;
		}
	for(int i=0;i<2;++i)
		frameSize[i]=camera->getActualFrameSize(Kinect::FrameSource::DEPTH)[i];

	/* Get the camera's per-pixel depth correction parameters and evaluate it on the depth frame's pixel grid: */
	Kinect::FrameSource::DepthCorrection* depthCorrection=camera->getDepthCorrectionParameters();
	if(depthCorrection!=0)
		{
		pixelDepthCorrection=depthCorrection->getPixelCorrection(frameSize);
		delete depthCorrection;
		}
	else
		{
		/* Create dummy per-pixel depth correction parameters: */
		pixelDepthCorrection=new PixelDepthCorrection[frameSize[1]*frameSize[0]];
		PixelDepthCorrection* pdcPtr=pixelDepthCorrection;
		for(unsigned int y=0;y<frameSize[1];++y)
			for(unsigned int x=0;x<frameSize[0];++x,++pdcPtr)
				{
				pdcPtr->scale=1.0f;
				pdcPtr->offset=0.0f;
				}
		}
	
	/* Get the camera's intrinsic parameters: */
	cameraIps=camera->getIntrinsicParameters();

	/* Create the calibration target extractor. It is idle until a projector
	   calibration is started, and its parameters describe the recommended target:
	   a flat disk about 12 cm across, such as a CD with a paper face. */
	diskExtractor=new Kinect::DiskExtractor(camera->getActualFrameSize(Kinect::FrameSource::DEPTH),
	                                        camera->getDepthCorrectionParameters(),cameraIps);
	/* maxBlobMergeDist is in raw 11-bit disparity units, not cm: 1 was at or
	   below the sensor's speckle noise floor, which fragmented a real disk into
	   multiple sub-threshold blobs. diskFlatness/diskRadiusMargin are in cm on
	   the PCA-fitted extents and were tight enough that a few degrees of
	   hand-held tilt already failed the shape test; a 6cm-radius disk tilted
	   ~25 degrees has about 2.5cm of through-depth spread and ~10% foreshortening
	   on one in-plane axis, so both are loosened to tolerate that plus noise. */
	/* The shape test is not tilt-invariant: DiskExtractor runs its PCA in depth
	   image space, where x and y are pixels and z is raw disparity, so a tilted
	   target's fitted axes come out skewed and it fails the radius test even
	   though it is a perfectly good disk. Nothing to be done about that short of
	   patching the extractor, so the gates are loose instead -- affordable now
	   that the calibration blanks the sand out and the only foreground left is
	   the target and the hand holding it. The rim overlay shows what was
	   accepted, so a bad take is visible rather than silent. */
	diskExtractor->setMaxBlobMergeDist(4);
	diskExtractor->setMinNumPixels(300);
	diskExtractor->setDiskRadius(6.0);
	diskExtractor->setDiskRadiusMargin(1.5); // Accepts a 4-9cm fitted radius
	diskExtractor->setDiskFlatness(4.0);

	/* Render the raw 3D video as the backdrop of the calibration view, the way
	   the standalone CalibrateProjector does. Untextured and unlit: the sandbox
	   does not stream color, and a flat colour is more legible projected on sand
	   than a shaded surface. */
	calibrationFacade=new Kinect::ProjectorType(*camera);
	calibrationFacade->setTriangleDepthRange(4); // Same step the extractor merges blobs across
	calibrationFacade->setExtrinsicParameters(Kinect::FrameSource::ExtrinsicParameters::identity);
	#if KINECT_CONFIG_USE_PROJECTOR2
	calibrationFacade->setMapTexture(false);
	calibrationFacade->setIlluminate(false);
	#endif

	/* The projector matrix lives beside the layout file: */
	projectionMatrixFileName=CONFIG_CONFIGDIR;
	projectionMatrixFileName.push_back('/');
	projectionMatrixFileName.append(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);

	/* Read the sandbox layout file: */
	Geometry::Plane<double,3> basePlane;
	{
	IO::ValueSource layoutSource(IO::openFile(sandboxLayoutFileName.c_str()));
	layoutSource.skipWs();
	
	/* Read the base plane equation: */
	std::string s=layoutSource.readLine();
	basePlane=Misc::ValueCoder<Geometry::Plane<double,3> >::decode(s.c_str(),s.c_str()+s.length());
	basePlane.normalize();
	
	/* Read the corners of the base quadrilateral and project them into the base plane: */
	for(int i=0;i<4;++i)
		{
		layoutSource.skipWs();
		s=layoutSource.readLine();
		basePlaneCorners[i]=basePlane.project(Misc::ValueCoder<Geometry::Point<double,3> >::decode(s.c_str(),s.c_str()+s.length()));
		}
	}
	
	/* Limit the valid elevation range to the intersection of the extents of all height color maps: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->elevationColorMap!=0)
			{
			Math::Interval<double> mapRange(rsIt->elevationColorMap->getScalarRangeMin(),rsIt->elevationColorMap->getScalarRangeMax());
			elevationRange.intersectInterval(mapRange);
			}
	
	/* Scale all sizes by the given scale factor: */
	double sf=scale/100.0; // Scale factor from cm to final units
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			cameraIps.depthProjection.getMatrix()(i,j)*=sf;
	basePlane=Geometry::Plane<double,3>(basePlane.getNormal(),basePlane.getOffset()*sf);
	for(int i=0;i<4;++i)
		for(int j=0;j<3;++j)
			basePlaneCorners[i][j]*=sf;
	if(elevationRange!=Math::Interval<double>::full)
		elevationRange*=sf;
	if(rainElevationRange!=Math::Interval<double>::full)
		rainElevationRange*=sf;
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		if(rsIt->elevationColorMap!=0)
			rsIt->elevationColorMap->setScalarRange(rsIt->elevationColorMap->getScalarRangeMin()*sf,rsIt->elevationColorMap->getScalarRangeMax()*sf);
		rsIt->contourLineSpacing*=sf;
		rsIt->waterOpacity/=sf;
		for(int i=0;i<4;++i)
			rsIt->projectorTransform.getMatrix()(i,3)*=sf;
		}
	rainStrength*=sf;
	evaporationRate*=sf;
	demDistScale*=sf;
	
	/* Create the frame filter object: */
	frameFilter=new FrameFilter(frameSize,numAveragingSlots,pixelDepthCorrection,cameraIps.depthProjection,basePlane);
	frameFilter->setValidElevationInterval(cameraIps.depthProjection,basePlane,elevationRange.getMin(),elevationRange.getMax());
	frameFilter->setStableParameters(minNumSamples,maxVariance);
	frameFilter->setHysteresis(hysteresis);
	frameFilter->setSpatialFilter(true);
	frameFilter->setOutputFrameFunction(Misc::createFunctionCall(this,&Sandbox::receiveFilteredFrame));
	
	if(waterSpeed>0.0)
		{
		/* Create the hand extractor object: */
		handExtractor=new HandExtractor(frameSize,pixelDepthCorrection,cameraIps.depthProjection);
		}
	
	/* Start streaming depth frames. The color stream is never requested: nothing
	   displays it. The facade builds meshes on its own thread, but only from the
	   frames submitted to it, which is only while calibrating. */
	#if !KINECT_CONFIG_USE_SHADERPROJECTOR
	calibrationFacade->startStreaming(Misc::createFunctionCall(this,&Sandbox::facadeMeshCallback));
	#endif
	camera->startStreaming(0,Misc::createFunctionCall(this,&Sandbox::rawDepthFrameDispatcher));
	
	/* Create the depth image renderer: */
	depthImageRenderer=new DepthImageRenderer(frameSize);
	depthImageRenderer->setIntrinsics(cameraIps);
	depthImageRenderer->setBasePlane(basePlane);
	
	updateBoxTransform(basePlane);
	
	/* Calculate a bounding box around all potential surfaces: */
	bbox=Box::empty;
	for(int i=0;i<4;++i)
		{
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMin());
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMax());
		}
	
	if(waterSpeed>0.0)
		{
		/* Initialize the water flow simulator: */
		waterTable=new WaterTable2(wtSize[0],wtSize[1],depthImageRenderer,basePlaneCorners);
		waterTable->setElevationRange(elevationRange.getMin(),rainElevationRange.getMax());
		waterTable->setWaterDeposit(evaporationRate);
		
		/* Register a render function with the water table: */
		addWaterFunction=Misc::createFunctionCall(this,&Sandbox::addWater);
		waterTable->addRenderFunction(addWaterFunction);
		addWaterFunctionRegistered=true;
		}
	
	if(useRemoteServer)
		{
		/* Create a remote server: */
		try
			{
			remoteServer=new RemoteServer(this,remoteServerPortId,1.0/30.0);
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedConsoleError("Sandbox: Unable to create remote server on port %d due to exception %s",remoteServerPortId,err.what());
			}
		}
	
	/* Initialize all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		/* Calculate the texture mapping plane for this renderer's height map: */
		if(rsIt->elevationColorMap!=0)
			{
			if(haveHeightMapPlane)
				rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
			else
				rsIt->elevationColorMap->calcTexturePlane(depthImageRenderer);
			}
		
		/* Initialize the surface renderer: */
		rsIt->surfaceRenderer=new SurfaceRenderer(depthImageRenderer);
		rsIt->surfaceRenderer->setDrawContourLines(rsIt->useContourLines);
		rsIt->surfaceRenderer->setContourLineDistance(rsIt->contourLineSpacing);
		rsIt->surfaceRenderer->setContourLineWidth(rsIt->contourLineWidth);
		rsIt->surfaceRenderer->setReliefStrength(rsIt->reliefStrength);
		rsIt->surfaceRenderer->setElevationColorMap(rsIt->elevationColorMap);
		rsIt->surfaceRenderer->setIlluminate(rsIt->hillshade);
		if(waterTable!=0)
			{
			if(rsIt->renderWaterSurface)
				{
				/* Create a water renderer: */
				rsIt->waterRenderer=new WaterRenderer(waterTable);
				}
			else
				{
				rsIt->surfaceRenderer->setWaterTable(waterTable);
				rsIt->surfaceRenderer->setAdvectWaterTexture(true);
				rsIt->surfaceRenderer->setWaterOpacity(rsIt->waterOpacity);
				}
			}
		rsIt->surfaceRenderer->setDemDistScale(demDistScale);
		}
	
	/* Create a fixed-position light source if any render setting uses hill shading.
	   Without this the only light is the viewer's headlight, which for a
	   projector-over-sandbox geometry is nearly frontal on a nearly horizontal
	   surface, so n dot l is close to 1 everywhere and the shading carries no
	   directional information at all.

	   The direction is given as an azimuth and elevation in degrees rather than a
	   hardcoded vector so it can be matched to the physical rig without
	   rebuilding; the cartographic convention for relief shading is an azimuth of
	   315 degrees and an elevation of 45 degrees. Both are also settable live
	   through the control pipe. */
	bool anyHillshade=false;
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->hillshade)
			anyHillshade=true;
	if(anyHillshade)
		{
		sun=Vrui::getLightsourceManager()->createLightsource(true);
		for(int i=0;i<Vrui::getNumViewers();++i)
			Vrui::getViewer(i)->setHeadlightState(false);
		sun->enable();
		setSunDirection(sunAzimuth,sunElevation);
		}
	
	if(!controlPipeName.empty())
		{
		/* Create both FIFOs if they are not there yet, so neither the user nor the
		   launch script has to. An existing FIFO is fine; anything else at that
		   path is left alone and reported when the open fails below. */
		statusPipeName=controlPipeName+".status";
		mkfifo(controlPipeName.c_str(),0666);
		mkfifo(statusPipeName.c_str(),0666);

		/* Open the control pipe in non-blocking mode: */
		controlPipeFd=open(controlPipeName.c_str(),O_RDONLY|O_NONBLOCK);
		if(controlPipeFd<0)
			std::cerr<<"Unable to open control pipe "<<controlPipeName<<"; ignoring"<<std::endl;

		/* Report state back on a second pipe alongside the first, so neither side
		   needs a separate option for it. Opened lazily in sendStatus(), since a
		   panel may come and go while the sandbox runs.
		   This must be set before the menu is built: the menu entries that talk to
		   the panel are only created when a status pipe is configured. */
		statusPipeName=controlPipeName+".status";

		/* A panel exiting between our open() and write() would otherwise raise
		   SIGPIPE and terminate the sandbox: */
		signal(SIGPIPE,SIG_IGN);
		}

	/* Create the GUI: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	if(waterTable!=0)
		waterControlDialog=createWaterControlDialog();
	
	/* Initialize the custom tool classes: */
	GlobalWaterTool::initClass(*Vrui::getToolManager());
	LocalWaterTool::initClass(*Vrui::getToolManager());
	DEMTool::initClass(*Vrui::getToolManager());
	if(waterTable!=0)
		BathymetrySaverTool::initClass(waterTable,*Vrui::getToolManager());
	addEventTool("Pause Topography",0,0);
	addEventTool("Show Control Menu",0,1);


	/* Inhibit the screen saver: */
	Vrui::inhibitScreenSaver();
	
	/* Set the linear unit to support proper scaling: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::METER,scale/100.0));
	}

Sandbox::~Sandbox(void)
	{
	/* Stop streaming depth frames: */
	camera->stopStreaming();
	delete camera;
	delete frameFilter;
	
	/* Delete helper objects: */
	delete waterTable;
	delete depthImageRenderer;
	delete handExtractor;
	delete calibrationFacade;
	delete addWaterFunction;
	delete[] pixelDepthCorrection;
	delete remoteServer;
	
	delete mainMenu;
	delete waterControlDialog;
	
	close(controlPipeFd);
	}

void Sandbox::toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData)
	{
	/* Check if the destroyed tool is the active DEM tool: */
	if(activeDem==dynamic_cast<DEM*>(cbData->tool))
		{
		/* Deactivate the active DEM tool: */
		activeDem=0;
		}
	}

namespace {

/****************
Helper functions:
****************/

std::vector<std::string> tokenizeLine(const char*& buffer)
	{
	std::vector<std::string> result;
	
	/* Skip initial whitespace but not end-of-line: */
	const char* bPtr=buffer;
	while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
		++bPtr;
	
	/* Extract white-space separated tokens until a newline or end-of-string are encountered: */
	while(*bPtr!='\0'&&*bPtr!='\n')
		{
		/* Remember the start of the current token: */
		const char* tokenStart=bPtr;
		
		/* Find the end of the current token: */
		while(*bPtr!='\0'&&!isspace(*bPtr))
			++bPtr;
		
		/* Extract the token: */
		result.push_back(std::string(tokenStart,bPtr));
		
		/* Skip whitespace but not end-of-line: */
		while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
			++bPtr;
		}
	
	/* Skip end-of-line: */
	if(*bPtr=='\n')
		++bPtr;
	
	/* Set the start of the next line and return the token list: */
	buffer=bPtr;
	return result;
	}

bool isToken(const std::string& token,const char* pattern)
	{
	return strcasecmp(token.c_str(),pattern)==0;
	}

}

void Sandbox::frame(void)
	{
	/* Call the remote server's frame method: */
	if(remoteServer!=0)
		remoteServer->frame(Vrui::getApplicationTime());
	
	/* Check if the filtered frame has been updated: */
	if(filteredFrames.lockNewValue())
		{
		/* Update the depth image renderer's depth image: */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());

		/* Feed the same frame into a base plane measurement if one is running: */
		lastFilteredFrame=filteredFrames.getLockedValue();
		}

	if(handExtractor!=0)
		{
		/* Lock the most recent extracted hand list: */
		handExtractor->lockNewExtractedHands();
		
		#if 0
		
		/* Register/unregister the rain rendering function based on whether hands have been detected: */
		bool registerWaterFunction=!handExtractor->getLockedExtractedHands().empty();
		if(addWaterFunctionRegistered!=registerWaterFunction)
			{
			if(registerWaterFunction)
				waterTable->addRenderFunction(addWaterFunction);
			else
				waterTable->removeRenderFunction(addWaterFunction);
			addWaterFunctionRegistered=registerWaterFunction;
			}
		
		#endif
		}
	
	/* Update all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
	
	/* Check if there is a control command on the control pipe: */
	if(controlPipeFd>=0)
		{
		/* Try reading a chunk of data (will fail with EAGAIN if no data due to non-blocking access): */
		char commandBuffer[1024];
		ssize_t readResult=read(controlPipeFd,commandBuffer,sizeof(commandBuffer)-1);
		if(readResult>0)
			{
			commandBuffer[readResult]='\0';
			
			/* Extract commands line-by-line: */
			const char* cPtr=commandBuffer;
			while(*cPtr!='\0')
				{
				/* Split the current line into tokens and skip empty lines: */
				std::vector<std::string> tokens=tokenizeLine(cPtr);
				if(tokens.empty())
					continue;
				
				/* Parse the command: */
				if(isToken(tokens[0],"waterSpeed"))
					{
					if(tokens.size()==2)
						{
						waterSpeed=atof(tokens[1].c_str());
						if(waterSpeedSlider!=0)
							waterSpeedSlider->setValue(waterSpeed);
						}
					else
						std::cerr<<"Wrong number of arguments for waterSpeed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterMaxSteps"))
					{
					if(tokens.size()==2)
						{
						waterMaxSteps=atoi(tokens[1].c_str());
						if(waterMaxStepsSlider!=0)
							waterMaxStepsSlider->setValue(waterMaxSteps);
						}
					else
						std::cerr<<"Wrong number of arguments for waterMaxSteps control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterAttenuation"))
					{
					if(tokens.size()==2)
						{
						double attenuation=atof(tokens[1].c_str());
						if(waterTable!=0)
							waterTable->setAttenuation(GLfloat(1.0-attenuation));
						if(waterAttenuationSlider!=0)
							waterAttenuationSlider->setValue(attenuation);
						}
					else
						std::cerr<<"Wrong number of arguments for waterAttenuation control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"drainWater"))
					{
					/* Setting the water level needs a GL context, which the pipe
					   command thread does not have; defer to the next display()
					   call, same as the grid read-back requests do. */
					if(waterTable!=0)
						drainWaterRequested=true;
					}
				else if(isToken(tokens[0],"colorMap"))
					{
					if(tokens.size()==2)
						{
						try
							{
							/* Update all height color maps: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								if(rsIt->elevationColorMap!=0)
									rsIt->elevationColorMap->load(tokens[1].c_str());
							}
						catch(const std::runtime_error& err)
							{
							std::cerr<<"Cannot read height color map "<<tokens[1]<<" due to exception "<<err.what()<<std::endl;
							}
						}
					else
						std::cerr<<"Wrong number of arguments for colorMap control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"heightMapPlane"))
					{
					if(tokens.size()==5)
						{
						/* Read the height map plane equation: */
						double hmp[4];
						for(int i=0;i<4;++i)
							hmp[i]=atof(tokens[1+i].c_str());
						Plane heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
						heightMapPlane.normalize();
						
						/* Override the height mapping planes of all elevation color maps: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->elevationColorMap!=0)
								rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
						}
					else
						std::cerr<<"Wrong number of arguments for heightMapPlane control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"useContourLines"))
					{
					if(tokens.size()==2)
						{
						/* Parse the command parameter: */
						if(isToken(tokens[1],"on")||isToken(tokens[1],"off"))
							{
							/* Enable or disable contour lines on all surface renderers: */
							bool useContourLines=isToken(tokens[1],"on");
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setDrawContourLines(useContourLines);
							}
						else
							std::cerr<<"Invalid parameter "<<tokens[1]<<" for useContourLines control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for contourLineSpacing control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"contourLineSpacing"))
					{
					if(tokens.size()==2)
						{
						/* Parse the contour line distance: */
						GLfloat contourLineSpacing=GLfloat(atof(tokens[1].c_str()));
						
						/* Check if the requested spacing is valid: */
						if(contourLineSpacing>0.0f)
							{
							/* Override the contour line spacing of all surface renderers: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setContourLineDistance(contourLineSpacing);
							}
						else
							std::cerr<<"Invalid parameter "<<contourLineSpacing<<" for contourLineSpacing control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for contourLineSpacing control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"contourLineWidth"))
					{
					if(tokens.size()==2)
						{
						/* Parse the contour line width: */
						GLfloat contourLineWidth=GLfloat(atof(tokens[1].c_str()));

						/* Check if the requested width is valid: */
						if(contourLineWidth>0.0f)
							{
							/* Override the contour line width of all surface renderers: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setContourLineWidth(contourLineWidth);
							}
						else
							std::cerr<<"Invalid parameter "<<contourLineWidth<<" for contourLineWidth control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for contourLineWidth control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"pauseUpdates"))
					{
					if(tokens.size()==2)
						{
						pauseUpdates=isToken(tokens[1],"on");

						/* Keep the in-application toggle in step, so the two user
						   interfaces cannot disagree about the pause state: */
						if(pauseUpdatesToggle!=0)
							pauseUpdatesToggle->setToggle(pauseUpdates);
						}
					else
						std::cerr<<"Wrong number of arguments for pauseUpdates control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"calibrateProjector"))
					{
					if(tokens.size()>=2&&isToken(tokens[1],"capture"))
						captureTiePoint();
					else if(tokens.size()>=2&&isToken(tokens[1],"abort"))
						abortProjectorCalibration();
					else if(tokens.size()>=4&&isToken(tokens[1],"start"))
						{
						unsigned int w=(unsigned int)(atoi(tokens[2].c_str()));
						unsigned int h=(unsigned int)(atoi(tokens[3].c_str()));
						unsigned int n=tokens.size()>=5?(unsigned int)(atoi(tokens[4].c_str())):12U;
						if(w>0&&h>0)
							startProjectorCalibration(w,h,n);
						else
							std::cerr<<"Invalid projector size for calibrateProjector control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Usage: calibrateProjector start <width> <height> [points] | capture | abort"<<std::endl;
					}
				else if(isToken(tokens[0],"grabDepth"))
					{
					if(tokens.size()==2)
						grabDepthImage(tokens[1].c_str());
					else
						std::cerr<<"Usage: grabDepth <file>"<<std::endl;
					}
				else if(isToken(tokens[0],"fitPlane"))
					{
					if(tokens.size()==5)
						fitPlaneToRegion((unsigned int)(atoi(tokens[1].c_str())),(unsigned int)(atoi(tokens[2].c_str())),
						                 (unsigned int)(atoi(tokens[3].c_str())),(unsigned int)(atoi(tokens[4].c_str())));
					else
						std::cerr<<"Usage: fitPlane <x0> <y0> <x1> <y1>"<<std::endl;
					}
				else if(isToken(tokens[0],"projectorView"))
					{
					/* The escape hatch. A calibration that puts the sand outside
					   the projector frustum renders a black window, and without
					   this the only way back is to edit a file and restart. */
					if(tokens.size()==2)
						{
						bool on=isToken(tokens[1],"on");
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->fixProjectorView=on&&rsIt->projectorTransformValid;
						}
					else
						std::cerr<<"Usage: projectorView on|off"<<std::endl;
					}
				else if(isToken(tokens[0],"setExtents"))
					{
					if(tokens.size()==5)
						setExtentsFromRegion((unsigned int)(atoi(tokens[1].c_str())),(unsigned int)(atoi(tokens[2].c_str())),
						                     (unsigned int)(atoi(tokens[3].c_str())),(unsigned int)(atoi(tokens[4].c_str())));
					else
						std::cerr<<"Usage: setExtents <x0> <y0> <x1> <y1>"<<std::endl;
					}
				else if(isToken(tokens[0],"extractPoint"))
					{
					if(tokens.size()==3)
						extractPoint((unsigned int)(atoi(tokens[1].c_str())),(unsigned int)(atoi(tokens[2].c_str())));
					else
						std::cerr<<"Usage: extractPoint <x> <y>"<<std::endl;
					}
				else if(isToken(tokens[0],"writeLayout"))
					writeSandboxLayout();
				else if(isToken(tokens[0],"resetLayout"))
					resetLayoutCapture();
				else if(isToken(tokens[0],"reliefStrength"))
					{
					if(tokens.size()==2)
						{
						/* Parse the relief shading strength: */
						GLfloat reliefStrength=GLfloat(atof(tokens[1].c_str()));

						/* Check if the requested strength is valid: */
						if(reliefStrength>=0.0f&&reliefStrength<=1.0f)
							{
							/* Override the relief strength of all surface renderers: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setReliefStrength(reliefStrength);
							}
						else
							std::cerr<<"Invalid parameter "<<reliefStrength<<" for reliefStrength control pipe command; must be in [0, 1]"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for reliefStrength control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"sunDirection"))
					{
					if(tokens.size()==3)
						{
						/* Parse the azimuth and elevation and re-aim the fixed light source: */
						setSunDirection(GLfloat(atof(tokens[1].c_str())),GLfloat(atof(tokens[2].c_str())));
						}
					else
						std::cerr<<"Wrong number of arguments for sunDirection control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBed"))
					{
					if(tokens.size()==2&&isToken(tokens[1],"off"))
						{
						/* Disable dipping bed rendering on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDrawDippingBed(false);
						}
					else if(tokens.size()==5)
						{
						/* Read the dipping bed plane equation: */
						GLfloat dbp[4];
						for(int i=0;i<4;++i)
							dbp[i]=GLfloat(atof(tokens[1+i].c_str()));
						SurfaceRenderer::Plane dippingBedPlane=SurfaceRenderer::Plane(SurfaceRenderer::Plane::Vector(dbp),dbp[3]);
						dippingBedPlane.normalize();
						
						/* Enable dipping bed rendering and set the dipping bed plane equation on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedPlane(dippingBedPlane);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"foldedDippingBed"))
					{
					if(tokens.size()==6)
						{
						/* Read the dipping bed coefficients: */
						GLfloat dbc[5];
						for(int i=0;i<5;++i)
							dbc[i]=GLfloat(atof(tokens[1+i].c_str()));
						
						/* Enable dipping bed rendering and set the dipping bed coefficients on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedCoeffs(dbc);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for foldedDippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBedThickness"))
					{
					if(tokens.size()==2)
						{
						/* Read the dipping bed thickness: */
						float dippingBedThickness=float(atof(tokens[1].c_str()));
						
						/* Set the dipping bed thickness on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDippingBedThickness(dippingBedThickness);
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBedThickness control pipe command"<<std::endl;
					}
				else
					std::cerr<<"Unrecognized control pipe command "<<tokens[0]<<std::endl;
				}
			}
		}
	
	/* The facade renders whatever mesh is locked in, and only this call locks a
	   newer one: */
	if(calibratingProjector)
		calibrationFacade->updateFrames();

	/* Pick up the newest extracted calibration target, if any. The extractor only
	   reports when it finds a disk, never when one leaves, so presence is judged
	   by how recently it last reported: a flag set on arrival would latch on and
	   the marker would stay put after the target was taken away. */
	if(calibratingProjector&&lastDisk.lockNewValue())
		lastDiskTime=Vrui::getApplicationTime();
	haveDisk=calibratingProjector&&Vrui::getApplicationTime()-lastDiskTime<0.3;

	/* Push state to the control panel a couple of times a second. Sending it per
	   frame would be 60 writes a second for values a human is reading. */
	if(!statusPipeName.empty()&&Vrui::getApplicationTime()>=nextStatusTime)
		{
		nextStatusTime=Vrui::getApplicationTime()+0.5;
		sendStatus();
		}

	if(frameRateTextField!=0&&Vrui::getWidgetManager()->isVisible(waterControlDialog))
		{
		/* Update the frame rate display: */
		frameRateTextField->setValue(1.0/Vrui::getCurrentFrameTime());
		}
	
	if(pauseUpdates)
		Vrui::scheduleUpdate(Vrui::getApplicationTime()+1.0/30.0);
	}

void Sandbox::drawCalibrationView(GLContextData& contextData,const int viewport[4]) const
	{
	/* The same view the standalone CalibrateProjector projects: the sandbox area
	   seen straight down, with the extracted disk drawn where the extractor puts
	   it. Unlike the topography this is drawn in camera space, so it does not
	   depend on the calibration being solved yet. */
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE); // The disk faces the camera, so its winding depends on which way rotateFromTo turned it
	glLineWidth(2.0f);

	/* Fit the sandbox area to the window, keeping its aspect ratio: */
	Box box=Box::empty;
	for(int c=0;c<4;++c)
		box.addPoint(boxTransform.transform(basePlaneCorners[c]));
	double bw=box.getSize(0);
	double bh=box.getSize(1);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	if(bw*double(viewport[3])>=double(viewport[2])*bh) // Sandbox area is wider than the window
		{
		double filler=Math::div2((bw*double(viewport[3]))/double(viewport[2])-bh);
		glOrtho(box.min[0],box.max[0],box.min[1]-filler,box.max[1]+filler,-200.0,200.0);
		}
	else
		{
		double filler=Math::div2((bh*double(viewport[2]))/double(viewport[3])-bw);
		glOrtho(box.min[0]-filler,box.max[0]+filler,box.min[1],box.max[1],-200.0,200.0);
		}
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrix(boxTransform);

	/* The live 3D video, as a solid yellow facade: */
	glColor3f(1.0f,1.0f,0.0f);
	calibrationFacade->glRenderAction(contextData);
	glDisable(GL_DEPTH_TEST); // The projector turns depth testing back on

	/* The sandbox outline, in the corner order the layout file stores: */
	glColor3f(1.0f,1.0f,0.0f);
	glBegin(GL_LINE_LOOP);
	glVertex(basePlaneCorners[0]);
	glVertex(basePlaneCorners[1]);
	glVertex(basePlaneCorners[3]);
	glVertex(basePlaneCorners[2]);
	glEnd();

	/* The blob the extractor is currently calling a disk, at its extracted
	   position, orientation and size -- so a wrong detection is visible as a
	   circle in the wrong place or of the wrong size: */
	if(haveDisk)
		{
		const Kinect::DiskExtractor::Disk& disk=lastDisk.getLockedValue();
		glPushMatrix();
		glTranslate(disk.center-Kinect::DiskExtractor::Point::origin);
		glRotate(Vrui::Rotation::rotateFromTo(Vrui::Vector(0,0,1),Vrui::Vector(disk.normal)));
		glColor3f(0.0f,0.0f,1.0f); // Blue: the one colour that cannot be confused with the yellow facade
		glBegin(GL_POLYGON);
		for(int i=0;i<32;++i)
			{
			double angle=2.0*Math::Constants<double>::pi*double(i)/32.0;
			glVertex3d(Math::cos(angle)*disk.radius,Math::sin(angle)*disk.radius,0.0);
			}
		glEnd();
		glPopMatrix();
		}

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopAttrib();
	}

void Sandbox::display(GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Get the rendering settings for this window: */
	const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
	const Vrui::VRWindow* window=ds.window;
	int windowIndex;
	for(windowIndex=0;windowIndex<Vrui::getNumWindows()&&window!=Vrui::getWindow(windowIndex);++windowIndex)
		;
	const RenderSettings& rs=windowIndex<int(renderSettings.size())?renderSettings[windowIndex]:renderSettings.back();
	
	/* Check if the water simulation state needs to be updated: */
	if(waterTable!=0&&dataItem->waterTableTime!=Vrui::getApplicationTime())
		{
		/* Retrieve a potential pending grid read-back request: */
		GridRequest::Request request=gridRequest.getRequest();
		
		/* Update the water table's bathymetry grid: */
		waterTable->updateBathymetry(contextData);

		/* Check if a drain was requested. Done after updateBathymetry, not before:
		   setWaterLevel adapts the given level to the *current* terrain, so the
		   result is exactly dry rather than a flat pool wherever the sand sits
		   above the water table's lowest possible elevation. */
		if(drainWaterRequested)
			{
			const GLsizei* wtGridSize=waterTable->getSize();
			std::vector<GLfloat> dry(size_t(wtGridSize[0])*size_t(wtGridSize[1]),GLfloat(waterTable->getDomain().min[2]));
			waterTable->setWaterLevel(dry.data(),contextData);
			drainWaterRequested=false;
			}

		/* Check if the grid request is active and wants bathymetry data: */
		if(request.isActive()&&request.bathymetryBuffer!=0)
			{
			/* Read back the current bathymetry grid: */
			waterTable->bindBathymetryTexture(contextData);
			glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,request.bathymetryBuffer);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
			}
		
		/* Run the water flow simulation's main pass: */
		GLfloat totalTimeStep=GLfloat(Vrui::getFrameTime()*waterSpeed);
		unsigned int numSteps=0;
		while(numSteps<waterMaxSteps-1U&&totalTimeStep>1.0e-8f)
			{
			/* Run with a self-determined time step to maintain stability: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(false,contextData);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		#if 0
		if(totalTimeStep>1.0e-8f)
			{
			std::cout<<'.'<<std::flush;
			/* Force the final step to avoid simulation slow-down: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(true,contextData);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		#else
		if(totalTimeStep>1.0e-8f)
			{
			/* The simulation could not consume the frame's worth of time within
			   waterMaxSteps, so the water is running slow. Printing this every
			   frame buries everything else in the log at 30 lines a second, so
			   report it at most once every few seconds and say what to do. */
			static double nextSlowReport=0.0;
			if(Vrui::getApplicationTime()>=nextSlowReport)
				{
				nextSlowReport=Vrui::getApplicationTime()+5.0;
				std::cout<<"Water simulation is behind by "<<totalTimeStep
				         <<" s per frame; raise waterMaxSteps (currently "<<waterMaxSteps
				         <<") or lower waterTableSize in SARndbox.cfg"<<std::endl;
				}
			}
		#endif
		
		/* Check if the grid request is active and wants water level data: */
		if(request.isActive()&&request.waterLevelBuffer!=0)
			{
			/* Read back the current water level grid: */
			waterTable->bindQuantityTexture(contextData);
			glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,request.waterLevelBuffer);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
			}
		
		/* Finish an active grid request: */
		if(request.isActive())
			request.complete();
		
		/* Mark the water simulation state as up-to-date for this frame: */
		dataItem->waterTableTime=Vrui::getApplicationTime();
		}
	
	/* Calculate the projection matrix: */
	PTransform projection=ds.projection;
	if(rs.fixProjectorView&&rs.projectorTransformValid)
		{
		/* Use the projector transformation instead: */
		projection=rs.projectorTransform;
		
		/* Multiply with the inverse modelview transformation so that lighting still works as usual: */
		projection*=Geometry::invert(ds.modelviewNavigational);
		}
	
	if(calibratingProjector)
		{
		/* Replace the topography with the calibration view while calibrating, so
		   the operator can see whether the target is currently being detected: */
		drawCalibrationView(contextData,ds.viewport);
		}
	else
		{
		if(rs.hillshade)
			{
			/* Set the surface material: */
			glMaterial(GLMaterialEnums::FRONT,rs.surfaceMaterial);
			}

		#if 0
	if(rs.hillshade&&rs.useShadows)
		{
		/* Set up OpenGL state: */
		glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_POLYGON_BIT);
		
		GLLightTracker& lt=*contextData.getLightTracker();
		
		/* Save the currently-bound frame buffer and viewport: */
		GLint currentFrameBuffer;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
		GLint currentViewport[4];
		glGetIntegerv(GL_VIEWPORT,currentViewport);
		
		/*******************************************************************
		First rendering pass: Global ambient illumination only
		*******************************************************************/
		
		/* Draw the surface mesh: */
		surfaceRenderer->glRenderGlobalAmbientHeightMap(dataItem->heightColorMapObject,contextData);
		
		/*******************************************************************
		Second rendering pass: Add local illumination for every light source
		*******************************************************************/
		
		/* Enable additive rendering: */
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		
		for(int lightSourceIndex=0;lightSourceIndex<lt.getMaxNumLights();++lightSourceIndex)
			if(lt.getLightState(lightSourceIndex).isEnabled())
				{
				/***************************************************************
				First step: Render to the light source's shadow map
				***************************************************************/
				
				/* Set up OpenGL state to render to the shadow map: */
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
				glViewport(0,0,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
				glDepthMask(GL_TRUE);
				glClear(GL_DEPTH_BUFFER_BIT);
				glCullFace(GL_FRONT);
				
				/*************************************************************
				Calculate the shadow projection matrix:
				*************************************************************/
				
				/* Get the light source position in eye space: */
				Geometry::HVector<float,3> lightPosEc;
				glGetLightfv(GL_LIGHT0+lightSourceIndex,GL_POSITION,lightPosEc.getComponents());
				
				/* Transform the light source position to camera space: */
				Vrui::ONTransform::HVector lightPosCc=Vrui::getDisplayState(contextData).modelviewNavigational.inverseTransform(Vrui::ONTransform::HVector(lightPosEc));
				
				/* Calculate the direction vector from the center of the bounding box to the light source: */
				Point bboxCenter=Geometry::mid(bbox.min,bbox.max);
				Vrui::Vector lightDirCc=Vrui::Vector(lightPosCc.getComponents())-Vrui::Vector(bboxCenter.getComponents())*lightPosCc[3];
				
				/* Build a transformation that aligns the light direction with the positive z axis: */
				Vrui::ONTransform shadowModelview=Vrui::ONTransform::rotate(Vrui::Rotation::rotateFromTo(lightDirCc,Vrui::Vector(0,0,1)));
				shadowModelview*=Vrui::ONTransform::translateToOriginFrom(bboxCenter);
				
				/* Create a projection matrix, based on whether the light is positional or directional: */
				PTransform shadowProjection(0.0);
				if(lightPosEc[3]!=0.0f)
					{
					/* Modify the modelview transformation such that the light source is at the origin: */
					shadowModelview.leftMultiply(Vrui::ONTransform::translate(Vrui::Vector(0,0,-lightDirCc.mag())));
					
					/***********************************************************
					Create a perspective projection:
					***********************************************************/
					
					/* Calculate the perspective bounding box of the surface bounding box in eye space: */
					Box pBox=Box::empty;
					for(int i=0;i<8;++i)
						{
						Point bc=shadowModelview.transform(bbox.getVertex(i));
						pBox.addPoint(Point(-bc[0]/bc[2],-bc[1]/bc[2],-bc[2]));
						}
					
					/* Upload the frustum matrix: */
					double l=pBox.min[0]*pBox.min[2];
					double r=pBox.max[0]*pBox.min[2];
					double b=pBox.min[1]*pBox.min[2];
					double t=pBox.max[1]*pBox.min[2];
					double n=pBox.min[2];
					double f=pBox.max[2];
					shadowProjection.getMatrix()(0,0)=2.0*n/(r-l);
					shadowProjection.getMatrix()(0,2)=(r+l)/(r-l);
					shadowProjection.getMatrix()(1,1)=2.0*n/(t-b);
					shadowProjection.getMatrix()(1,2)=(t+b)/(t-b);
					shadowProjection.getMatrix()(2,2)=-(f+n)/(f-n);
					shadowProjection.getMatrix()(2,3)=-2.0*f*n/(f-n);
					shadowProjection.getMatrix()(3,2)=-1.0;
					}
				else
					{
					/***********************************************************
					Create a perspective projection:
					***********************************************************/
					
					/* Transform the bounding box with the modelview transformation: */
					Box bboxEc=bbox;
					bboxEc.transform(shadowModelview);
					
					/* Upload the ortho matrix: */
					double l=bboxEc.min[0];
					double r=bboxEc.max[0];
					double b=bboxEc.min[1];
					double t=bboxEc.max[1];
					double n=-bboxEc.max[2];
					double f=-bboxEc.min[2];
					shadowProjection.getMatrix()(0,0)=2.0/(r-l);
					shadowProjection.getMatrix()(0,3)=-(r+l)/(r-l);
					shadowProjection.getMatrix()(1,1)=2.0/(t-b);
					shadowProjection.getMatrix()(1,3)=-(t+b)/(t-b);
					shadowProjection.getMatrix()(2,2)=-2.0/(f-n);
					shadowProjection.getMatrix()(2,3)=-(f+n)/(f-n);
					shadowProjection.getMatrix()(3,3)=1.0;
					}
				
				/* Multiply the shadow modelview matrix onto the shadow projection matrix: */
				shadowProjection*=shadowModelview;
				
				/* Draw the surface into the shadow buffer: */
				surfaceRenderer->glRenderDepthOnly(shadowProjection,contextData);
				
				/* Reset OpenGL state: */
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
				glViewport(currentViewport[0],currentViewport[1],currentViewport[2],currentViewport[3]);
				glCullFace(GL_BACK);
				glDepthMask(GL_FALSE);
				
				#if SAVEDEPTH
				/* Save the depth image: */
				{
				glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
				GLfloat* depthTextureImage=new GLfloat[dataItem->shadowBufferSize[1]*dataItem->shadowBufferSize[0]];
				glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depthTextureImage);
				glBindTexture(GL_TEXTURE_2D,0);
				Images::RGBImage dti(dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
				GLfloat* dtiPtr=depthTextureImage;
				Images::RGBImage::Color* ciPtr=dti.modifyPixels();
				for(int y=0;y<dataItem->shadowBufferSize[1];++y)
					for(int x=0;x<dataItem->shadowBufferSize[0];++x,++dtiPtr,++ciPtr)
						{
						GLColor<GLfloat,3> tc(*dtiPtr,*dtiPtr,*dtiPtr);
						*ciPtr=tc;
						}
				delete[] depthTextureImage;
				Images::writeImageFile(dti,"DepthImage.png");
				}
				#endif
				
				/* Draw the surface using the shadow texture: */
				rs.surfaceRenderer->glRenderShadowedIlluminatedHeightMap(dataItem->heightColorMapObject,dataItem->shadowDepthTextureObject,shadowProjection,contextData);
				}
		
		/* Reset OpenGL state: */
		glPopAttrib();
		}
	else
	#endif
		{
		/* Render the surface in a single pass: */
		rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData);
		}
	
	if(rs.waterRenderer!=0)
		{
		/* Draw the water surface: */
		glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,0.5f,0.8f));
		glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
		glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		rs.waterRenderer->render(projection,ds.modelviewNavigational,contextData);
		glDisable(GL_BLEND);
		}
	
	/* Call the remote server's render method: */
	if(remoteServer!=0)
		{
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrix(projection);
		glMatrixMode(GL_MODELVIEW);
		remoteServer->glRenderAction(contextData);
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		}
		}

	/* Draw the projector calibration target over everything else. It is drawn in
	   raw window pixels rather than in the scene, because its whole purpose is to
	   mark an exact position in the projected image: */
	if(calibratingProjector)
		{
		const int* vp=ds.viewport;

		glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT|GL_CURRENT_BIT);
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0,double(vp[2]),0.0,double(vp[3]),-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		/* The target is defined in projector image pixels; scale into the viewport
		   in case the window is not exactly the calibrated size. y runs bottom-up,
		   matching both the ortho projection above and the clip-space mapping the
		   solved matrix is scaled by -- drawing the cross flipped while recording
		   the tie point unflipped fits a mirror-image projector, which still
		   passes through every tie point and inverts parallax with height. */
		Geometry::Point<double,2> t=getTiePointTarget(tiePointIndex);
		double tx=t[0]*double(vp[2])/double(projectorImageSize[0]);
		double ty=t[1]*double(vp[3])/double(projectorImageSize[1]);

		/* Always white: this cross is only the reference point to place the target
		   on. Whether the target is detected is said by the green disk. */
		glColor3f(1.0f,1.0f,1.0f);

		const double arm=40.0;
		glLineWidth(3.0f);
		glBegin(GL_LINES);
		glVertex2d(tx-arm,ty);
		glVertex2d(tx+arm,ty);
		glVertex2d(tx,ty-arm);
		glVertex2d(tx,ty+arm);
		glEnd();

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopAttrib();
		}
	}

void Sandbox::resetNavigation(void)
	{
	/* Construct a navigation transformation to center the sandbox area in the display, facing the viewer, with the long sandbox axis facing to the right: */
	Vrui::NavTransform nav=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	nav*=Vrui::NavTransform::scale(Vrui::getDisplaySize()/boxSize);
	Vrui::Vector y=Vrui::getUpDirection();
	Vrui::Vector z=Vrui::getForwardDirection();
	Vrui::Vector x=z^y;
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(x,y));
	nav*=boxTransform;
	Vrui::setNavigationTransformation(nav);
	}

void Sandbox::eventCallback(Vrui::Application::EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		switch(eventId)
			{
			case 0:
				/* Invert the current pause setting: */
				pauseUpdates=!pauseUpdates;

				/* The toggle only exists if a GLMotif menu was built, which it no
				   longer is; guard rather than assume: */
				if(pauseUpdatesToggle!=0)
					pauseUpdatesToggle->setToggle(pauseUpdates);

				break;

			case 1:
				/* Right click: ask the control panel to pop up its menu at the
				   pointer. Drawing the menu in Qt rather than GLMotif means it is a
				   real desktop menu, and it keeps the projected image clean. */
				if(!sendEvent("showMenu"))
					showPanelCallback(0);

				break;
			}
		}
	}

void Sandbox::initContext(GLContextData& contextData) const
	{
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	{
	/* Save the currently bound frame buffer: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* Set the default shadow buffer size: */
	dataItem->shadowBufferSize[0]=1024;
	dataItem->shadowBufferSize[1]=1024;
	
	/* Generate the shadow rendering frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->shadowFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
	
	/* Generate a depth texture for shadow rendering: */
	glGenTextures(1,&dataItem->shadowDepthTextureObject);
	glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE_ARB,GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC_ARB,GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE_ARB,GL_INTENSITY);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24_ARB,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1],0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	
	/* Attach the depth texture to the frame buffer object: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,dataItem->shadowDepthTextureObject,0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	}

	}

VRUI_APPLICATION_RUN(Sandbox)
