#pragma once

#include "gl.hpp"

inline void GL_DrawPic(
	vec2_t vertices[4], vec2_t texcoords[4],
	color_t color, int texnum, int flags)
{
	glVertexDesc2D_t *dst_vert;
	glIndex_t *dst_indices;

	if (tess.numverts + 4 > TESS_MAX_VERTICES ||
		tess.numindices + 6 > TESS_MAX_INDICES ||
		(tess.numverts && tess.texnum[TMU_TEXTURE] != texnum))
		GL_Flush2D();

	tess.texnum[TMU_TEXTURE] = texnum;

	dst_vert = ((glVertexDesc2D_t *) tess.vertices) + tess.numverts;

	for (int i = 0; i < 4; i++, dst_vert++) {
		Vector2Copy(vertices[i], dst_vert->xy);
		Vector2Copy(texcoords[i], dst_vert->st);
		dst_vert->c = color.u32;
	}

	dst_indices = tess.indices + tess.numindices;
	dst_indices[0] = tess.numverts + 0;
	dst_indices[1] = tess.numverts + 2;
	dst_indices[2] = tess.numverts + 3;
	dst_indices[3] = tess.numverts + 0;
	dst_indices[4] = tess.numverts + 1;
	dst_indices[5] = tess.numverts + 2;

	if (flags & IF_TRANSPARENT) {
		if ((flags & IF_PALETTED) && draw.scale == 1)
			tess.flags |= GLS_ALPHATEST_ENABLE;
		else
			tess.flags |= GLS_BLEND_BLEND;
	}

	if (color.a != 255)
		tess.flags |= GLS_BLEND_BLEND;

	tess.numverts += 4;
	tess.numindices += 6;
}

inline void GL_StretchPic_(
	float x, float y, float w, float h,
	float s1, float t1, float s2, float t2,
	color_t color, int texnum, int flags)
{
	std::array<vec2_t, 4> vertices{};
	std::array<vec2_t, 4> texcoords{};

	Vector2Set(vertices[0], x, y);
	Vector2Set(vertices[1], x + w, y);
	Vector2Set(vertices[2], x + w, y + h);
	Vector2Set(vertices[3], x, y + h);

	Vector2Set(texcoords[0], s1, t1);
	Vector2Set(texcoords[1], s2, t1);
	Vector2Set(texcoords[2], s2, t2);
	Vector2Set(texcoords[3], s1, t2);

	GL_DrawPic(vertices.data(), texcoords.data(), color, texnum, flags);
}
