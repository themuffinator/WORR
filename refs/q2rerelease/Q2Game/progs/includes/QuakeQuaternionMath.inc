//
// Copyright(C) 2019-2020 Nightdive Studios, LLC
//

//----------------------------------------------------
vec4 QMul(vec4 v1, vec4 v2)
{
    const float t0 = (v1.z - v1.y) * (v2.y - v2.z);
    const float t1 = (v1.w + v1.x) * (v2.w + v2.x);
    const float t2 = (v1.w - v1.x) * (v2.y + v2.z);
    const float t3 = (v1.y + v1.z) * (v2.w - v2.x);
    const float t4 = (v1.z - v1.x) * (v2.x - v2.y);
    const float t5 = (v1.z + v1.x) * (v2.x + v2.y);
    const float t6 = (v1.w + v1.y) * (v2.w - v2.z);
    const float t7 = (v1.w - v1.y) * (v2.w + v2.z);
    const float t8 = t5 + t6 + t7;
    const float t9 = 0.5f * (t4 + t8);

    return vec4(
        t1 + t9 - t8,
        t2 + t9 - t7,
        t3 + t9 - t6,
        t0 + t9 - t5);
}

//----------------------------------------------------
mat4 QToMatrix(vec4 quat)
{
    const float xx = quat.x * quat.x;
    const float yx = quat.y * quat.x;
    const float zx = quat.z * quat.x;
    const float wx = quat.w * quat.x;
    const float yy = quat.y * quat.y;
    const float zy = quat.z * quat.y;
    const float wy = quat.w * quat.y;
    const float zz = quat.z * quat.z;
    const float wz = quat.w * quat.z;
    const float ww = quat.w * quat.w;
    
    const float xy2 = 2*yx;
    const float wz2 = 2*wz;
    const float xz2 = 2*zx;
    const float wy2 = 2*wy;
    const float yz2 = 2*zy;
    const float wx2 = 2*wx;
    
    mat4 mtxOut;

    M(mtxOut, 0, 0) = ww + xx - yy - zz;
    M(mtxOut, 0, 1) = xy2 + wz2;
    M(mtxOut, 0, 2) = xz2 - wy2;
    M(mtxOut, 1, 0) = xy2 - wz2;
    M(mtxOut, 1, 1) = yy + ww - xx - zz;
    M(mtxOut, 1, 2) = yz2 + wx2;
    M(mtxOut, 2, 0) = xz2 + wy2;
    M(mtxOut, 2, 1) = yz2 - wx2;
    M(mtxOut, 2, 2) = ww - xx - yy + zz;
    M(mtxOut, 3, 0) = 0;
    M(mtxOut, 3, 1) = 0;
    M(mtxOut, 3, 2) = 0;
    M(mtxOut, 3, 3) = 1.0;
    
    return mtxOut;
}

//----------------------------------------------------
vec3 QRotateVec3(vec3 v, vec4 r)
{
    return mul(cast(mat3, QToMatrix(r)), v);
}

//----------------------------------------------------
float ATanPositive(float y, float x)
{
    float a, d, s, t;

    if (y > x)
    {
        a = -x / y;
        d = 1.5707963267948966192313216916398;
    }
    else
    {
        a = y / x;
        d = 0.0;
    }
    s = a * a;
    t = 0.0028662257;
    t *= s;
    t += -0.0161657367;
    t *= s;
    t += 0.0429096138;
    t *= s;
    t += -0.0752896400;
    t *= s;
    t += 0.1065626393;
    t *= s;
    t += -0.1420889944;
    t *= s;
    t += 0.1999355085;
    t *= s;
    t += -0.3333314528;
    t *= s;
    t += 1.0;
    t *= a;
    t += d;

    return t;
}

//----------------------------------------------------
float SinZeroHalfPI(float a)
{
    float s, t;

    s = a * a;
    t = -2.39e-08;
    t *= s;
    t += 2.7526e-06;
    t *= s;
    t += -1.98409e-04;
    t *= s;
    t += 8.3333315e-03;
    t *= s;
    t += -1.666666664e-01;
    t *= s;
    t += 1.0;
    t *= a;

    return t;
}

//----------------------------------------------------
vec4 QSlerp(in vec4 src, in vec4 quat, float movement)
{
    vec4 rdest = quat;
    vec4 qinv  = -quat;
    float d1 = dot(src, quat);
    float d2 = dot(src, qinv);

    if(d1 < d2)
    {
        rdest = qinv;
        d1 = d2;
    }

    if(d1 <= 0.7071067811865001)
    {
        float sinsqr = 1.0 - d1 * d1;
        float d = inversesqrt(sinsqr);
        float scale = ATanPositive(sinsqr * d, d1);

        float ms1 = SinZeroHalfPI((1.0 - movement) * scale) * d;
        float ms2 = SinZeroHalfPI(movement * scale) * d;

        if(ms2 < 0)
        {
            rdest = qinv;
        }

        return src * ms1 + rdest * ms2;
    }
    
    return normalize(src + ((rdest - src) * movement));
}
