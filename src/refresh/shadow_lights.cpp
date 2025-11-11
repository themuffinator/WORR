#include "refresh/refresh.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "common/math.hpp"

namespace {

constexpr float kMaxShadowLightRadius = 8192.0f;
constexpr float kQuarterPi = 0.78539816339744830962f;

std::vector<shadow_light_submission_t> g_shadowLights;
std::unordered_map<int, size_t> g_entityLookup;

static void cone_to_bounding_sphere(const vec3_t origin, const vec3_t forward, float size,
	float angle_radians, float c, float s, vec4_t out)
{
	if (angle_radians > kQuarterPi) {
	    VectorMA(origin, c * size, forward, out);
	    out[3] = s * size;
	} else {
	    VectorMA(origin, size / (2.0f * c), forward, out);
	    out[3] = size / (2.0f * c);
	}
}

static float compute_fade_factor(const shadow_light_submission_t &light, const vec3_t vieworg, float *distance_out = nullptr)
{
	if (light.fade_start <= 1.0f && light.fade_end <= 1.0f)
		return 1.0f;
	if (light.fade_end <= 0.0f)
		return 1.0f;

	const float distance = VectorDistance(vieworg, light.origin);
	if (distance_out)
		*distance_out = distance;
	const float frac_to_end = Q_clipf(distance / light.fade_end, 0.0f, 1.0f);
	const float min_fraction = light.fade_start / light.fade_end;

	if (min_fraction > 1.0f)
		return 1.0f;
	if (min_fraction <= 0.0f)
		return frac_to_end;

	return 1.0f - smoothstep(min_fraction, 1.0f, frac_to_end);
}

static shadow_light_submission_t sanitize_submission(const shadow_light_submission_t &light)
{
	shadow_light_submission_t result = light;

	result.radius = std::clamp(result.radius, 0.0f, kMaxShadowLightRadius);
	if (result.resolution < 0)
		result.resolution = 0;

	if (result.fade_end > 0.0f && result.fade_start > result.fade_end)
		std::swap(result.fade_start, result.fade_end);

	if (result.lighttype != shadow_light_type_cone || result.coneangle <= 0.0f) {
		result.lighttype = shadow_light_type_point;
		result.coneangle = 0.0f;
		VectorClear(result.direction);
	} else {
		result.coneangle = std::clamp(result.coneangle, 0.1f, 179.0f);
		if (VectorNormalize(result.direction) == 0.0f) {
			result.lighttype = shadow_light_type_point;
			result.coneangle = 0.0f;
			VectorClear(result.direction);
		}
	}

	return result;
}

} // namespace

void R_ClearShadowLights(void)
{
	g_shadowLights.clear();
	g_entityLookup.clear();
}

void R_QueueShadowLight(const shadow_light_submission_t &light)
{
	if (light.radius <= 0.0f || light.intensity <= 0.0f)
		return;

	shadow_light_submission_t sanitized = sanitize_submission(light);
	if (sanitized.radius <= 0.0f || sanitized.intensity <= 0.0f)
		return;

	if (sanitized.entity_number > 0) {
		auto [it, inserted] = g_entityLookup.try_emplace(sanitized.entity_number, g_shadowLights.size());
		if (!inserted) {
			g_shadowLights[it->second] = sanitized;
			return;
		}
	}

	g_shadowLights.push_back(sanitized);
}

/*
=============
R_CollectShadowLights

Collects queued shadow-casting lights, applying fade and style weighting,
and writes the results into the renderer's dynamic light array.
=============
*/
size_t R_CollectShadowLights(const vec3_t vieworg, const lightstyle_t *styles,
        dlight_t *dlights, size_t max_dlights)
{
	if (!dlights || max_dlights == 0)
		return 0;

	struct collected_light_t {
		const shadow_light_submission_t *light;
		float	intensity;
		float	distance;
		size_t	submission_index;
	};

	static std::vector<collected_light_t> collected;
	collected.clear();
	collected.reserve(g_shadowLights.size());

	for (size_t light_index = 0; light_index < g_shadowLights.size(); ++light_index) {
		const shadow_light_submission_t &light = g_shadowLights[light_index];
		float distance = 0.0f;
		const float fade = compute_fade_factor(light, vieworg, &distance);
		if (fade <= 0.0f)
			continue;

		float style_scale = 1.0f;
		if (styles && light.lightstyle >= 0 && light.lightstyle < MAX_LIGHTSTYLES)
			style_scale = styles[light.lightstyle].white;

		const float intensity = light.intensity * style_scale * fade;
		if (intensity <= 0.0f)
			continue;

		collected.push_back({ &light, intensity, distance, light_index });
	}

	if (collected.empty())
		return 0;

	const auto compare = [](const collected_light_t &lhs, const collected_light_t &rhs) {
		if (lhs.intensity != rhs.intensity)
			return lhs.intensity > rhs.intensity;
		return lhs.distance < rhs.distance;
	};

	if (collected.size() > max_dlights) {
		std::partial_sort(collected.begin(), collected.begin() + max_dlights, collected.end(), compare);
		collected.resize(max_dlights);
	} else {
		std::sort(collected.begin(), collected.end(), compare);
	}

	size_t count = 0;
	for (const collected_light_t &entry : collected) {
		dlight_t &out = dlights[count++];
		const shadow_light_submission_t &light = *entry.light;
		out.shadow_submission_index = static_cast<int>(entry.submission_index);
		out.shadow_view_base = -1;
		out.shadow_view_count = 0;
		VectorCopy(light.origin, out.origin);
		out.radius = light.radius;
		out.intensity = entry.intensity;
		out.color[0] = light.color.r / 255.0f;
		out.color[1] = light.color.g / 255.0f;
		out.color[2] = light.color.b / 255.0f;
		out.fade[0] = light.fade_start;
		out.fade[1] = light.fade_end;

		if (light.lighttype == shadow_light_type_cone && light.coneangle > 0.0f) {
			VectorCopy(light.direction, out.cone);
			out.cone[3] = DEG2RAD(light.coneangle);
			out.conecos = cosf(out.cone[3]);
			const float sine = sinf(out.cone[3]);
			cone_to_bounding_sphere(out.origin, out.cone, out.radius, out.cone[3], out.conecos, sine, out.sphere);
		} else {
			out.conecos = 0.0f;
			VectorClear(out.cone);
			out.cone[3] = 0.0f;
			VectorCopy(out.origin, out.sphere);
			out.sphere[3] = out.radius;
		}
	}

	return count;
}

const shadow_light_submission_t *R_GetQueuedShadowLights(size_t *count)
{
	if (count)
	    *count = g_shadowLights.size();
	return g_shadowLights.data();
}
