#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "client/video.h"
#include "common/cvar.h"
#include "common/math.h"
#include "common/common.h"
#include "refresh/refresh.h"

namespace refresh::vk {

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool init(bool total);
    void shutdown(bool total);

    void beginRegistration(const char *map);
    qhandle_t registerModel(const char *name);
    qhandle_t registerImage(const char *name, imagetype_t type, imageflags_t flags);
    void setSky(const char *name, float rotate, bool autorotate, const vec3_t axis);
    void endRegistration();

    void beginFrame();
    void endFrame();
    void renderFrame(const refdef_t *fd);
    void lightPoint(const vec3_t origin, vec3_t light) const;

    void setClipRect(const clipRect_t *clip);
    float clampScale(cvar_t *var) const;
    void setScale(float scale);
    int autoScale() const;

    void drawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font);
    void drawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font);
    int drawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                          const char *string, color_t color, qhandle_t font);
    int drawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint,
                      color_t color, const kfont_t *kfont);

    bool getPicSize(int *w, int *h, qhandle_t pic) const;
    void drawPic(int x, int y, color_t color, qhandle_t pic);
    void drawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void drawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                              int pivot_x, int pivot_y, qhandle_t pic);
    void drawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void drawStretchRaw(int x, int y, int w, int h);
    void updateRawPic(int pic_w, int pic_h, const uint32_t *pic);
    void tileClear(int x, int y, int w, int h, qhandle_t pic);
    void drawFill8(int x, int y, int w, int h, int c);
    void drawFill32(int x, int y, int w, int h, color_t color);

    void modeChanged(int width, int height, int flags);
    bool videoSync() const;
    void expireDebugObjects();
    bool supportsPerPixelLighting() const;
    r_opengl_config_t getGLConfig() const;

    void loadKFont(kfont_t *font, const char *filename);
    const kfont_char_t *lookupKFontChar(const kfont_t *kfont, uint32_t codepoint) const;

private:
    struct SkyDefinition {
        std::string name;
        float rotate = 0.0f;
        bool autorotate = false;
        std::array<float, 3> axis{ 0.0f, 0.0f, 1.0f };
    };

    struct ModelRecord {
        qhandle_t handle = 0;
        std::string name;
        unsigned registrationSequence = 0;
    };

    struct ImageRecord {
        qhandle_t handle = 0;
        std::string name;
        imagetype_t type = IT_PIC;
        imageflags_t flags = IF_NONE;
        int width = 0;
        int height = 0;
        bool transparent = false;
        unsigned registrationSequence = 0;
    };

    struct RawPicState {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;
    };

    using ModelMap = std::unordered_map<qhandle_t, ModelRecord>;
    using ImageMap = std::unordered_map<qhandle_t, ImageRecord>;
    using NameLookup = std::unordered_map<std::string, qhandle_t>;

    qhandle_t nextHandle();
    qhandle_t registerResource(NameLookup &lookup, std::string_view name);

    void resetTransientState();

    std::atomic<qhandle_t> handleCounter_;
    bool initialized_ = false;
    bool frameActive_ = false;

    SkyDefinition sky_{};
    std::string currentMap_;

    std::optional<clipRect_t> clipRect_;
    float scale_ = 1.0f;
    int autoScaleValue_ = 1;

    ModelMap models_;
    ImageMap images_;
    NameLookup modelLookup_;
    NameLookup imageLookup_;
    RawPicState rawPic_;
};

} // namespace refresh::vk


