#include "progs/SSAO/SSAO_defines.inc"

def_samplerArray(2D, viewspaceDepthSource, 0);
def_sampler(2D, tNormals, 1);
#if 0
def_samplerArray(2D, finalSSAO, 2);
def_samplerUint(1D, loadCounter, 3);
def_sampler(2D, importanceMap, 4);
#endif

static const vec4 g_samplePatternMain[INTELSSAO_MAIN_DISK_SAMPLE_COUNT] =
{
     vec4( 0.78488064,  0.56661671,  1.500000, -0.126083),
     vec4( 0.26022232, -0.29575172,  1.500000, -1.064030),
     vec4( 0.10459357,  0.08372527,  1.110000, -2.730563),
     vec4(-0.68286800,  0.04963045,  1.090000, -0.498827),
     vec4(-0.13570161, -0.64190155,  1.250000, -0.532765),
     vec4(-0.26193795, -0.08205118,  0.670000, -1.783245),
     vec4(-0.61177456,  0.66664219,  0.710000, -0.044234),
     vec4( 0.43675563,  0.25119025,  0.610000, -1.167283),
     vec4( 0.07884444,  0.86618668,  0.640000, -0.459002),
     vec4(-0.12790935, -0.29869005,  0.600000, -1.729424),
     vec4(-0.04031125,  0.02413622,  0.600000, -4.792042),
     vec4( 0.16201244, -0.52851415,  0.790000, -1.067055),
     vec4(-0.70991218,  0.47301072,  0.640000, -0.335236),
     vec4( 0.03277707, -0.22349690,  0.600000, -1.982384),
     vec4( 0.68921727,  0.36800742,  0.630000, -0.266718),
     vec4( 0.29251814,  0.37775412,  0.610000, -1.422520),
     vec4(-0.12224089,  0.96582592,  0.600000, -0.426142),
     vec4( 0.11071457, -0.16131058,  0.600000, -2.165947),
     vec4( 0.46562141, -0.59747696,  0.600000, -0.189760),
     vec4(-0.51548797,  0.11804193,  0.600000, -1.246800),
     vec4( 0.89141309, -0.42090443,  0.600000,  0.028192),
     vec4(-0.32402530, -0.01591529,  0.600000, -1.543018),
     vec4( 0.60771245,  0.41635221,  0.600000, -0.605411),
     vec4( 0.02379565, -0.08239821,  0.600000, -3.809046),
     vec4( 0.48951152, -0.23657045,  0.600000, -1.189011),
     vec4(-0.17611565, -0.81696892,  0.600000, -0.513724),
     vec4(-0.33930185, -0.20732205,  0.600000, -1.698047),
     vec4(-0.91974425,  0.05403209,  0.600000,  0.062246),
     vec4(-0.15064627, -0.14949332,  0.600000, -1.896062),
     vec4( 0.53180975, -0.35210401,  0.600000, -0.758838),
     vec4( 0.41487166,  0.81442589,  0.600000, -0.505648),
     vec4(-0.24106961, -0.32721516,  0.600000, -1.665244)
};

#define SSAO_MAX_TAPS (32)
#define SSAO_MAX_REF_TAPS (512)
#define SSAO_ADAPTIVE_TAP_BASE_COUNT (5)
#define SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT (SSAO_MAX_TAPS - SSAO_ADAPTIVE_TAP_BASE_COUNT)
#define SSAO_DEPTH_MIP_LEVELS (4)

// these values can be changed (up to SSAO_MAX_TAPS) with no changes required elsewhere; values for 4th and 5th preset are ignored but array needed to avoid compilation errors
// the actual number of texture samples is two times this value (each "tap" has two symmetrical depth texture samples)
static const uint g_numTaps[5] = { 3, 5, 12, 0, 0 };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Optional parts that can be enabled for a required quality preset level and above (0 == Low, 1 == Medium, 2 == High, 3 == Highest/Adaptive, 4 == reference/unused )
// Each has its own cost. To disable just set to 5 or above.
//
// (experimental) tilts the disk (although only half of the samples!) towards surface normal; this helps with effect uniformity between objects but reduces effect distance and has other side-effects
#define SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET                      (99)        // to disable simply set to 99 or similar
#define SSAO_TILT_SAMPLES_AMOUNT                                        (0.4)
//
#define SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET                 (1)         // to disable simply set to 99 or similar
#define SSAO_HALOING_REDUCTION_AMOUNT                                   (0.6)       // values from 0.0 - 1.0, 1.0 means max weighting (will cause artifacts, 0.8 is more reasonable)
//
#define SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                (2) //2        // to disable simply set to 99 or similar
#define SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD                           (0.5)       // use 0-0.1 for super-sharp normal-based edges
//
#define SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET                         (1) //1         // whether to use DetailAOStrength; to disable simply set to 99 or similar
//
#define SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET                        (2)         // !!warning!! the MIP generation on the C++ side will be enabled on quality preset 2 regardless of this value, so if changing here, change the C++ side too
#define SSAO_DEPTH_MIPS_GLOBAL_OFFSET                                   (-4.3)      // best noise/quality/performance tradeoff, found empirically
//
// !!warning!! the edge handling is hard-coded to 'disabled' on quality level 0, and enabled above, on the C++ side; while toggling it here will work for 
// testing purposes, it will not yield performance gains (or correct results)
#define SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                 (1)     
//
#define SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET  (99)        // 99 means disabled; only helpful if artifacts at the edges caused by lack of out of screen depth data are not acceptable with the depth sampler in either clamp or mirror modes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------
vec3 DecodeNormal(vec3 encodedNormal)
{
    vec3 normal = encodedNormal * g_ASSAOConsts.NormalsUnpackMul.xxx + g_ASSAOConsts.NormalsUnpackAdd.xxx;

#if SSAO_ENABLE_NORMAL_WORLD_TO_VIEW_CONVERSION
    normal = mul(vec4(normal, 1.0), g_ASSAOConsts.NormalsWorldToViewspaceMatrix).xyz;
#endif

    return normal;
}

//----------------------------------------------------
vec3 LoadNormal(const ivec2 pos)
{
    vec3 encodedNormal = load(tNormals, pos, 0).xyz;
    //return DecodeNormal(encodedNormal);
    return encodedNormal;
}

//----------------------------------------------------
vec3 LoadNormal(const uvec2 pos)
{
    return LoadNormal(ivec2(pos));
}

//----------------------------------------------------
vec3 LoadNormal(const ivec2 pos, const ivec2 offset)
{
    vec3 encodedNormal = load(tNormals, pos + offset, 0).xyz;
    //return DecodeNormal(encodedNormal);
    return encodedNormal;
}

//----------------------------------------------------
vec3 LoadNormal(const uvec2 pos, const ivec2 offset)
{
    return LoadNormal(ivec2(pos), offset);
}

//----------------------------------------------------
// all vectors in viewspace
float CalculatePixelObscurance(vec3 pixelNormal, vec3 hitDelta, float falloffCalcMulSq)
{
    float lengthSq = dot(hitDelta, hitDelta);
    float NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

    float falloffMult = max(0.0, lengthSq * falloffCalcMulSq + 1.0);

    return max(0, NdotD - g_ASSAOConsts.EffectHorizonAngleThreshold) * falloffMult;
}

//----------------------------------------------------
void SSAOTapInner(const int qualityLevel, inout float obscuranceSum, inout float weightSum, const vec2 samplingUV,
                  const float mipLevel, const vec3 pixCenterPos, const vec3 negViewspaceDir, vec3 pixelNormal,
                  const float falloffCalcMulSq, const float weightMod, const int dbgTapIndex)
{
    // get depth at sample
    float viewspaceSampleZ = sampleLOD(viewspaceDepthSource, vec3(samplingUV.xy, float(g_ASSAOConsts.PassIndex)), mipLevel).x;

    // convert to viewspace
    vec3 hitPos = NDCToViewspace(samplingUV.xy, viewspaceSampleZ).xyz;
    vec3 hitDelta = hitPos - pixCenterPos;

    float obscurance = CalculatePixelObscurance(pixelNormal, hitDelta, falloffCalcMulSq);
    float weight = 1.0;

    if(qualityLevel >= SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET)
    {
        //float reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
        float reduct = max(0, -hitDelta.z); // cheaper, less correct version
        reduct = saturate(reduct * g_ASSAOConsts.NegRecEffectRadius + 2.0);
        weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
    }
    weight *= weightMod;
    obscuranceSum += obscurance * weight;
    weightSum += weight;
}

//----------------------------------------------------
void SSAOTap(const int qualityLevel, inout float obscuranceSum, inout float weightSum, const int tapIndex,
             const mat2 rotScale, const vec3 pixCenterPos, const vec3 negViewspaceDir, vec3 pixelNormal,
             const vec2 normalizedScreenPos, const float mipOffset, const float falloffCalcMulSq,
             float weightMod, vec2 normXY, float normXYLength)
{
    vec2 sampleOffset;
    float samplePow2Len;

    // patterns
    {
        vec4 newSample = g_samplePatternMain[tapIndex];
        sampleOffset = mul(rotScale, newSample.xy);
        samplePow2Len = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
        weightMod *= newSample.z;
    }

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    sampleOffset = round(sampleOffset);

    // calculate MIP based on the sample distance from the centre, similar to as described 
    // in http://graphics.cs.williams.edu/papers/SAOHPG12/.
    float mipLevel = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

    vec2 samplingUV = sampleOffset * g_ASSAOConsts.Viewport2xPixelSize + normalizedScreenPos;

    SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingUV, mipLevel, pixCenterPos, negViewspaceDir,
        pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2);

    // for the second tap, just use the mirrored offset
    vec2 sampleOffsetMirroredUV = -sampleOffset;

    // tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    if(qualityLevel >= SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET)
    {
        float dotNorm = dot(sampleOffsetMirroredUV, normXY);
        sampleOffsetMirroredUV -= dotNorm * normXYLength * normXY;
        sampleOffsetMirroredUV = round(sampleOffsetMirroredUV);
    }

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    vec2 samplingMirroredUV = sampleOffsetMirroredUV * g_ASSAOConsts.Viewport2xPixelSize + normalizedScreenPos;

    SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingMirroredUV, mipLevel, pixCenterPos, negViewspaceDir,
        pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 + 1);
}

//----------------------------------------------------
void GenerateSSAOShadowsInternal(out float outShadowTerm, out vec4 outEdges, out float outWeight,
                                 const vec2 SVPos, int qualityLevel, bool adaptiveBase)
{
    vec2 SVPosRounded = trunc(SVPos);
    uvec2 SVPosui = uvec2(SVPosRounded); //same as uvec2( SVPos )

    const int numberOfTaps = (adaptiveBase) ? (SSAO_ADAPTIVE_TAP_BASE_COUNT) : int(g_numTaps[qualityLevel]);
    float pixZ, pixLZ, pixTZ, pixRZ, pixBZ;

    vec4 valuesUL = gatherRed(viewspaceDepthSource, vec3(SVPosRounded * g_ASSAOConsts.HalfViewportPixelSize, float(g_ASSAOConsts.PassIndex)));
    vec4 valuesBR = gatherRedOffset(viewspaceDepthSource, vec3(SVPosRounded * g_ASSAOConsts.HalfViewportPixelSize, float(g_ASSAOConsts.PassIndex)), ivec2(1, 1));

    // get this pixel's viewspace depth
    pixZ = valuesUL.y;

    // get left right top bottom neighbouring pixels for edge detection (gets compiled out on qualityLevel == 0)
    pixLZ = valuesUL.x;
    pixTZ = valuesUL.z;
    pixRZ = valuesBR.z;
    pixBZ = valuesBR.x;

    vec2 normalizedScreenPos = SVPosRounded * g_ASSAOConsts.Viewport2xPixelSize + g_ASSAOConsts.Viewport2xPixelSize_x_025;
    vec3 pixCenterPos = NDCToViewspace(normalizedScreenPos, pixZ);

    // Load this pixel's viewspace normal
    uvec2 fullResCoord = SVPosui * 2 + g_ASSAOConsts.PerPassFullResCoordOffset.xy;
    ivec3 normalCoord = ivec3(SVPosui, g_ASSAOConsts.PassIndex);
    vec3 pixelNormal = LoadNormal(fullResCoord);

    // optimized approximation of:
    // vec2 pixelDirRBViewspaceSizeAtCenterZ = NDCToViewspace( normalizedScreenPos.xy + g_ASSAOConsts._ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
    const vec2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * g_ASSAOConsts.NDCToViewMul * g_ASSAOConsts.Viewport2xPixelSize;

    float pixLookupRadiusMod;
    float falloffCalcMulSq;

    // calculate effect radius and fit our screen sampling pattern inside it
    float effectViewspaceRadius;
    CalculateRadiusParameters(length(pixCenterPos), pixelDirRBViewspaceSizeAtCenterZ, pixLookupRadiusMod, effectViewspaceRadius, falloffCalcMulSq);

    // calculate samples rotation/scaling
    mat2 rotScale;
    {
        // reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
        if(!adaptiveBase && (qualityLevel >= SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET))
        {
            float nearScreenBorder = min(min(normalizedScreenPos.x, 1.0 - normalizedScreenPos.x), min(normalizedScreenPos.y, 1.0 - normalizedScreenPos.y));
            nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
            pixLookupRadiusMod *= nearScreenBorder;
        }

        // load & update pseudo-random rotation matrix
        uint pseudoRandomIndex = uint(SVPosRounded.y * 2 + SVPosRounded.x) % 5;
        vec4 rs = g_ASSAOConsts.PatternRotScaleMatrices[pseudoRandomIndex];
        rotScale = mat2(rs.x * pixLookupRadiusMod, rs.y * pixLookupRadiusMod, rs.z * pixLookupRadiusMod, rs.w * pixLookupRadiusMod);
    }

    // the main obscurance & sample weight storage
    float obscuranceSum = 0.0;
    float weightSum = 0.0;

    // edge mask for between this and left/right/top/bottom neighbour pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
    vec4 edgesLRTB = vec4(1.0, 1.0, 1.0, 1.0);

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer; a lot smaller offsets needed when using 32bit floats
    pixCenterPos *= g_ASSAOConsts.DepthPrecisionOffsetMod;

    if(!adaptiveBase && (qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        edgesLRTB = CalculateEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    }

    // adds a more high definition sharp effect, which gets blurred out (reuses left/right/top/bottom samples that we used for edge detection)
    if(!adaptiveBase && (qualityLevel >= SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET))
    {
        // disable in case of quality level 4 (reference)
        if(qualityLevel != 4)
        {
            //approximate neighbouring pixels positions (actually just deltas or "positions - pixCenterPos" )
            vec3 viewspaceDirZNormalized = vec3(pixCenterPos.xy / pixCenterPos.zz, 1.0);

            // very close approximation of: vec3 pixLPos  = NDCToViewspace( normalizedScreenPos + vec2( -g_ASSAOConsts.HalfViewportPixelSize.x, 0.0 ), pixLZ ).xyz - pixCenterPos.xyz;
            vec3 pixLDelta = vec3(-pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixLZ - pixCenterPos.z);
            // very close approximation of: vec3 pixRPos  = NDCToViewspace( normalizedScreenPos + vec2( +g_ASSAOConsts.HalfViewportPixelSize.x, 0.0 ), pixRZ ).xyz - pixCenterPos.xyz;
            vec3 pixRDelta = vec3(+pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixRZ - pixCenterPos.z);
            // very close approximation of: vec3 pixTPos  = NDCToViewspace( normalizedScreenPos + vec2( 0.0, -g_ASSAOConsts.HalfViewportPixelSize.y ), pixTZ ).xyz - pixCenterPos.xyz;
            vec3 pixTDelta = vec3(0.0, -pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixTZ - pixCenterPos.z);
            // very close approximation of: vec3 pixBPos  = NDCToViewspace( normalizedScreenPos + vec2( 0.0, +g_ASSAOConsts.HalfViewportPixelSize.y ), pixBZ ).xyz - pixCenterPos.xyz;
            vec3 pixBDelta = vec3(0.0, +pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixBZ - pixCenterPos.z);

            const float rangeReductionConst = 4.0f;                         // this is to avoid various artifacts
            const float modifiedFalloffCalcMulSq = rangeReductionConst * falloffCalcMulSq;

            vec4 additionalObscurance;
            additionalObscurance.x = CalculatePixelObscurance(pixelNormal, pixLDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.y = CalculatePixelObscurance(pixelNormal, pixRDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.z = CalculatePixelObscurance(pixelNormal, pixTDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.w = CalculatePixelObscurance(pixelNormal, pixBDelta, modifiedFalloffCalcMulSq);

            obscuranceSum += g_ASSAOConsts.DetailAOStrength * dot(additionalObscurance, edgesLRTB);
        }
    }

    // Sharp normals also create edges - but this adds to the cost as well
    if(!adaptiveBase && (qualityLevel >= SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        vec3 neighbourNormalL = LoadNormal(fullResCoord, ivec2(-2,  0));
        vec3 neighbourNormalR = LoadNormal(fullResCoord, ivec2( 2,  0));
        vec3 neighbourNormalT = LoadNormal(fullResCoord, ivec2( 0, -2));
        vec3 neighbourNormalB = LoadNormal(fullResCoord, ivec2( 0,  2));
        
        const float dotThreshold = SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

        vec4 normalEdgesLRTB;
        normalEdgesLRTB.x = saturate((dot(pixelNormal, neighbourNormalL) + dotThreshold));
        normalEdgesLRTB.y = saturate((dot(pixelNormal, neighbourNormalR) + dotThreshold));
        normalEdgesLRTB.z = saturate((dot(pixelNormal, neighbourNormalT) + dotThreshold));
        normalEdgesLRTB.w = saturate((dot(pixelNormal, neighbourNormalB) + dotThreshold));

        //#define SSAO_SMOOTHEN_NORMALS // fixes some aliasing artifacts but kills a lot of high detail and adds to the cost - not worth it probably but feel free to play with it
#ifdef SSAO_SMOOTHEN_NORMALS
        //neighbourNormalL  = LoadNormal( fullResCoord, ivec2( -1,  0 ) );
        //neighbourNormalR  = LoadNormal( fullResCoord, ivec2(  1,  0 ) );
        //neighbourNormalT  = LoadNormal( fullResCoord, ivec2(  0, -1 ) );
        //neighbourNormalB  = LoadNormal( fullResCoord, ivec2(  0,  1 ) );
        pixelNormal += neighbourNormalL * edgesLRTB.x + neighbourNormalR * edgesLRTB.y + neighbourNormalT * edgesLRTB.z + neighbourNormalB * edgesLRTB.w;
        pixelNormal = normalize(pixelNormal);
#endif

        edgesLRTB *= normalEdgesLRTB;
    }

    const float globalMipOffset = SSAO_DEPTH_MIPS_GLOBAL_OFFSET;
    float mipOffset = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (log2(pixLookupRadiusMod) + globalMipOffset);

    // Used to tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    vec2 normXY = vec2(pixelNormal.x, pixelNormal.y);
    float normXYLength = length(normXY);
    normXY /= vec2(normXYLength, -normXYLength);
    normXYLength *= SSAO_TILT_SAMPLES_AMOUNT;

    const vec3 negViewspaceDir = -normalize(pixCenterPos);

    // standard, non-adaptive approach
    if((qualityLevel != 3) || adaptiveBase)
    {
        unroll
        for(int i = 0; i < numberOfTaps; i++)
        {
            SSAOTap(qualityLevel, obscuranceSum, weightSum, i, rotScale, pixCenterPos, negViewspaceDir, pixelNormal, normalizedScreenPos, mipOffset, falloffCalcMulSq, 1.0, normXY, normXYLength);
            // SSAOHits hits = SSAOGetHits(qualityLevel, normalizedScreenPos, i, mipOffset, rotScale);
            // SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, 1.0f, falloffCalcMulSq, weightSum, obscuranceSum, hits);
        }
    }
#if 0
    else // if( qualityLevel == 3 ) adaptive approach
    {
        // add new ones if needed
        vec2 fullResUV = normalizedScreenPos + g_ASSAOConsts.PerPassFullResUVOffset.xy;
        float importance = sampleLevelZero(importanceMap, fullResUV).x;

        // this is to normalize SSAO_DETAIL_AO_AMOUNT across all pixel regardless of importance
        obscuranceSum *= (SSAO_ADAPTIVE_TAP_BASE_COUNT / float(SSAO_MAX_TAPS)) + (importance * SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT / float(SSAO_MAX_TAPS));

        // load existing base values
        vec2 baseValues = loadArray(finalSSAO, SVPosui, g_ASSAOConsts.PassIndex, 0).xy;
        weightSum += baseValues.y * float(SSAO_ADAPTIVE_TAP_BASE_COUNT * 4.0);
        obscuranceSum += baseValues.x * weightSum;

        // increase importance around edges
        float edgeCount = dot(1.0 - edgesLRTB, vec4(1.0, 1.0, 1.0, 1.0));

#if defined(GLSL_VERSION)
        float avgTotalImportance = float(load(loadCounter, 0, 0)) * g_ASSAOConsts.LoadCounterAvgDiv;
#else
        float avgTotalImportance = float(load(loadCounter, ivec2(0, 0), 0)) * g_ASSAOConsts.LoadCounterAvgDiv;
#endif

        float importanceLimiter = saturate(g_ASSAOConsts.AdaptiveSampleCountLimit / avgTotalImportance);
        importance *= importanceLimiter;

        float additionalSampleCountFlt = SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

        additionalSampleCountFlt += 1.5;
        uint additionalSamples = uint(additionalSampleCountFlt);
        uint additionalSamplesTo = min(SSAO_MAX_TAPS, additionalSamples + SSAO_ADAPTIVE_TAP_BASE_COUNT);

        // sample loop
        {
            vec4 newSample = g_samplePatternMain[SSAO_ADAPTIVE_TAP_BASE_COUNT];
            SSAOSampleData data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
            SSAOHits hits = SSAOGetHits2(data, depthBufferUV);
            newSample = g_samplePatternMain[SSAO_ADAPTIVE_TAP_BASE_COUNT + 1];

            for(uint i = SSAO_ADAPTIVE_TAP_BASE_COUNT; i < additionalSamplesTo - 1; i++)
            {
                data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
                newSample = g_samplePatternMain[i + 2];
                SSAOHits nextHits = SSAOGetHits2(data, depthBufferUV);

                SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
                hits = nextHits;
            }

            // last loop iteration
            {
                SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
            }
        }
    }
#endif

    // early out for adaptive base - just output weight (used for the next pass)
    if(adaptiveBase)
    {
        float obscurance = obscuranceSum / weightSum;

        outShadowTerm = obscurance;
        outEdges = vec4(0.0, 0.0, 0.0, 0.0);
        outWeight = weightSum;
        return;
    }

    // calculate weighted average
    float obscurance = obscuranceSum / weightSum;

    // calculate fadeout (1 close, gradient, 0 far)
    float fadeOut = saturate(pixCenterPos.z * g_ASSAOConsts.EffectFadeOutMul + g_ASSAOConsts.EffectFadeOutAdd);

    // Reduce the SSAO shadowing if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
    if(!adaptiveBase && (qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        // when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
        float edgeFadeoutFactor = saturate((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35) + saturate((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35);

        // (experimental) if you want to reduce the effect next to any edge
        // edgeFadeoutFactor += 0.1 * saturate( dot( 1 - edgesLRTB, vec4( 1, 1, 1, 1 ) ) );

        fadeOut *= saturate(1.0 - edgeFadeoutFactor);
    }

    // strength
    obscurance = g_ASSAOConsts.EffectShadowStrength * obscurance;
    
    // [Kex]: Reduced resolution causes lighter shadows, so re-adjust strength of overall shadow accordingly
    obscurance *= ((uResolutionScale.z + uResolutionScale.w) * 0.5);

    // clamp
    obscurance = min(obscurance, g_ASSAOConsts.EffectShadowClamp);

    // fadeout
    obscurance *= fadeOut;

    // conceptually switch to occlusion with the meaning being visibility (grows with visibility, occlusion == 1 implies full visibility), 
    // to be in line with what is more commonly used.
    float occlusion = 1.0 - obscurance;

    // modify the gradient
    // note: this cannot be moved to a later pass because of loss of precision after storing in the render target
    occlusion = pow(saturate(occlusion), g_ASSAOConsts.EffectShadowPow);

    // outputs!
    outShadowTerm = occlusion;    // Our final 'occlusion' term (0 means fully occluded, 1 means fully lit)
    outEdges = edgesLRTB;    // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
    outWeight = weightSum;
}
