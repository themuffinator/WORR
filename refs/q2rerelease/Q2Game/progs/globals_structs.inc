//
// Copyright(C) 2007-2014 Samuel Villarreal
// Copyright(C) 2015 Night Dive Studios, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#pragma once

begin_cbuffer(RenderView, 0) 
    cbuffer_member(mat4, uProjectionMatrix);
    cbuffer_member(mat4, uModelViewMatrix);
    cbuffer_member(mat4, uInverseProjectionMatrix);
    cbuffer_member(mat4, uPrevProjection);
    cbuffer_member(mat4, uInverseModelViewMatrix);
    cbuffer_member(mat4, uPrevModelView);
    cbuffer_member(mat4, uRotationMatrix);
    cbuffer_member(mat4, uInverseRotationMatrix);
    cbuffer_member(mat4, uTransposedRotationMatrix);
    cbuffer_member(mat4, uClipMatrix);
    cbuffer_member(mat4, uInverseClipMatrix);
    cbuffer_member(mat4, uPrevClipMatrix);
    cbuffer_member(mat4, uNormalMatrix);
    cbuffer_member(vec3, uViewOrigin);
    cbuffer_member(float, uViewWidth);
    cbuffer_member(float, uViewHeight);
    cbuffer_member(float, uZNear);
    cbuffer_member(float, uZFar);
    cbuffer_member(vec2, uInvFocalCoords);
    cbuffer_member(vec4, uResolutionScale);
    cbuffer_member(vec4, uScreenBounds);
    cbuffer_member(vec4, uFrustumCorners[4]);
end_cbuffer()
