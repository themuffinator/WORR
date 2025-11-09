//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
    var_attrib(ATTRIB_COLOR, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)        = inVarAttrib(ATTRIB_TEXCOORD, input);
    outVar(output, out_color)           = inVarAttrib(ATTRIB_COLOR, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output


def_sampler(2D, tImage, 0);

begin_cbuffer(QuakeCRTParams, 0)
    cbuffer_member(vec4, uParams1);
    cbuffer_member(vec4, uParams2);
end_cbuffer()

#define hardScan uParams1.x//-6.0
#define hardPix uParams1.y//-3.0
#define warpX 0.0
#define warpY 0.0
#define maskDark uParams1.z//0.5
#define maskLight uParams1.w//1.5
#define scaleInLinearGamma uParams2.x//1
#define shadowMask uParams2.y//1
#define brightboost uParams2.z//2
#define hardBloomScan -2.0
#define hardBloomPix -1.5
#define bloomAmount 1.0/16.0
#define shape 2.0

//Uncomment to reduce instructions with simpler linearization
//(fixes HD3000 Sandy Bridge IGP)
//#define SIMPLE_LINEAR_GAMMA

#define warp vec2(warpX,warpY)

//------------------------------------------------------------------------

// sRGB to Linear.
// Assuing using sRGB typed textures this should not be needed.
#ifdef SIMPLE_LINEAR_GAMMA

float ToLinear1(float c)
{
   return c;
}

vec3 ToLinear(vec3 c)
{
   return c;
}

vec3 ToSrgb(vec3 c)
{
   return pow(c, 1.0 / 2.2);
}

#else
    
float ToLinear1(float c)
{
   if (scaleInLinearGamma==0) return c;
   return(c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);
}

vec3 ToLinear(vec3 c)
{
   if (scaleInLinearGamma==0) return c;
   return vec3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));
}

// Linear to sRGB.
// Assuming using sRGB typed textures this should not be needed.
float ToSrgb1(float c)
{
   if (scaleInLinearGamma==0) return c;
   return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);
}

vec3 ToSrgb(vec3 c)
{
   if (scaleInLinearGamma==0) return c;
   return vec3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));
}
#endif

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
vec3 Fetch(vec2 pos, vec2 off, vec2 texture_size){
  pos=(floor(pos*texture_size.xy+off)+vec2(0.5,0.5))/texture_size.xy;
#ifdef SIMPLE_LINEAR_GAMMA
  return ToLinear(brightboost * pow(sampleLevelZero(tImage,pos.xy).rgb, 2.2));
#else
  return ToLinear(brightboost * sampleLevelZero(tImage, pos.xy).rgb);
#endif
}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos, vec2 texture_size)
{
    pos = pos*texture_size.xy;
    return -((pos-floor(pos))-vec2(0.5, 0.5));
}
    
// 1D Gaussian.
float Gaus(float pos,float scale)
{
    return exp2(scale*pow(abs(pos),shape));
}

// 3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos, float off, vec2 texture_size)
{
    vec3 b=Fetch(pos,vec2(-1.0,off),texture_size);
    vec3 c=Fetch(pos,vec2( 0.0,off),texture_size);
    vec3 d=Fetch(pos,vec2( 1.0,off),texture_size);
    float dst=Dist(pos, texture_size).x;
    // Convert distance to weight.
    float scale=hardPix;
    float wb=Gaus(dst-1.0,scale);
    float wc=Gaus(dst+0.0,scale);
    float wd=Gaus(dst+1.0,scale);
    // Return filtered sample.
    return (b*wb+c*wc+d*wd)/(wb+wc+wd);
}
  
// 5-tap Gaussian filter along horz line.
vec3 Horz5(vec2 pos, float off, vec2 texture_size)
{
    vec3 a=Fetch(pos,vec2(-2.0,off),texture_size);
    vec3 b=Fetch(pos,vec2(-1.0,off),texture_size);
    vec3 c=Fetch(pos,vec2( 0.0,off),texture_size);
    vec3 d=Fetch(pos,vec2( 1.0,off),texture_size);
    vec3 e=Fetch(pos,vec2( 2.0,off),texture_size);
    float dst=Dist(pos, texture_size).x;
    // Convert distance to weight.
    float scale=hardPix;
    float wa=Gaus(dst-2.0,scale);
    float wb=Gaus(dst-1.0,scale);
    float wc=Gaus(dst+0.0,scale);
    float wd=Gaus(dst+1.0,scale);
    float we=Gaus(dst+2.0,scale);
    // Return filtered sample.
    return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);
}

// 7-tap Gaussian filter along horz line.
vec3 Horz7(vec2 pos, float off, vec2 texture_size)
{
    vec3 a=Fetch(pos,vec2(-3.0,off),texture_size);
    vec3 b=Fetch(pos,vec2(-2.0,off),texture_size);
    vec3 c=Fetch(pos,vec2(-1.0,off),texture_size);
    vec3 d=Fetch(pos,vec2( 0.0,off),texture_size);
    vec3 e=Fetch(pos,vec2( 1.0,off),texture_size);
    vec3 f=Fetch(pos,vec2( 2.0,off),texture_size);
    vec3 g=Fetch(pos,vec2( 3.0,off),texture_size);
    float dst=Dist(pos, texture_size).x;
    // Convert distance to weight.
    float scale=hardBloomPix;
    float wa=Gaus(dst-3.0,scale);
    float wb=Gaus(dst-2.0,scale);
    float wc=Gaus(dst-1.0,scale);
    float wd=Gaus(dst+0.0,scale);
    float we=Gaus(dst+1.0,scale);
    float wf=Gaus(dst+2.0,scale);
    float wg=Gaus(dst+3.0,scale);
    // Return filtered sample.
    return (a*wa+b*wb+c*wc+d*wd+e*we+f*wf+g*wg)/(wa+wb+wc+wd+we+wf+wg);
}

// Return scanline weight.
float Scan(vec2 pos,float off, vec2 texture_size)
{
  float dst=Dist(pos, texture_size).y;
  return Gaus(dst+off,hardScan);
}
  
  // Return scanline weight for bloom.
float BloomScan(vec2 pos,float off, vec2 texture_size)
{
  float dst=Dist(pos, texture_size).y;
  return Gaus(dst+off,hardBloomScan);
}

// Allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos, vec2 texture_size)
{
  vec3 a=Horz3(pos,-1.0, texture_size);
  vec3 b=Horz5(pos, 0.0, texture_size);
  vec3 c=Horz3(pos, 1.0, texture_size);
  float wa=Scan(pos,-1.0, texture_size);
  float wb=Scan(pos, 0.0, texture_size);
  float wc=Scan(pos, 1.0, texture_size);
  return a*wa+b*wb+c*wc;
}
  
// Small bloom.
vec3 Bloom(vec2 pos, vec2 texture_size)
{
  vec3 a=Horz5(pos,-2.0, texture_size);
  vec3 b=Horz7(pos,-1.0, texture_size);
  vec3 c=Horz7(pos, 0.0, texture_size);
  vec3 d=Horz7(pos, 1.0, texture_size);
  vec3 e=Horz5(pos, 2.0, texture_size);
  float wa=BloomScan(pos,-2.0, texture_size);
  float wb=BloomScan(pos,-1.0, texture_size);
  float wc=BloomScan(pos, 0.0, texture_size);
  float wd=BloomScan(pos, 1.0, texture_size);
  float we=BloomScan(pos, 2.0, texture_size);
  return a*wa+b*wb+c*wc+d*wd+e*we;
}

// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos)
{
  pos=pos*2.0-1.0;    
  pos*=vec2(1.0+(pos.y*pos.y)*warp.x,1.0+(pos.x*pos.x)*warp.y);
  return pos*0.5+0.5;
}

// Shadow mask 
vec3 Mask(vec2 pos) {
  vec3 mask=vec3(maskDark,maskDark,maskDark);

  // Very compressed TV style shadow mask.
  if (shadowMask == 1) {
    float mask_line = maskLight;
    float odd=0.0;
    if(frac(pos.x/6.0)<0.5) odd = 1.0;
    if(frac((pos.y+odd)/2.0)<0.5) mask_line = maskDark;  
    pos.x=frac(pos.x/3.0);
   
    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
    mask *= mask_line;  
  } 

  // Aperture-grille.
  else if (shadowMask == 2) {
    pos.x=frac(pos.x/3.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  } 

  // Stretched VGA style shadow mask (same as prior shaders).
  else if (shadowMask == 3) {
    pos.x+=pos.y*3.0;
    pos.x=frac(pos.x/6.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  }

  // VGA style shadow mask.
  else if (shadowMask == 4) {
    pos.xy=floor(pos.xy*vec2(1.0,0.5));
    pos.x+=pos.y*3.0;
    pos.x=frac(pos.x/6.0);

    if(pos.x<0.333)mask.r=maskLight;
    else if(pos.x<0.666)mask.g=maskLight;
    else mask.b=maskLight;
  }

  return mask;
}    

vec4 crt_lottes(vec2 texture_size, vec2 video_size, vec2 output_size, vec2 tex)
{
  vec2 pos=Warp(tex.xy*(texture_size.xy/video_size.xy))*(video_size.xy/texture_size.xy);
  vec3 outColor = Tri(pos, texture_size);

#ifdef DO_BLOOM
  //Add Bloom
  outColor.rgb+=Bloom(pos, texture_size)*bloomAmount;
#endif

  if(shadowMask != 0)
    outColor.rgb*=Mask(floor(tex.xy*(texture_size.xy/video_size.xy)*output_size.xy)+vec2(0.5,0.5));

  return vec4(ToSrgb(outColor.rgb),1.0);
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    const float fAspect = ScreenSize().x / ScreenSize().y;
    const float fWidth = 240.0 * fAspect;
    
    outVarFragment(output, fragment) =
        crt_lottes(
            vec2(fWidth, 240.0),
            vec2(fWidth, 240.0),
            ScreenSize(),
            inVar(input, out_texcoord));
            
    outReturn(output)
}

#endif
