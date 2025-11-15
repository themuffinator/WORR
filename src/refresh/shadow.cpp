
#include "gl.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

struct shadow_quality_config_t {
	int		width;
	int		height;
	int		tiles_per_row;
	int		tiles_per_column;
};

constexpr std::array<shadow_quality_config_t, 3> kShadowQualityLevels = {{
	{ 4096, 4096, 16, 16 },
	{ 8192, 4096, 16, 8 },
	{ 8192, 8192, 16, 16 },
}};

constexpr size_t kMaxShadowViews = MAX_SHADOW_VIEWS;
constexpr float kDefaultShadowBias = 0.001f;
constexpr float kMinShadowRadius = 16.0f;
constexpr float kMinNearPlane = 0.25f;
constexpr float kNearPlaneFraction = 0.05f;
constexpr float kMaxNearPlaneFraction = 0.5f;

struct face_orientation_t {
	std::array<float, 3>	forward;
	std::array<float, 3>	up;
};

constexpr std::array<face_orientation_t, 6> kCubemapFaceOrientations = {{
	{ { 1.0f,  0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f } },
	{ { -1.0f, 0.0f,  0.0f }, { 0.0f, -1.0f,  0.0f } },
	{ { 0.0f,  1.0f,  0.0f }, { 0.0f,  0.0f,  1.0f } },
	{ { 0.0f, -1.0f,  0.0f }, { 0.0f,  0.0f, -1.0f } },
	{ { 0.0f,  0.0f,  1.0f }, { 0.0f, -1.0f,  0.0f } },
	{ { 0.0f,  0.0f, -1.0f }, { 0.0f, -1.0f,  0.0f } },
}};

struct shadow_render_view_t {
	shadow_view_assignment_t	assignment;
	mat4_t					view_matrix;
	mat4_t					proj_matrix;
	vec3_t					axis[3];
	vec3_t					origin;
	float					fov_x;
	float					fov_y;
	float					near_plane;
	float					far_plane;
};

std::vector<shadow_render_view_t> g_render_views;
std::vector<int> g_light_shadow_bases;
std::vector<int> g_light_shadow_counts;
int g_shadow_requested_quality = -1;

bool has_required_gl_capabilities()
{
	return qglGenTextures && qglDeleteTextures && qglBindTexture &&
		qglTexParameteri && qglTexImage2D && qglGenFramebuffers &&
		qglDeleteFramebuffers && qglBindFramebuffer &&
		qglFramebufferTexture2D && qglCheckFramebufferStatus;
}

void destroy_resources()
{
	auto &shadow = gl_static.shadow;

	if (shadow.framebuffer && qglDeleteFramebuffers) {
		qglDeleteFramebuffers(1, &shadow.framebuffer);
		shadow.framebuffer = 0;
	}

	if (shadow.texture && qglDeleteTextures) {
		qglDeleteTextures(1, &shadow.texture);
		shadow.texture = 0;
	}

	shadow.supported = false;
	shadow.width = 0;
	shadow.height = 0;
	shadow.tiles_per_row = 0;
	shadow.tiles_per_column = 0;
	shadow.tile_width = 0;
	shadow.tile_height = 0;
	shadow.view_count = 0;
	shadow.quality = -1;
	g_shadow_requested_quality = -1;
}

int choose_quality_level()
{
	int quality = 1;
	if (r_shadows)
		quality = r_shadows->integer;
	quality = std::clamp(quality, 1, static_cast<int>(kShadowQualityLevels.size()));
	return quality - 1;
}

void apply_quality_layout(int quality_index)
{
	auto &shadow = gl_static.shadow;
	const auto &config = kShadowQualityLevels[quality_index];

	shadow.width = config.width;
	shadow.height = config.height;
	shadow.tiles_per_row = config.tiles_per_row;
	shadow.tiles_per_column = config.tiles_per_column;
	shadow.tile_width = shadow.tiles_per_row ? shadow.width / shadow.tiles_per_row : 0;
	shadow.tile_height = shadow.tiles_per_column ? shadow.height / shadow.tiles_per_column : 0;
	shadow.quality = quality_index;
}

void reset_frame_state()
{
	auto &shadow = gl_static.shadow;
	shadow.view_count = 0;
	shadow.tile_width = shadow.tiles_per_row ? shadow.width / shadow.tiles_per_row : 0;
	shadow.tile_height = shadow.tiles_per_column ? shadow.height / shadow.tiles_per_column : 0;

	for (auto &assignment : shadow.assignments) {
		assignment.valid = false;
		assignment.atlas_index = -1;
		assignment.face = -1;
		assignment.resolution = 0;
		for (int i = 0; i < 4; ++i) {
			assignment.parameters.viewport_rect[i] = 0.0f;
			assignment.parameters.source_position[i] = 0.0f;
		}
	}
}

/*
=============
configure_texture_parameters

Configures the filtering and wrapping modes for the shadow atlas texture while
preserving the caller's texture binding state.
=============
*/
void configure_texture_parameters(GLuint texture)
{
	GLint prev_active_enum = GL_TEXTURE0;
	GLint prev_active_binding = 0;
	if (qglGetIntegerv) {
		qglGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_enum);
		qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_active_binding);
	} else {
		prev_active_enum = GL_TEXTURE0 + gls.server_tmu;
		prev_active_binding = static_cast<GLint>(gls.texnums[gls.server_tmu]);
	}

	glTmu_t prev_active_tmu = gls.server_tmu;
	if (prev_active_enum >= GL_TEXTURE0) {
		const GLint computed_tmu = prev_active_enum - GL_TEXTURE0;
		if (computed_tmu >= 0 && computed_tmu < MAX_TMUS)
			prev_active_tmu = static_cast<glTmu_t>(computed_tmu);
	}
	const GLuint restore_active_binding = prev_active_binding >= 0 ? static_cast<GLuint>(prev_active_binding) : 0;

	GL_ActiveTexture(TMU_TEXTURE);

	GLint prev_texture_binding = 0;
	if (qglGetIntegerv)
		qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture_binding);
	else
		prev_texture_binding = static_cast<GLint>(gls.texnums[TMU_TEXTURE]);
	const GLuint restore_texture_binding = prev_texture_binding >= 0 ? static_cast<GLuint>(prev_texture_binding) : 0;

	qglBindTexture(GL_TEXTURE_2D, texture);
	gls.texnums[TMU_TEXTURE] = texture;
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_COMPARE_MODE
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
	qglBindTexture(GL_TEXTURE_2D, restore_texture_binding);
	gls.texnums[TMU_TEXTURE] = restore_texture_binding;

	GL_ActiveTexture(prev_active_tmu);
	if (prev_active_tmu != TMU_TEXTURE) {
		qglBindTexture(GL_TEXTURE_2D, restore_active_binding);
		gls.texnums[prev_active_tmu] = restore_active_binding;
	}
}

float compute_light_bias(const shadow_light_submission_t &light)
{
	if (light.bias > 0.0f)
		return light.bias;
	return kDefaultShadowBias;
}

float compute_light_shade(const shadow_light_submission_t &light)
{
	return (std::max)(light.intensity, 0.0f);
}

float compute_shadow_far_plane(const shadow_light_submission_t &light)
{
	return (std::max)(light.radius, kMinShadowRadius);
}

float compute_shadow_near_plane(float far_plane)
{
	float near_plane = (std::max)(far_plane * kNearPlaneFraction, kMinNearPlane);
	if (near_plane >= far_plane)
	near_plane = far_plane * kMaxNearPlaneFraction;
	return (std::max)(near_plane, kMinNearPlane);
}

void build_axis_from_direction(const vec3_t forward_in, const vec3_t up_hint, vec3_t out_axis[3])
{
	vec3_t forward;
	VectorCopy(forward_in, forward);
	if (VectorNormalize(forward) == 0.0f) {
		forward[0] = 0.0f;
		forward[1] = 0.0f;
		forward[2] = 1.0f;
	}

	vec3_t up;
	VectorCopy(up_hint, up);
	if (VectorNormalize(up) == 0.0f) {
		up[0] = 0.0f;
		up[1] = 0.0f;
		up[2] = 1.0f;
	}

	if (std::fabs(DotProduct(forward, up)) > 0.99f) {
		vec3_t fallback = { 0.0f, 0.0f, 1.0f };
		if (std::fabs(DotProduct(forward, fallback)) > 0.99f) {
			fallback[0] = 1.0f;
			fallback[1] = 0.0f;
			fallback[2] = 0.0f;
		}
		VectorCopy(fallback, up);
		VectorNormalize(up);
	}

	vec3_t left;
	CrossProduct(up, forward, left);
	if (VectorNormalize(left) == 0.0f) {
		left[0] = 1.0f;
		left[1] = 0.0f;
		left[2] = 0.0f;
	}

	vec3_t corrected_up;
	CrossProduct(forward, left, corrected_up);
	VectorNormalize(corrected_up);

	VectorCopy(forward, out_axis[0]);
	VectorCopy(left, out_axis[1]);
	VectorCopy(corrected_up, out_axis[2]);
}

void build_axis_from_orientation(const face_orientation_t &orientation, vec3_t out_axis[3])
{
	vec3_t forward;
	vec3_t up;
	forward[0] = orientation.forward[0];
	forward[1] = orientation.forward[1];
	forward[2] = orientation.forward[2];
	up[0] = orientation.up[0];
	up[1] = orientation.up[1];
	up[2] = orientation.up[2];
	build_axis_from_direction(forward, up, out_axis);
}

void store_shadow_item(const shadow_view_assignment_t &assignment)
{
	if (assignment.atlas_index < 0)
		return;
	const size_t index = static_cast<size_t>(assignment.atlas_index);
	if (index >= gls.shadow_items.size())
		return;

	glShadowItem_t &item = gls.shadow_items[index];
	std::memcpy(item.volume_matrix, assignment.parameters.view_projection, sizeof(mat4_t));
	for (int i = 0; i < 4; ++i) {
		item.viewport_rect[i] = assignment.parameters.viewport_rect[i];
		item.source_position[i] = assignment.parameters.source_position[i];
	}
	item.bias = assignment.parameters.bias;
	item.shade_amount = assignment.parameters.shade_amount;
}

int compute_view_resolution(const shadow_light_submission_t &light)
{
	const int base_resolution = (std::min)(gl_static.shadow.tile_width, gl_static.shadow.tile_height);
	if (base_resolution <= 0)
	return 0;
	int resolution = light.resolution > 0 ? light.resolution : base_resolution;
	resolution = std::clamp(resolution, 1, base_resolution);
	return resolution;
}

/*
=============
append_shadow_view

Builds a shadow map view for the given light and face index.
=============
*/
bool append_shadow_view(const shadow_light_submission_t &light, size_t light_index, const vec3_t axis[3], float fov,
	float near_plane, float far_plane, int face)
{
	mat4_t view_matrix;
	Matrix_FromOriginAxis(light.origin, axis, view_matrix);

	mat4_t proj_matrix;
	Matrix_Frustum(fov, fov, 1.0f, near_plane, far_plane, proj_matrix);

	mat4_t view_projection;
	Matrix_Multiply(proj_matrix, view_matrix, view_projection);

	shadow_view_parameters_t params{};
	std::memcpy(params.view_projection, view_projection, sizeof(mat4_t));
	for (int i = 0; i < 4; ++i) {
		params.viewport_rect[i] = 0.0f;
		params.source_position[i] = 0.0f;
	}
	params.source_position[0] = light.origin[0];
	params.source_position[1] = light.origin[1];
	params.source_position[2] = light.origin[2];
	params.source_position[3] = far_plane;
	params.bias = compute_light_bias(light);
	params.shade_amount = compute_light_shade(light);

	const int resolution = compute_view_resolution(light);
	shadow_view_assignment_t assignment{};
	if (!R_ShadowAtlasAllocateView(params, face, resolution, &assignment))
		return false;

	shadow_render_view_t view{};
	view.assignment = assignment;
	std::memcpy(view.view_matrix, view_matrix, sizeof(mat4_t));
	std::memcpy(view.proj_matrix, proj_matrix, sizeof(mat4_t));
	for (int i = 0; i < 3; ++i)
		VectorCopy(axis[i], view.axis[i]);
	VectorCopy(light.origin, view.origin);
	view.fov_x = fov;
	view.fov_y = fov;
	view.near_plane = near_plane;
	view.far_plane = far_plane;

	g_render_views.push_back(view);
	store_shadow_item(assignment);
	if (light_index < g_light_shadow_bases.size() && assignment.atlas_index >= 0) {
		if (g_light_shadow_bases[light_index] < 0)
			g_light_shadow_bases[light_index] = assignment.atlas_index;
		g_light_shadow_counts[light_index]++;
	}
	return true;
}

/*
=============
render_shadow_views

Render each queued shadow view into the shadow atlas, preserving prior GL state.
Renders the shadow map views into the atlas while preserving the main view
state so post-processing receives consistent matrices.
=============
*/
void render_shadow_views()
{
	if (g_render_views.empty())
		return;

	const bool rendering_shadows = true;
	const bool prev_rendering_shadows = glr.rendering_shadows;
	glr.rendering_shadows = rendering_shadows;

	GLint prev_draw_buffer = GL_BACK;
	GLint prev_read_buffer = GL_BACK;
	GLint prev_fbo = 0;
	GLint prev_viewport[4] = { 0, 0, 0, 0 };
	GLint prev_color_mask_i[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLint prev_depth_mask_i = GL_TRUE;
	GLboolean prev_color_mask[4];
	GLboolean prev_depth_mask = GL_TRUE;

	qglGetIntegerv(GL_DRAW_BUFFER, &prev_draw_buffer);
	const bool has_read_buffer = qglReadBuffer != nullptr;
	if (has_read_buffer)
		qglGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);
	qglGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
	qglGetIntegerv(GL_VIEWPORT, prev_viewport);
	qglGetIntegerv(GL_COLOR_WRITEMASK, prev_color_mask_i);
	qglGetIntegerv(GL_DEPTH_WRITEMASK, &prev_depth_mask_i);
	for (int i = 0; i < 4; ++i)
	prev_color_mask[i] = prev_color_mask_i[i] ? GL_TRUE : GL_FALSE;
	prev_depth_mask = prev_depth_mask_i ? GL_TRUE : GL_FALSE;

	const bool prev_framebuffer_bound = glr.framebuffer_bound;

	qglBindFramebuffer(GL_FRAMEBUFFER, gl_static.shadow.framebuffer);
	const GLenum no_buffers = GL_NONE;
	if (qglDrawBuffers) {
		qglDrawBuffers(1, &no_buffers);
	} else if (qglDrawBuffer) {
		qglDrawBuffer(GL_NONE);
	}
	if (has_read_buffer)
		qglReadBuffer(GL_NONE);
	qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	qglDepthMask(GL_TRUE);

	refdef_t saved_fd = glr.fd;
	mat4_t saved_viewmatrix;
	mat4_t saved_projmatrix;
	mat4_t saved_view_proj_matrix;
	mat4_t saved_inv_view_proj_matrix;
	vec3_t saved_viewaxis[3];
	bool saved_view_proj_valid = glr.view_proj_valid;
	float saved_view_znear = glr.view_znear;
	float saved_view_zfar = glr.view_zfar;
	std::memcpy(saved_viewmatrix, glr.viewmatrix, sizeof(mat4_t));
	std::memcpy(saved_projmatrix, glr.projmatrix, sizeof(mat4_t));
	std::memcpy(saved_view_proj_matrix, glr.view_proj_matrix, sizeof(mat4_t));
	std::memcpy(saved_inv_view_proj_matrix, glr.inv_view_proj_matrix, sizeof(mat4_t));
	for (int i = 0; i < 3; ++i)
		VectorCopy(glr.viewaxis[i], saved_viewaxis[i]);

	glr.framebuffer_bound = true;

	for (const auto &view : g_render_views) {
		const float *rect = view.assignment.parameters.viewport_rect;
		const GLint viewport_x = static_cast<GLint>(rect[0]);
		const GLint viewport_y = static_cast<GLint>(rect[1]);
		const GLsizei viewport_w = static_cast<GLsizei>(rect[2] - rect[0]);
		const GLsizei viewport_h = static_cast<GLsizei>(rect[3] - rect[1]);
		if (viewport_w <= 0 || viewport_h <= 0)
			continue;

		qglViewport(viewport_x, viewport_y, viewport_w, viewport_h);
		qglClear(GL_DEPTH_BUFFER_BIT);

		glr.fd = saved_fd;
		VectorCopy(view.origin, glr.fd.vieworg);
		glr.fd.width = viewport_w;
		glr.fd.height = viewport_h;
		glr.fd.fov_x = view.fov_x;
		glr.fd.fov_y = view.fov_y;

		for (int axis_index = 0; axis_index < 3; ++axis_index)
			VectorCopy(view.axis[axis_index], glr.viewaxis[axis_index]);

		std::memcpy(glr.viewmatrix, view.view_matrix, sizeof(mat4_t));
		std::memcpy(glr.projmatrix, view.proj_matrix, sizeof(mat4_t));
		Matrix_Multiply(glr.projmatrix, glr.viewmatrix, glr.view_proj_matrix);
		glr.view_znear = view.near_plane;
		glr.view_zfar = view.far_plane;
		glr.view_proj_valid = Matrix_Invert(glr.view_proj_matrix, glr.inv_view_proj_matrix);

		if (gl_backend->setup_3d)
			gl_backend->setup_3d();

		gl_backend->load_matrix(GL_PROJECTION, view.proj_matrix, gl_identity);
		GL_ForceMatrix(gl_identity, view.view_matrix);

		GL_ClearSolidFaces();
		if (!(glr.fd.rdflags & RDF_NOWORLDMODEL) && gl_drawworld->integer)
			GL_DrawWorld();

		GL_ClassifyEntities();
		GL_DrawEntities(glr.ents.bmodels);
		GL_DrawEntities(glr.ents.opaque);
		// Shadow atlas rendering is depth-only; skip the alpha entity lists so we
		// don't reintroduce blended passes that produce incorrect shadows.
		if (!rendering_shadows)
			GL_DrawAlphaFaces();
		GL_DrawDebugObjects();

		GL_Flush3D();
	}

	glr.fd = saved_fd;
	for (int i = 0; i < 3; ++i)
		VectorCopy(saved_viewaxis[i], glr.viewaxis[i]);
	std::memcpy(glr.viewmatrix, saved_viewmatrix, sizeof(mat4_t));
	std::memcpy(glr.projmatrix, saved_projmatrix, sizeof(mat4_t));
	glr.view_znear = saved_view_znear;
	glr.view_zfar = saved_view_zfar;
	std::memcpy(glr.view_proj_matrix, saved_view_proj_matrix, sizeof(mat4_t));
	std::memcpy(glr.inv_view_proj_matrix, saved_inv_view_proj_matrix, sizeof(mat4_t));
	glr.view_proj_valid = saved_view_proj_valid;
	glr.framebuffer_bound = prev_framebuffer_bound;
	gl_backend->load_matrix(GL_PROJECTION, glr.projmatrix, gl_identity);
	GL_ForceMatrix(gl_identity, glr.viewmatrix);

	qglBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	GLenum prev_draw_buffer_enum = static_cast<GLenum>(prev_draw_buffer);
	if (qglDrawBuffers) {
		qglDrawBuffers(1, &prev_draw_buffer_enum);
	} else if (qglDrawBuffer) {
		qglDrawBuffer(prev_draw_buffer_enum);
	}
	if (has_read_buffer)
		qglReadBuffer(prev_read_buffer);
	qglViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
	qglColorMask(prev_color_mask[0], prev_color_mask[1], prev_color_mask[2], prev_color_mask[3]);
	qglDepthMask(prev_depth_mask);
	glr.rendering_shadows = prev_rendering_shadows;
}

} // namespace

/*
=============
R_ShadowAtlasInit

Initializes the shadow atlas resources and selects the best supported
configuration.
=============
*/
bool R_ShadowAtlasInit(void)
{
	if (!has_required_gl_capabilities()) {
		destroy_resources();
		return false;
	}

	auto &shadow = gl_static.shadow;
	GLuint old_texture = shadow.texture;
	GLuint old_framebuffer = shadow.framebuffer;
	shadow.supported = false;

	const int requested_quality = choose_quality_level();
	const int max_texture_size = gl_config.max_texture_size;
	GLuint new_texture = 0;
	GLuint new_framebuffer = 0;
	int selected_quality = -1;

	for (int quality = requested_quality; quality >= 0; --quality) {
		const auto &config = kShadowQualityLevels[quality];
		if (config.width > max_texture_size || config.height > max_texture_size)
		continue;

		GLuint texture = 0;
		qglGenTextures(1, &texture);
		if (!texture)
		continue;

		if (qglGetError) {
			while (qglGetError() != GL_NO_ERROR) {
			}
		}

		GLint prev_active_enum = GL_TEXTURE0;
		GLint prev_active_binding = 0;
		if (qglGetIntegerv) {
			qglGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_enum);
			qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_active_binding);
		} else {
			prev_active_enum = GL_TEXTURE0 + gls.server_tmu;
			prev_active_binding = static_cast<GLint>(gls.texnums[gls.server_tmu]);
		}

		glTmu_t prev_active_tmu = gls.server_tmu;
		if (prev_active_enum >= GL_TEXTURE0) {
			const GLint computed_tmu = prev_active_enum - GL_TEXTURE0;
			if (computed_tmu >= 0 && computed_tmu < MAX_TMUS)
				prev_active_tmu = static_cast<glTmu_t>(computed_tmu);
		}
		const GLuint restore_active_binding = prev_active_binding >= 0 ? static_cast<GLuint>(prev_active_binding) : 0;

		GL_ActiveTexture(TMU_TEXTURE);

		GLint prev_texture_binding = 0;
		if (qglGetIntegerv)
			qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture_binding);
		else
			prev_texture_binding = static_cast<GLint>(gls.texnums[TMU_TEXTURE]);
		const GLuint restore_texture_binding = prev_texture_binding >= 0 ? static_cast<GLuint>(prev_texture_binding) : 0;

		qglBindTexture(GL_TEXTURE_2D, texture);
		gls.texnums[TMU_TEXTURE] = texture;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
		config.width, config.height, 0, GL_DEPTH_COMPONENT,
		GL_UNSIGNED_INT, nullptr);

		GLenum tex_error = GL_NO_ERROR;
		if (qglGetError)
			tex_error = qglGetError();

		qglBindTexture(GL_TEXTURE_2D, restore_texture_binding);
		gls.texnums[TMU_TEXTURE] = restore_texture_binding;

		GL_ActiveTexture(prev_active_tmu);
		if (prev_active_tmu != TMU_TEXTURE) {
			qglBindTexture(GL_TEXTURE_2D, restore_active_binding);
			gls.texnums[prev_active_tmu] = restore_active_binding;
		}

		if (tex_error != GL_NO_ERROR) {
			if (qglDeleteTextures)
			qglDeleteTextures(1, &texture);
			continue;
		}

		configure_texture_parameters(texture);

		GLuint framebuffer = 0;
		qglGenFramebuffers(1, &framebuffer);
		if (!framebuffer) {
			if (qglDeleteTextures)
			qglDeleteTextures(1, &texture);
			continue;
		}

		qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, texture, 0);

		const GLenum status = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
		qglBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			if (qglDeleteFramebuffers)
			qglDeleteFramebuffers(1, &framebuffer);
			if (qglDeleteTextures)
			qglDeleteTextures(1, &texture);
			continue;
		}

		new_texture = texture;
		new_framebuffer = framebuffer;
		selected_quality = quality;
		break;
	}

	if (selected_quality < 0) {
		destroy_resources();
		return false;
	}

	shadow.texture = new_texture;
	shadow.framebuffer = new_framebuffer;
	if (old_framebuffer && old_framebuffer != new_framebuffer && qglDeleteFramebuffers)
	qglDeleteFramebuffers(1, &old_framebuffer);
	if (old_texture && old_texture != new_texture && qglDeleteTextures)
	qglDeleteTextures(1, &old_texture);

	apply_quality_layout(selected_quality);
	shadow.supported = true;
	reset_frame_state();
	g_shadow_requested_quality = requested_quality;

	if (selected_quality < requested_quality) {
		const auto &config = kShadowQualityLevels[selected_quality];
		Com_Printf("Shadow atlas quality downgraded from %d to %d (%dx%d)\n",
		requested_quality + 1, selected_quality + 1,
		config.width, config.height);
	}

	return true;
}

void R_ShadowAtlasShutdown(void)
{
	destroy_resources();
}

void R_ShadowAtlasBeginFrame(void)
{
	if (!has_required_gl_capabilities())
		return;

	const int desired_quality = choose_quality_level();
	if (g_shadow_requested_quality != desired_quality) {
		if (!R_ShadowAtlasInit())
			return;
	}

	if (!gl_static.shadow.supported)
		return;

	reset_frame_state();
}

bool R_ShadowAtlasAllocateView(const shadow_view_parameters_t &params,
	int face, int resolution,
	shadow_view_assignment_t *out_assignment)
{
	auto &shadow = gl_static.shadow;

	if (!shadow.supported)
		return false;

	const size_t max_tiles = static_cast<size_t>(shadow.tiles_per_row) * static_cast<size_t>(shadow.tiles_per_column);
	if (shadow.view_count >= kMaxShadowViews || shadow.view_count >= max_tiles)
		return false;

	const size_t index = shadow.view_count;
	const int tile_width = shadow.tile_width;
	const int tile_height = shadow.tile_height;
	if (tile_width <= 0 || tile_height <= 0)
		return false;

	const int column = static_cast<int>(index % shadow.tiles_per_row);
	const int row = static_cast<int>(index / shadow.tiles_per_row);
	const int max_resolution = (std::min)(tile_width, tile_height);
	const int sanitized_resolution = resolution > 0 ? (std::min)(resolution, max_resolution) : max_resolution;
	const float viewport_width = static_cast<float>(sanitized_resolution);
	const float viewport_height = static_cast<float>(sanitized_resolution);
	const float tile_origin_x = static_cast<float>(column * tile_width);
	const float tile_origin_y = static_cast<float>(row * tile_height);
	const float x0 = tile_origin_x + (static_cast<float>(tile_width) - viewport_width) * 0.5f;
	const float y0 = tile_origin_y + (static_cast<float>(tile_height) - viewport_height) * 0.5f;
	const float x1 = x0 + viewport_width;
	const float y1 = y0 + viewport_height;

	shadow_view_assignment_t assignment{};
	assignment.parameters = params;
	assignment.parameters.viewport_rect[0] = x0;
	assignment.parameters.viewport_rect[1] = y0;
	assignment.parameters.viewport_rect[2] = x1;
	assignment.parameters.viewport_rect[3] = y1;
	assignment.cube_face_offset[0] = x0;
	assignment.cube_face_offset[1] = y0;
	assignment.cube_face_offset[2] = viewport_width;
	assignment.cube_face_offset[3] = viewport_height;
	assignment.atlas_index = static_cast<int>(index);
	assignment.face = face;
	assignment.resolution = sanitized_resolution;
	assignment.valid = true;

	shadow.assignments[index] = assignment;
	shadow.view_count = index + 1;

	if (out_assignment)
		*out_assignment = assignment;

	return true;
}

size_t R_ShadowAtlasViewCount(void)
{
	return gl_static.shadow.view_count;
}

const shadow_view_assignment_t *R_ShadowAtlasViews(void)
{
	return gl_static.shadow.assignments.data();
}

/*
=============
R_RenderShadowViews

Renders the queued shadow map views and records the atlas allocation
information for each light.
=============
*/
void R_RenderShadowViews(void)
{
	if (!gl_static.use_shaders)
		return;

	if (!gl_static.shadow.supported)
		return;

	std::memset(gls.shadow_items.data(), 0, gls.shadow_items.size() * sizeof(glShadowItem_t));
	g_render_views.clear();
	g_render_views.reserve((std::min)(kMaxShadowViews, gls.shadow_items.size()));

	auto &shadow = gl_static.shadow;

	size_t light_count = 0;
	const shadow_light_submission_t *lights = R_GetQueuedShadowLights(&light_count);
	g_light_shadow_bases.assign(light_count, -1);
	g_light_shadow_counts.assign(light_count, 0);
	if (!lights || !light_count)
		return;

	for (size_t i = 0; i < light_count; ++i) {
		const shadow_light_submission_t &light = lights[i];
		const int light_start_atlas_index = static_cast<int>(shadow.view_count);
		const size_t light_start_view_index = g_render_views.size();

		g_light_shadow_bases[i] = -1;
		g_light_shadow_counts[i] = 0;

		if (!light.casts_shadow)
			continue;

		int expected_faces = 0;
		const float far_plane = compute_shadow_far_plane(light);
		const float near_plane = compute_shadow_near_plane(far_plane);

		if (light.lighttype == shadow_light_type_point) {
			expected_faces = static_cast<int>(kCubemapFaceOrientations.size());
			for (size_t face_index = 0; face_index < kCubemapFaceOrientations.size(); ++face_index) {
				vec3_t axis[3];
				build_axis_from_orientation(kCubemapFaceOrientations[face_index], axis);
				if (!append_shadow_view(light, i, axis, 90.0f, near_plane, far_plane, static_cast<int>(face_index)))
					break;
			}
		} else if (light.lighttype == shadow_light_type_cone && light.coneangle > 0.0f) {
			expected_faces = 1;
			vec3_t up = { 0.0f, 0.0f, 1.0f };
			vec3_t axis[3];
			build_axis_from_direction(light.direction, up, axis);
			const float fov = std::clamp(light.coneangle, 5.0f, 170.0f);
			append_shadow_view(light, i, axis, fov, near_plane, far_plane, -1);
		}

		if (expected_faces <= 0)
			continue;

		if (g_light_shadow_counts[i] < expected_faces) {
			const int appended_count = static_cast<int>(shadow.view_count) - light_start_atlas_index;
			if (appended_count > 0) {
				for (int atlas_index = light_start_atlas_index; atlas_index < light_start_atlas_index + appended_count; ++atlas_index) {
					if (atlas_index >= 0 && static_cast<size_t>(atlas_index) < shadow.assignments.size()) {
						shadow_view_assignment_t &assignment = shadow.assignments[static_cast<size_t>(atlas_index)];
						assignment.valid = false;
						assignment.atlas_index = -1;
						assignment.face = -1;
						assignment.resolution = 0;
						for (int param_index = 0; param_index < 4; ++param_index) {
							assignment.parameters.viewport_rect[param_index] = 0.0f;
							assignment.parameters.source_position[param_index] = 0.0f;
						}
						for (int offset_index = 0; offset_index < 4; ++offset_index)
							assignment.cube_face_offset[offset_index] = 0.0f;
					}
				}
				shadow.view_count = light_start_atlas_index;
			}
			if (light_start_view_index < g_render_views.size())
				g_render_views.resize(light_start_view_index);
			g_light_shadow_bases[i] = -1;
			g_light_shadow_counts[i] = 0;
		}
	}
	const int atlas_view_count = static_cast<int>(gl_static.shadow.view_count);
	if (glr.fd.dlights && glr.fd.num_dlights > 0) {
		for (int dlight_index = 0; dlight_index < glr.fd.num_dlights; ++dlight_index) {
			dlight_t &dl = glr.fd.dlights[dlight_index];
			if (dl.shadow_submission_index < 0) {
				dl.shadow_view_base = -1;
				dl.shadow_view_count = 0;
				continue;
			}
			size_t submission_index = static_cast<size_t>(dl.shadow_submission_index);
			int base = -1;
			int count = 0;
			if (submission_index < g_light_shadow_bases.size()) {
				base = g_light_shadow_bases[submission_index];
				count = g_light_shadow_counts[submission_index];
			}
			if (count <= 0 || base < 0 || base >= atlas_view_count || base + count > atlas_view_count) {
				dl.shadow_view_base = -1;
				dl.shadow_view_count = 0;
				continue;
			}
			dl.shadow_view_base = base;
			dl.shadow_view_count = count;
		}
	}

	render_shadow_views();
}
