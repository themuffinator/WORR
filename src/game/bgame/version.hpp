#pragma once

#include <string_view>

#if defined(__has_include)
#if __has_include("version_autogen.hpp")
#include "version_autogen.hpp"
#endif
#else
#include "version_autogen.hpp"
#endif

namespace worr::version {

	inline constexpr std::string_view kGameTitle{"WORR"};

	namespace detail {
#if defined(WORR_SEMVER)
	inline constexpr std::string_view kVersionSource{WORR_SEMVER};
#elif defined(WORR_GENERATED_VERSION_STRING)
	inline constexpr std::string_view kVersionSource{WORR_GENERATED_VERSION_STRING};
#elif defined(WORR_GENERATED_VERSION)
	inline constexpr std::string_view kVersionSource{WORR_GENERATED_VERSION};
#elif defined(WORR_VERSION_STRING)
	inline constexpr std::string_view kVersionSource{WORR_VERSION_STRING};
#elif defined(WORR_VERSION)
	inline constexpr std::string_view kVersionSource{WORR_VERSION};
#else
	inline constexpr std::string_view kVersionSource{"0.0.0-dev"};
#endif
	}  // namespace detail

	inline constexpr std::string_view kGameVersion{detail::kVersionSource};

#if defined(WORR_VERSION_MAJOR)
	inline constexpr int kMajor = WORR_VERSION_MAJOR;
#else
	inline constexpr int kMajor = 0;
#endif

#if defined(WORR_VERSION_MINOR)
	inline constexpr int kMinor = WORR_VERSION_MINOR;
#else
	inline constexpr int kMinor = 0;
#endif

#if defined(WORR_VERSION_PATCH)
	inline constexpr int kPatch = WORR_VERSION_PATCH;
#else
	inline constexpr int kPatch = 0;
#endif

#if defined(WORR_VERSION_PRERELEASE)
	inline constexpr std::string_view kPrerelease{WORR_VERSION_PRERELEASE};
	inline constexpr bool kHasPrerelease = (sizeof(WORR_VERSION_PRERELEASE) > 1);
#else
	inline constexpr std::string_view kPrerelease{};
	inline constexpr bool kHasPrerelease = false;
#endif

#if defined(WORR_BUILD_METADATA)
	inline constexpr std::string_view kBuildMetadata{WORR_BUILD_METADATA};
	inline constexpr bool kHasBuildMetadata = (sizeof(WORR_BUILD_METADATA) > 1);
#else
	inline constexpr std::string_view kBuildMetadata{};
	inline constexpr bool kHasBuildMetadata = false;
#endif

}  // namespace worr::version
