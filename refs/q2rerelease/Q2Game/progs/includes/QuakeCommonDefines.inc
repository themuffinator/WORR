//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#ifdef GLSLC_NX
    #define RT_DIFFUSE_TARGET       def_var_pixelTarget(vec4, 0)
    #define RT_NORMALS_TARGET       def_var_pixelTarget(vec4, 1)
    #define RT_LIGHTBUFFER_TARGET   def_var_pixelTarget(vec4, 2)
    #define RT_GLOWBUFFER_TARGET    def_var_pixelTarget(vec4, 3)
    
    #define RT_DIFFUSE              outVarPixelTarget(output, 0)
    #define RT_NORMALS              outVarPixelTarget(output, 1)
    #define RT_LIGHTBUFFER          outVarPixelTarget(output, 2)
    #define RT_GLOWBUFFER           outVarPixelTarget(output, 3)
#else
    #define RT_DIFFUSE_TARGET       def_var_pixelTarget(vec4, 0)
    #define RT_MOTIONBLUR_TARGET    def_var_pixelTarget(vec4, 1)
    #define RT_NORMALS_TARGET       def_var_pixelTarget(vec4, 2)
    #define RT_LIGHTBUFFER_TARGET   def_var_pixelTarget(vec4, 3)
    #define RT_GLOWBUFFER_TARGET    def_var_pixelTarget(vec4, 4)
    
    #define RT_DIFFUSE              outVarPixelTarget(output, 0)
    #define RT_MOTIONBLUR           outVarPixelTarget(output, 1)
    #define RT_NORMALS              outVarPixelTarget(output, 2)
    #define RT_LIGHTBUFFER          outVarPixelTarget(output, 3)
    #define RT_GLOWBUFFER           outVarPixelTarget(output, 4)
#endif

