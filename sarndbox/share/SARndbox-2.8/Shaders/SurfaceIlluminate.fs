/***********************************************************************
SurfaceIlluminate - Shader fragment to modulate a surface's base color
with diffuse and specular colors computed during vertex lighting.
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

varying vec4 diffColor,specColor; // Diffuse and specular colors

/* Strength of the relief shading, from 0.0 (no shading, base color passed
   through unchanged) to 1.0 (full Lambertian illumination). Values well below
   1.0 add slope legibility while preserving the elevation color map's
   saturation, which matters on a projector where contrast is already
   contested by ambient light and the sand's own albedo. Set from the
   application via -rs or the reliefStrength control pipe command: */
uniform float reliefStrength;

void illuminate(inout vec4 baseColor)
	{
	/* Modulate the base color, treated as diffuse reflectivity, with the diffuse light color and add the specular light color: */
	vec4 litColor=baseColor*diffColor+specColor;

	/* Blend between the unlit and fully lit color: */
	baseColor=mix(baseColor,litColor,reliefStrength);
	}
