#pragma once

#include <cstdint>

void CRT_RegisterCvars();
bool CRT_IsEnabled();
int32_t CRT_ModifiedCount();
void CRT_UpdateUniforms(int viewportWidth, int viewportHeight);
