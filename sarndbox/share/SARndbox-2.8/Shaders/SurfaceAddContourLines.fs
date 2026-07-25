/***********************************************************************
SurfaceAddContourLines - Shader fragment to add topographic contour
lines extracted from a half-pixel offset 2D elevation map to a surface's
base color.
Copyright (c) 2012 Oliver Kreylos

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

#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect pixelCornerElevationSampler;
uniform float contourLineFactor;

/* Width of a normal contour line in screen pixels, set from the application
   via -clw or the contourLineWidth control pipe command. An unset uniform
   reads as 0.0, which still yields thin but visible lines: */
uniform float contourLineWidth;

/***********************************************************************
Anti-aliased contour lines.

The original algorithm made a binary decision per pixel and wrote opaque
black, which produced hard one-pixel-wide aliased lines, and used a
checkerboard parity term to thin 4-connected runs, which stippled
shallow-slope contours into dashes.

This version instead measures the screen-space distance from the pixel
centre to the nearest contour and converts it into a coverage value.
The elevation gradient is computed analytically from the same four
corner samples the original algorithm already fetched, so no derivative
instructions are needed and the result is exact at triangle edges.

SurfaceRenderer installs an IO::FileMonitor on this file, so the tuning
constants below may be edited while the sandbox is running.
***********************************************************************/

/* Index contour lines are drawn this much wider than normal ones: */
const float indexLineWidthFactor=1.875;

/* Every indexLineStep'th contour is drawn as a heavier index contour; set
   to 1.0 or less to disable index contours entirely: */
const float indexLineStep=5.0;

/* Color of topographic contour lines: */
const vec3 lineColor=vec3(0.0,0.0,0.0);

/* Contours fade out where consecutive lines crowd closer together than
   fadeStartPx screen pixels, and reach full strength at fadeEndPx. This
   keeps near-vertical sand walls from turning into a solid black smear,
   and suppresses the spurious ring at the outer edge of the surface,
   where the pixel corner elevation buffer's clear value meets the
   reconstructed sand surface. Set fadeStartPx=0.0 and fadeEndPx=0.001 to
   disable the fade: */
const float fadeStartPx=5.0;
const float fadeEndPx=12.0;

void addContourLines(in vec2 fragCoord,inout vec4 baseColor)
	{
	/* Evaluate the half-pixel offset elevation texture at the pixel's four corners: */
	float c00=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x    ,fragCoord.y    )).r;
	float c10=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x+1.0,fragCoord.y    )).r;
	float c01=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x    ,fragCoord.y+1.0)).r;
	float c11=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x+1.0,fragCoord.y+1.0)).r;

	/* Calculate the elevation at the pixel's centre and the elevation gradient
	   across the pixel, in elevation units per screen pixel: */
	float e =(c00+c10+c01+c11)*0.25;
	float gx=((c10+c11)-(c00+c01))*0.5;
	float gy=((c01+c11)-(c00+c10))*0.5;

	/* Move to contour index space, in which the contour lines are exactly the
	   integers, and express the gradient as contours crossed per screen pixel: */
	float f=e*contourLineFactor;
	float grad=length(vec2(gx,gy))*contourLineFactor;

	/* Guard against a zero gradient, which occurs on plateaus where the frame
	   filter has frozen a run of pixels to bit-identical elevations: */
	float safeGrad=max(grad,1.0e-8);

	/* Find the nearest contour and the screen-space distance to it: */
	float idx=floor(f+0.5);
	float dPix=abs(f-idx)/safeGrad;

	/* Index contours are drawn heavier than intermediate ones: */
	float w=contourLineWidth;
	if(indexLineStep>1.0&&mod(idx,indexLineStep)<0.5)
		w=contourLineWidth*indexLineWidthFactor;

	/* Calculate the exact box filter coverage of a straight line of width w
	   centred on the contour. This is crisper than a smoothstep of the same
	   nominal width, which matters because the projector adds blur of its own: */
	float cov=clamp((w*0.5+0.5)-dPix,0.0,1.0);

	/* Fade out where contours crowd together: */
	cov*=smoothstep(fadeStartPx,fadeEndPx,1.0/safeGrad);

	/* Composite the contour line over the base color. Blending rather than
	   overwriting matters because contour lines are added before illumination
	   and before the water color, so a submerged contour is tinted by the water
	   instead of staying stark black, and contours are shaded together with the
	   terrain when hill shading is enabled: */
	baseColor.rgb=mix(baseColor.rgb,lineColor,cov);
	}
