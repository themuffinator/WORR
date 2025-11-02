//
// Copyright(C) 2016-2017 Samuel Villarreal
// Copyright(C) 2016 Night Dive Studios, Inc.
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

#include "progs/common.inc"

begin_cbuffer(PostProcess_MSAA, 0)
    cbuffer_member(vec4, uRTSize);
end_cbuffer()

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec4);
    var_attrib(1, vec2);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord,     TEXCOORD0)
    def_var_out(vec4, out_smaaCoord1,   TEXCOORD1)
    def_var_out(vec4, out_smaaCoord2,   TEXCOORD2)
    def_var_out(vec4, out_smaaCoord3,   TEXCOORD3)
    def_var_out(vec4, out_smaaCoord4,   TEXCOORD4)
end_output

#define SMAA_MAX_SEARCH_STEPS 32.0

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    outVarPosition(output, position)    = inVarAttrib(0, input);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);

    vec2 tc = inVarAttrib(1, input);
    
    // We will use these offsets for the searches later on (see @PSEUDO_GATHER4):
    outVar(output, out_smaaCoord1) = mad(uRTSize.xyxy, vec4(-0.25, -0.125,  1.25, -0.125), tc.xyxy);
    outVar(output, out_smaaCoord2) = mad(uRTSize.xyxy, vec4(-0.125, -0.25, -0.125,  1.25), tc.xyxy);

    // And these for the searches, they indicate the ends of the loops:
    outVar(output, out_smaaCoord3) = mad(uRTSize.xxyy,
        vec4(-2.0, 2.0, -2.0, 2.0) * SMAA_MAX_SEARCH_STEPS,
        vec4(outVar(output, out_smaaCoord1).xz,
        outVar(output, out_smaaCoord2).yw));

    outVar(output, out_smaaCoord4) = vec4(tc * uRTSize.zw, 0.0, 0.0);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord,      TEXCOORD0)
    def_var_in(vec4, out_smaaCoord1,    TEXCOORD1)
    def_var_in(vec4, out_smaaCoord2,    TEXCOORD2)
    def_var_in(vec4, out_smaaCoord3,    TEXCOORD3)
    def_var_in(vec4, out_smaaCoord4,    TEXCOORD4)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tEdgeRenderTarget, 0);
def_sampler(2D, tArea, 1);
def_sampler(2D, tSearch, 2);

#define SMAA_MAX_SEARCH_STEPS_DIAG 16
#define SMAA_CORNER_ROUNDING 25

#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20.0
#define SMAA_AREATEX_PIXEL_SIZE (1.0 / vec2(160.0, 560.0))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE vec2(66.0, 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE vec2(64.0, 16.0)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

/**
 * Conditional move:
 */
void SMAAMovc(bvec2 cond, inout vec2 variable, vec2 value)
{
    flatten if (cond.x) variable.x = value.x;
    flatten if (cond.y) variable.y = value.y;
}

void SMAAMovc(bvec4 cond, inout vec4 variable, vec4 value)
{
    SMAAMovc(cond.xy, variable.xy, value.xy);
    SMAAMovc(cond.zw, variable.zw, value.zw);
}

/**
 * Allows to decode two binary values from a bilinear-filtered access.
 */
vec2 SMAADecodeDiagBilinearAccess(vec2 e)
{
    // Bilinear access for fetching 'e' have a 0.25 offset, and we are
    // interested in the R and G edges:
    //
    // +---G---+-------+
    // |   x o R   x   |
    // +-------+-------+
    //
    // Then, if one of these edge is enabled:
    //   Red:   (0.75 * X + 0.25 * 1) => 0.25 or 1.0
    //   Green: (0.75 * 1 + 0.25 * X) => 0.75 or 1.0
    //
    // This function will unpack the values (mad + mul + round):
    // wolframalpha.com: round(x * abs(5 * x - 5 * 0.75)) plot 0 to 1
    e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
    return round(e);
}

vec4 SMAADecodeDiagBilinearAccess(vec4 e)
{
    e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
    return round(e);
}

/**
 * These functions allows to perform diagonal pattern searches.
 */
vec2 SMAASearchDiag1(vec2 texcoord, vec2 dir, out vec2 e)
{
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    vec3 t = vec3(uRTSize.xy, 1.0);
    while(coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9)
    {
        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);
        e = sampleLevelZero(tEdgeRenderTarget, coord.xy).rg;
        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

vec2 SMAASearchDiag2(vec2 texcoord, vec2 dir, out vec2 e)
{
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    coord.x += 0.25 * uRTSize.x; // See @SearchDiag2Optimization
    vec3 t = vec3(uRTSize.xy, 1.0);
    while(coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9)
    {
        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);

        // @SearchDiag2Optimization
        // Fetch both edges at once using bilinear filtering:
        e = sampleLevelZero(tEdgeRenderTarget, coord.xy).rg;
        e = SMAADecodeDiagBilinearAccess(e);

        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

/** 
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
vec2 SMAAArea(vec2 dist, float e1, float e2, float offset)
{
    // Rounding prevents precision errors of bilinear filtering:
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * vec2(e1, e2)), dist);
    
    // We do a scale and bias for mapping to texel space:
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

    // Move to proper place, according to the subpixel offset:
    texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);

    // Do it!
    return sampleLevelZero(tArea, texcoord).rg;
}

/** 
 * Similar to SMAAArea, this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
vec2 SMAAAreaDiag(vec2 dist, vec2 e, float offset)
{
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);

    // We do a scale and bias for mapping to texel space:
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

    // Diagonal areas are on the second half of the texture:
    texcoord.x += 0.5;

    // Move to proper place, according to the subpixel offset:
    texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;

    // Do it!
    return sampleLevelZero(tArea, texcoord).rg;
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
vec2 SMAACalculateDiagWeights(vec2 texcoord, vec2 e, vec4 subsampleIndices)
{
    vec2 weights = vec2(0.0, 0.0);

    // Search for the line ends:
    vec4 d;
    vec2 end;
    if (e.r > 0.0)
    {
        d.xz = SMAASearchDiag1(texcoord, vec2(-1.0,  1.0), end);
        d.x += float(end.y > 0.9);
    }
    else
    {
        d.xz = vec2(0.0, 0.0);
    }
    
    d.yw = SMAASearchDiag1(texcoord, vec2(1.0, -1.0), end);

    branch
    if (d.x + d.y > 2.0)
    { 
        // d.x + d.y + 1 > 3
        // Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25), uRTSize.xyxy, texcoord.xyxy);
        vec4 c;
        c.xy = sampleLODOffset(tEdgeRenderTarget, coords.xy, 0, ivec2(-1,  0)).rg;
        c.zw = sampleLODOffset(tEdgeRenderTarget, coords.zw, 0, ivec2( 1,  0)).rg;
        c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);

        // Merge crossing edges at each side into a single value:
        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        // Remove the crossing edge if we didn't found the end of the line:
        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        // Fetch the areas for this line:
        weights += SMAAAreaDiag(d.xy, cc, subsampleIndices.z);
    }

    // Search for the line ends:
    d.xz = SMAASearchDiag2(texcoord, vec2(-1.0, -1.0), end);
    if(sampleLODOffset(tEdgeRenderTarget, texcoord, 0, ivec2(1, 0)).r > 0.0)
    {
        d.yw = SMAASearchDiag2(texcoord, vec2(1.0, 1.0), end);
        d.y += float(end.y > 0.9);
    }
    else
    {
        d.yw = vec2(0.0, 0.0);
    }

    branch
    if(d.x + d.y > 2.0) // d.x + d.y + 1 > 3
    { 
        // Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x, -d.x, d.y, d.y), uRTSize.xyxy, texcoord.xyxy);
        vec4 c;
        c.x  = sampleLODOffset(tEdgeRenderTarget, coords.xy, 0, ivec2(-1,  0)).g;
        c.y  = sampleLODOffset(tEdgeRenderTarget, coords.xy, 0, ivec2( 0, -1)).r;
        c.zw = sampleLODOffset(tEdgeRenderTarget, coords.zw, 0, ivec2( 1,  0)).gr;
        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        // Remove the crossing edge if we didn't found the end of the line:
        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        // Fetch the areas for this line:
        weights += SMAAAreaDiag(d.xy, cc, subsampleIndices.w).gr;
    }

    return weights;
}

//-----------------------------------------------------------------------------
// Horizontal/Vertical Search Functions

/**
 * This allows to determine how much length should we add in the last step
 * of the searches. It takes the bilinearly interpolated edge (see 
 * @PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and
 * crossing edges are active.
 */
float SMAASearchLength(vec2 e, float offset) {
    // The texture is flipped vertically, with left and right cases taking half
    // of the space horizontally:
    vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
    vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);

    // Scale and bias to access texel centers:
    scale += vec2(-1.0,  1.0);
    bias  += vec2( 0.5, -0.5);

    // Convert from pixel coordinates to texcoords:
    // (We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped)
    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;

    // Lookup the search texture:
    return sampleLevelZero(tSearch, mad(scale, e, bias)).r;
}

/**
 * Horizontal/vertical search functions for the 2nd pass.
 */
float SMAASearchXLeft(vec2 texcoord, float end)
{
    /**
     * @PSEUDO_GATHER4
     * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to
     * sample between edge, thus fetching four edges in a row.
     * Sampling with different offsets in each direction allows to disambiguate
     * which edges are active from the four fetched ones.
     */
    vec2 e = vec2(0.0, 1.0);
    while(texcoord.x > end && 
           e.g > 0.8281 && // Is there some edge not activated?
           e.r == 0.0) { // Or is there a crossing edge that breaks the line?
        e = sampleLevelZero(tEdgeRenderTarget, texcoord).rg;
        texcoord = mad(-vec2(2.0, 0.0), uRTSize.xy, texcoord);
    }

    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e, 0.0), 3.25);
    return mad(uRTSize.x, offset, texcoord.x);
}

float SMAASearchXRight(vec2 texcoord, float end)
{
    vec2 e = vec2(0.0, 1.0);
    while(texcoord.x < end && 
           e.g > 0.8281 && // Is there some edge not activated?
           e.r == 0.0) { // Or is there a crossing edge that breaks the line?
        e = sampleLevelZero(tEdgeRenderTarget, texcoord).rg;
        texcoord = mad(vec2(2.0, 0.0), uRTSize.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e, 0.5), 3.25);
    return mad(-uRTSize.x, offset, texcoord.x);
}

float SMAASearchYUp(vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y > end && 
           e.r > 0.8281 && // Is there some edge not activated?
           e.g == 0.0) { // Or is there a crossing edge that breaks the line?
        e = sampleLevelZero(tEdgeRenderTarget, texcoord).rg;
        texcoord = mad(-vec2(0.0, 2.0), uRTSize.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.0), 3.25);
    return mad(uRTSize.y, offset, texcoord.y);
}

float SMAASearchYDown(vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y < end && 
           e.r > 0.8281 && // Is there some edge not activated?
           e.g == 0.0) { // Or is there a crossing edge that breaks the line?
        e = sampleLevelZero(tEdgeRenderTarget, texcoord).rg;
        texcoord = mad(vec2(0.0, 2.0), uRTSize.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.5), 3.25);
    return mad(-uRTSize.y, offset, texcoord.y);
}

//-----------------------------------------------------------------------------
// Corner Detection Functions

void SMAADetectHorizontalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d)
{
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y; // Reduce blending for pixels in the center of a line.

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * sampleLODOffset(tEdgeRenderTarget, texcoord.xy, 0, ivec2(0,  1)).r;
    factor.x -= rounding.y * sampleLODOffset(tEdgeRenderTarget, texcoord.zw, 0, ivec2(1,  1)).r;
    factor.y -= rounding.x * sampleLODOffset(tEdgeRenderTarget, texcoord.xy, 0, ivec2(0, -2)).r;
    factor.y -= rounding.y * sampleLODOffset(tEdgeRenderTarget, texcoord.zw, 0, ivec2(1, -2)).r;

    weights *= saturate(factor);
}

void SMAADetectVerticalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d)
{
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * sampleLODOffset(tEdgeRenderTarget, texcoord.xy, 0, ivec2( 1, 0)).g;
    factor.x -= rounding.y * sampleLODOffset(tEdgeRenderTarget, texcoord.zw, 0, ivec2( 1, 1)).g;
    factor.y -= rounding.x * sampleLODOffset(tEdgeRenderTarget, texcoord.xy, 0, ivec2(-2, 0)).g;
    factor.y -= rounding.y * sampleLODOffset(tEdgeRenderTarget, texcoord.zw, 0, ivec2(-2, 1)).g;

    weights *= saturate(factor);
}

//-----------------------------------------------------------------------------
// Blending Weight Calculation Pixel Shader (Second Pass)

vec4 SMAABlendingWeightCalculationPS(vec2 texcoord, vec2 pixcoord, vec4 offset[3], vec4 subsampleIndices)
{
    vec4 weights = vec4(0.0, 0.0, 0.0, 0.0);

    vec2 e = sample(tEdgeRenderTarget, texcoord).rg;

    branch
    if(e.g > 0.0) // Edge at north
    {
        // Diagonals have both north and west edges, so searching for them in
        // one of the boundaries is enough.
        weights.rg = SMAACalculateDiagWeights(texcoord, e, subsampleIndices);

        // We give priority to diagonals, so if we find a diagonal we skip 
        // horizontal/vertical processing.
        branch
        if(weights.r == -weights.g) // weights.r + weights.g == 0.0
        {
            vec2 d;

            // Find the distance to the left:
            vec3 coords;
            coords.x = SMAASearchXLeft(offset[0].xy, offset[2].x);
            coords.y = offset[1].y; // offset[1].y = texcoord.y - 0.25 * uRTSize.y (@CROSSING_OFFSET)
            d.x = coords.x;

            // Now fetch the left crossing edges, two at a time using bilinear
            // filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to
            // discern what value each edge has:
            float e1 = sampleLevelZero(tEdgeRenderTarget, coords.xy).r;

            // Find the distance to the right:
            coords.z = SMAASearchXRight(offset[0].zw, offset[2].y);
            d.y = coords.z;

            // We want the distances to be in pixel units (doing this here allow to
            // better interleave arithmetic and memory accesses):
            d = abs(round(mad(uRTSize.zz, d, -pixcoord.xx)));

            // SMAAArea below needs a sqrt, as the areas texture is compressed
            // quadratically:
            vec2 sqrt_d = sqrt(d);

            // Fetch the right crossing edges:
            float e2 = sampleLODOffset(tEdgeRenderTarget, coords.zy, 0, ivec2(1, 0)).r;

            // Ok, we know how this pattern looks like, now it is time for getting
            // the actual area:
            weights.rg = SMAAArea(sqrt_d, e1, e2, subsampleIndices.y);

            // Fix corners:
            coords.y = texcoord.y;
            SMAADetectHorizontalCornerPattern(weights.rg, coords.xyzy, d);

        }
        else
        {
            e.r = 0.0; // Skip vertical processing.
        }
    }

    branch
    if(e.r > 0.0) // Edge at west
    { 
        vec2 d;

        // Find the distance to the top:
        vec3 coords;
        coords.y = SMAASearchYUp(offset[1].xy, offset[2].z);
        coords.x = offset[0].x; // offset[1].x = texcoord.x - 0.25 * uRTSize.x;
        d.x = coords.y;

        // Fetch the top crossing edges:
        float e1 = sampleLevelZero(tEdgeRenderTarget, coords.xy).g;

        // Find the distance to the bottom:
        coords.z = SMAASearchYDown(offset[1].zw, offset[2].w);
        d.y = coords.z;

        // We want the distances to be in pixel units:
        d = abs(round(mad(uRTSize.ww, d, -pixcoord.yy)));

        // SMAAArea below needs a sqrt, as the areas texture is compressed 
        // quadratically:
        vec2 sqrt_d = sqrt(d);

        // Fetch the bottom crossing edges:
        float e2 = sampleLODOffset(tEdgeRenderTarget, coords.xz, 0, ivec2(0, 1)).g;

        // Get the area for this direction:
        weights.ba = SMAAArea(sqrt_d, e1, e2, subsampleIndices.x);

        // Fix corners:
        coords.x = texcoord.x;
        SMAADetectVerticalCornerPattern(weights.ba, coords.xyxz, d);
    }

    return weights;
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 vOffset[3];
    vOffset[0] = inVar(input, out_smaaCoord1);
    vOffset[1] = inVar(input, out_smaaCoord2);
    vOffset[2] = inVar(input, out_smaaCoord3);
    
    vec2 vFragCoord = inVar(input, out_smaaCoord4).xy;
    vec4 vSubSamples = vec4(0.0, 0.0, 0.0, 0.0);
    
    vec4 vColor = SMAABlendingWeightCalculationPS(
        inVar(input, out_texcoord),
        vFragCoord,
        vOffset,
        vSubSamples);
    
    outVarFragment(output, fragment) = vColor;
    outReturn(output)
}

#endif
