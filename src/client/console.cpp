#include "client.hpp"

#include "common/files.hpp"
#include "common/q3colors.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <string>
#include <string_view>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef clamp
#undef clamp
#endif

namespace {

	constexpr int kConsoleTimes = 16;
	constexpr int kConsoleTimesMask = kConsoleTimes - 1;
	constexpr int kTotalLines = 1024;
	constexpr int kTotalLinesMask = kTotalLines - 1;
	constexpr int kLegacyLineWidth = 126;
	constexpr int kPromptPadding = 2;
	constexpr int kScrollLargeStep = 6;
	constexpr int kScrollSmallStep = 2;
	constexpr float kMinConsoleHeight = 0.1f;

	class Console;

	static void Con_Dump_c(genctx_t* ctx, int argnum);
	static void CL_RemoteMode_c(genctx_t* ctx, int argnum);

	enum class ChatMode {
		None,
		Default,
		Team,
	};

	enum class ConsoleMode {
		Popup,
		Default,
		Remote,
	};

	struct ConsoleLine {
		color_index_t color{ COLOR_INDEX_NONE };
		std::string timestamp{};
		std::string content{};
		int timestampPixelWidth{ 0 };

		void clear() noexcept
		{
			timestamp.clear();
			content.clear();
			timestampPixelWidth = 0;
			color = COLOR_INDEX_NONE;
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return timestamp.empty() && content.empty();
		}

		[[nodiscard]] std::string combined() const
		{
			std::string result;
			result.reserve(timestamp.size() + content.size());
			result.append(timestamp);
			result.append(content);
			return result;
		}
	};

	class Console {
	public:
		static Console& instance()
		{
			static Console console;
			return console;
		}

		void init();
		void postInit();
		void shutdown();

		void skipNotify(bool skip) noexcept { skipNotify_ = skip; }
		void clearTyping();
		void close(bool force);
		void popup(bool force);
		void toggle(ConsoleMode mode, ChatMode chat);

		void setColor(color_index_t color) noexcept { currentColor_ = color; }
		void loadState(load_state_t state);

		void print(std::string_view text);
		void draw();
		void run();
		void registerMedia();
		void checkResize();
		void clearNotify();

		void keyEvent(int key);
		void charEvent(int key);
		void messageKeyEvent(int key);
		void messageCharEvent(int key);

		void clear();
		void dumpToFile(const char* name) const;
		void messageMode(ChatMode mode);
		void remoteMode();

		[[nodiscard]] bool initialized() const noexcept { return initialized_; }

		commandPrompt_t& prompt() noexcept { return prompt_; }
		commandPrompt_t& chatPrompt() noexcept { return chatPrompt_; }

	private:
		Console();

		void ensureCarriageReturn();
		void carriageReturn();
		void lineFeed();
		void advanceLine();
		void updateNotifyTime();

		void interactiveMode();
		void executePrompt();
		void paste(char* (*provider)(void));
		void searchUp();
		void searchDown();
		void sendChat(const char* msg);

		[[nodiscard]] qhandle_t fontHandle() const noexcept;
		[[nodiscard]] int lineHeight() const noexcept;
		[[nodiscard]] int charWidth() const noexcept;
		[[nodiscard]] int availablePixelWidth() const noexcept;

		[[nodiscard]] ConsoleLine& currentLine() noexcept
		{
			return lines_[currentIndex_ & kTotalLinesMask];
		}

		[[nodiscard]] const ConsoleLine& lineForRow(int row) const noexcept
		{
			return lines_[row & kTotalLinesMask];
		}

		void appendToCurrent(std::string_view chunk);
		void appendCharacter(char ch);
		void appendSpace();
		void appendTab();
		void appendControl(char ch);

		[[nodiscard]] bool needsWrap(std::string_view word) const;
		[[nodiscard]] int measureContentPixels(std::string_view text) const;
		void updateContentWidth();

		void drawSolid();
		void drawNotify();
		int drawLine(int y, int row, float alpha, bool notify);
		void refreshTimestampColor();

	private:
		std::array<ConsoleLine, kTotalLines> lines_{};
		std::array<unsigned, kConsoleTimes> notifyTimes_{};

		int currentIndex_{ 0 };
		int displayIndex_{ 0 };
		int contentPixelWidth_{ 0 };
		int lineWrapColumns_{ 0 };

		int vidWidth_{ 0 };
		int vidHeight_{ 0 };
		float scale_{ 1.0f };

		color_index_t currentColor_{ COLOR_INDEX_NONE };
		bool skipNotify_{ false };
		bool initialized_{ false };

		char pendingNewline_{ '\r' };

		qhandle_t backgroundImage_{ 0 };
		qhandle_t registeredFont_{ 0 };

		float currentHeight_{ 0.0f };
		float destinationHeight_{ 0.0f };

		commandPrompt_t prompt_{};
		commandPrompt_t chatPrompt_{};

		ChatMode chatMode_{ ChatMode::None };
		ConsoleMode mode_{ ConsoleMode::Popup };
		netadr_t remoteAddress_{};
		std::string remotePassword_{};

		load_state_t loadState_{ LOAD_NONE };

		cvar_t* notifyTime_{ nullptr };
		cvar_t* notifyLines_{ nullptr };
		cvar_t* clock_{ nullptr };
		cvar_t* height_{ nullptr };
		cvar_t* speed_{ nullptr };
		cvar_t* alpha_{ nullptr };
		cvar_t* scaleCvar_{ nullptr };
		cvar_t* font_{ nullptr };
		cvar_t* background_{ nullptr };
		cvar_t* scroll_{ nullptr };
		cvar_t* history_{ nullptr };
		cvar_t* timestamps_{ nullptr };
		cvar_t* timestampsFormat_{ nullptr };
		cvar_t* timestampsColor_{ nullptr };
		cvar_t* autoChat_{ nullptr };

		color_t timestampColor_{ ColorRGB(170, 170, 170) };
	};

	Console::Console()
	{
		IF_Init(&prompt_.inputLine, 0, MAX_FIELD_TEXT - 1);
		IF_Init(&chatPrompt_.inputLine, 0, MAX_FIELD_TEXT - 1);
		prompt_.printf = Con_Printf;
	}

	void Console::init()
	{
		if (initialized_)
			return;

                static const cmdreg_t consoleCommands[] = {
                        { "toggleconsole", Con_ToggleConsole_f },
                        { "messagemode", Con_MessageMode_f },
                        { "messagemode2", Con_MessageMode2_f },
                        { "remotemode", Con_RemoteMode_f, CL_RemoteMode_c },
                        { "clear", Con_Clear_f },
                        { "clearnotify", Con_ClearNotify_f },
                        { "condump", Con_Dump_f, Con_Dump_c },
                        { nullptr, nullptr }
                };
		Cmd_Register(consoleCommands);

		notifyTime_ = Cvar_Get("con_notifytime", "3", 0);
		notifyTime_->changed = cl_timeout_changed;
		notifyTime_->changed(notifyTime_);
		notifyLines_ = Cvar_Get("con_notifylines", "4", 0);
		clock_ = Cvar_Get("con_clock", "0", 0);
		height_ = Cvar_Get("con_height", "0.5", 0);
		speed_ = Cvar_Get("scr_conspeed", "3", 0);
		alpha_ = Cvar_Get("con_alpha", "1", 0);
		scaleCvar_ = Cvar_Get("con_scale", "0", 0);
		scaleCvar_->changed = [](cvar_t* self) {
			if (cls.ref_initialized)
				Console::instance().checkResize();
			};

		font_ = Cvar_Get("con_font", "/fonts/RobotoMono-Regular.ttf", 0);
		font_->changed = [](cvar_t* self) {
			if (cls.ref_initialized)
				Console::instance().registerMedia();
			};

		background_ = Cvar_Get("con_background", "conback", 0);
		background_->changed = [](cvar_t* self) {
			if (cls.ref_initialized)
				Console::instance().registerMedia();
			};

		scroll_ = Cvar_Get("con_scroll", "0", 0);
		history_ = Cvar_Get("con_history", STRINGIFY(HISTORY_SIZE), 0);
		timestamps_ = Cvar_Get("con_timestamps", "0", 0);
		timestamps_->changed = [](cvar_t* self) {
			if (cls.ref_initialized)
				Console::instance().checkResize();
			};

		timestampsFormat_ = Cvar_Get("con_timestampsformat", "%H:%M:%S ", 0);
		timestampsFormat_->changed = [](cvar_t* self) {
			if (cls.ref_initialized)
				Console::instance().checkResize();
			};

		timestampsColor_ = Cvar_Get("con_timestampscolor", "#aaa", 0);
		timestampsColor_->changed = [](cvar_t* self) {
			Console::instance().refreshTimestampColor();
			};
		refreshTimestampColor();

		autoChat_ = Cvar_Get("con_auto_chat", "0", 0);

		r_config.width = SCREEN_WIDTH;
		r_config.height = SCREEN_HEIGHT;

		scale_ = 1.0f;
		lineWrapColumns_ = -1;
		pendingNewline_ = '\r';
		currentColor_ = COLOR_INDEX_NONE;

		checkResize();

		initialized_ = true;
	}

	void Console::postInit()
	{
		if (history_->integer > 0) {
			Prompt_LoadHistory(&prompt_, COM_HISTORYFILE_NAME);
		}
	}

	void Console::shutdown()
	{
		if (!initialized_)
			return;

		if (history_->integer > 0) {
			Prompt_SaveHistory(&prompt_, COM_HISTORYFILE_NAME, history_->integer);
		}

		Prompt_Clear(&prompt_);
		Prompt_Clear(&chatPrompt_);
		remotePassword_.clear();
		initialized_ = false;
	}

	void Console::clearTyping()
	{
		IF_Clear(&prompt_.inputLine);
		Prompt_ClearState(&prompt_);
	}

	void Console::close(bool force)
	{
		if (mode_ > ConsoleMode::Popup && !force)
			return;

		if (cls.state < ca_active && !(cls.key_dest & KEY_MENU))
			return;

		clearTyping();
		clearNotify();

		Key_SetDest(Key_FromMask(cls.key_dest & ~KEY_CONSOLE));

		destinationHeight_ = currentHeight_ = 0.0f;
		mode_ = ConsoleMode::Popup;
		chatMode_ = ChatMode::None;
	}

	void Console::popup(bool force)
	{
		if (force)
			mode_ = ConsoleMode::Popup;

		Key_SetDest(Key_FromMask(cls.key_dest | KEY_CONSOLE));
		run();
	}

	void Console::toggle(ConsoleMode mode, ChatMode chat)
	{
		SCR_EndLoadingPlaque();

		clearTyping();
		clearNotify();

		if (cls.key_dest & KEY_CONSOLE) {
			Key_SetDest(Key_FromMask(cls.key_dest & ~KEY_CONSOLE));
			mode_ = ConsoleMode::Popup;
			chatMode_ = ChatMode::None;
			return;
		}

		Key_SetDest(Key_FromMask((cls.key_dest | KEY_CONSOLE) & ~KEY_MESSAGE));
		mode_ = mode;
		chatMode_ = chat;
	}

	void Console::loadState(load_state_t state)
	{
		loadState_ = state;
		SCR_UpdateScreen();
		if (vid)
			vid->pump_events();
		S_GetSoundSystem().Update();
	}

	void Console::ensureCarriageReturn()
	{
		if (pendingNewline_) {
			if (pendingNewline_ == '\n')
				lineFeed();
			else
				carriageReturn();
			pendingNewline_ = 0;
		}
	}

	void Console::carriageReturn()
	{
		ConsoleLine& line = currentLine();
		line.clear();
		line.color = currentColor_;

		if (timestamps_->integer) {
			char timestampBuffer[kLegacyLineWidth]{};
			int written = Com_FormatLocalTime(timestampBuffer, sizeof(timestampBuffer), timestampsFormat_->string);
			if (written > 0) {
				line.timestamp.assign(timestampBuffer, timestampBuffer + written);
				line.timestampPixelWidth = measureContentPixels(line.timestamp);
			}
		}

		contentPixelWidth_ = 0;
		updateNotifyTime();
	}

	void Console::updateNotifyTime()
	{
		if (!skipNotify_)
			notifyTimes_[currentIndex_ & kConsoleTimesMask] = cls.realtime;
	}

	void Console::advanceLine()
	{
		if (displayIndex_ == currentIndex_)
			++displayIndex_;

		++currentIndex_;

		if (scroll_->integer & 2)
			displayIndex_ = currentIndex_;
		else {
			int top = currentIndex_ - kTotalLines + 1;
			if (top < 0)
				top = 0;
			if (displayIndex_ < top)
				displayIndex_ = top;
		}

		if (currentIndex_ >= kTotalLines * 2) {
			currentIndex_ -= kTotalLines;
			displayIndex_ -= kTotalLines;
		}
	}

	void Console::lineFeed()
	{
		advanceLine();
		carriageReturn();
	}

	void Console::appendToCurrent(std::string_view chunk)
	{
		if (chunk.empty())
			return;

		ConsoleLine& line = currentLine();
		line.content.append(chunk);
		updateContentWidth();
	}

	void Console::appendCharacter(char ch)
	{
		ensureCarriageReturn();

		char text[2] = { ch, 0 };
		const int charPixels = SCR_MeasureString(1, 0, 1, text, fontHandle());
		if (contentPixelWidth_ + charPixels > availablePixelWidth() && contentPixelWidth_ > 0) {
			lineFeed();
			ensureCarriageReturn();
		}

		ConsoleLine& line = currentLine();
		line.content.push_back(ch);
		updateContentWidth();
	}

	void Console::appendSpace()
	{
		appendToCurrent(" ");
	}

	void Console::appendTab()
	{
		constexpr int tabSize = 4;
		const int position = contentPixelWidth_ / std::max(charWidth(), 1);
		const int spacesToInsert = tabSize - (position % tabSize);
		for (int i = 0; i < spacesToInsert; ++i)
			appendSpace();
	}

	void Console::appendControl(char ch)
	{
		switch (ch) {
		case '\n':
		case '\r':
			pendingNewline_ = ch;
			break;
		case '\t':
			appendTab();
			break;
		default:
			appendCharacter(' ');
			break;
		}
	}

	int Console::measureContentPixels(std::string_view text) const
	{
		if (text.empty())
			return 0;

		return SCR_MeasureString(1, 0, text.size(), text.data(), fontHandle());
	}

	void Console::updateContentWidth()
	{
		contentPixelWidth_ = measureContentPixels(currentLine().content);
	}

	bool Console::needsWrap(std::string_view word) const
	{
		if (word.empty())
			return false;

		const int wordPixels = SCR_MeasureString(1, 0, word.size(), word.data(), fontHandle());
		if (wordPixels >= availablePixelWidth())
			return false;

		return contentPixelWidth_ + wordPixels > availablePixelWidth();
	}

	void Console::print(std::string_view text)
	{
		if (!initialized_ || text.empty())
			return;

		const char* ptr = text.data();
		size_t remaining = text.size();

		color_t ignoredColor;

		while (remaining) {
			ensureCarriageReturn();

			size_t consumed = 0;
			if (Q3_ParseColorEscape(ptr, remaining, ignoredColor, consumed)) {
				appendToCurrent(std::string_view(ptr, consumed));
				ptr += consumed;
				remaining -= consumed;
				continue;
			}

			if (*ptr > 32) {
				size_t wordLen = 0;
				size_t inspectRemaining = remaining;
				const char* inspectPtr = ptr;
				while (inspectRemaining && *inspectPtr > 32) {
					size_t colorConsumed = 0;
					if (Q3_ParseColorEscape(inspectPtr, inspectRemaining, ignoredColor, colorConsumed)) {
						inspectPtr += colorConsumed;
						inspectRemaining -= colorConsumed;
						wordLen += colorConsumed;
						continue;
					}
					++inspectPtr;
					--inspectRemaining;
					++wordLen;
				}

				if (wordLen && needsWrap(std::string_view(ptr, wordLen))) {
					lineFeed();
					ensureCarriageReturn();
				}
			}

			const char ch = *ptr;
			if (ch == '\r' || ch == '\n' || ch == '\t') {
				appendControl(ch);
			}
			else if (ch == ' ') {
				appendCharacter(' ');
			}
			else {
				appendCharacter(ch);
			}

			++ptr;
			--remaining;
		}

		updateNotifyTime();
	}

	void Console::drawSolid()
	{
		const int visibleLines = std::clamp(static_cast<int>(vidHeight_ * currentHeight_), 0, vidHeight_);
		if (!visibleLines)
			return;

		color_t drawColor = COLOR_WHITE;

		if (cls.state >= ca_active && !(cls.key_dest & KEY_MENU) && alpha_->value) {
			const float alphaScale = 0.5f + 0.5f * (currentHeight_ / std::max(height_->value, kMinConsoleHeight));
			drawColor.a *= alphaScale * Cvar_ClampValue(alpha_, 0, 1);
		}

		if (cls.state < ca_active || (cls.key_dest & KEY_MENU) || alpha_->value) {
			if (backgroundImage_)
				R_DrawKeepAspectPic(0, visibleLines - vidHeight_, vidWidth_, vidHeight_, drawColor, backgroundImage_);
		}

		int y = visibleLines - (lineHeight() * 3 + lineHeight() / 4);
		int rows = y / lineHeight() + 1;

		if (displayIndex_ != currentIndex_) {
			const int glyphAdvance = std::max(charWidth(), 1);
			const int spacing = glyphAdvance * 2;
			for (int i = 1; i < vidWidth_ / spacing; i += 4) {
				SCR_DrawGlyph(i * glyphAdvance, y, 1, 0, '^', ColorSetAlpha(COLOR_RED, drawColor.a));
			}
			y -= lineHeight();
			--rows;
		}

		int row = displayIndex_;

		for (int i = 0; i < rows; ++i) {
			if (row < 0)
				break;
			if (currentIndex_ - row > kTotalLines - 1)
				break;

			int x = drawLine(y, row, 1.0f, false);

			y -= lineHeight();
			--row;
		}

		if (cls.download.current) {
			char pos[16];
			char suffix[32];
			const char* text = cls.download.current->path;
			if (const char* slash = strrchr(text, '/'))
				text = slash + 1;

			Com_FormatSizeLong(pos, sizeof(pos), cls.download.position);
			const int suffixLen = Q_scnprintf(suffix, sizeof(suffix), " %d%% (%s)", cls.download.percent, pos);

			const int maxWidth = lineWrapColumns_;
			std::string fileName{ text };
			if (maxWidth > 6 && static_cast<int>(fileName.size()) > maxWidth / 3) {
				fileName = fileName.substr(0, std::max(0, maxWidth / 3 - 3)) + "...";
			}

			std::string buffer = fileName + ": ";
			buffer.push_back('\x80');
			const int fill = std::max(0, maxWidth - static_cast<int>(fileName.size()) - suffixLen);
			const int dot = fill * cls.download.percent / 100;
			for (int i = 0; i < fill; ++i)
				buffer.push_back(i == dot ? '\x83' : '\x81');
			buffer.push_back('\x82');
			buffer.append(suffix);

			SCR_DrawStringStretch(charWidth(), visibleLines - (lineHeight() * 3), 1, 0, buffer.size(), buffer.c_str(), COLOR_WHITE, fontHandle());
		}
		else if (cls.state == ca_loading) {
			const char* text = nullptr;
			switch (loadState_) {
			case LOAD_MAP: text = cl.configstrings[cl.csr.models + 1]; break;
			case LOAD_MODELS: text = "models"; break;
			case LOAD_IMAGES: text = "images"; break;
			case LOAD_CLIENTS: text = "clients"; break;
			case LOAD_SOUNDS: text = "sounds"; break;
			default: break;
			}

			if (text) {
				char buffer[128];
				Q_snprintf(buffer, sizeof(buffer), "Loading %s...", text);
				SCR_DrawStringStretch(charWidth(), visibleLines - (lineHeight() * 3), 1, 0, strlen(buffer), buffer, COLOR_WHITE, fontHandle());
			}
		}

		const int baseFooterY = visibleLines - (lineHeight() * 2);

		if (cls.key_dest & KEY_CONSOLE) {
			const int inputY = baseFooterY;
			const int promptGlyph = mode_ == ConsoleMode::Remote ? '#' : 17;
			SCR_DrawGlyph(charWidth(), inputY, 1, 0, promptGlyph, COLOR_YELLOW);
			const int promptRight = IF_Draw(&prompt_.inputLine, charWidth() * 2, inputY, UI_DRAWCURSOR, fontHandle());
			const int promptWidth = std::max(0, promptRight - charWidth() * 2);

#ifdef APPNAME
			constexpr std::string_view appVersion = APPNAME " " VERSION;
#else
			constexpr std::string_view appVersion = VERSION;
#endif
			const int versionWidth = SCR_MeasureString(1, 0, appVersion.size(), appVersion.data(), fontHandle());

			int footerY = baseFooterY;
			if (promptWidth + versionWidth + charWidth() > vidWidth_)
				footerY -= lineHeight();

			if (clock_->integer) {
				char buffer[64];
				const int len = Com_Time_m(buffer, sizeof(buffer));
				if (len > 0) {
					const int width = SCR_MeasureString(1, 0, len + 1, buffer, fontHandle());
					SCR_DrawStringStretch(vidWidth_ - width, footerY - lineHeight(), 1, UI_RIGHT, len, buffer, COLOR_CYAN, fontHandle());
				}
			}

			SCR_DrawStringStretch(vidWidth_ - versionWidth, footerY, 1, UI_RIGHT, appVersion.size(), appVersion.data(), COLOR_CYAN, fontHandle());
			return;
		}

#ifdef APPNAME
		constexpr std::string_view appVersion = APPNAME " " VERSION;
#else
		constexpr std::string_view appVersion = VERSION;
#endif
		const int versionWidth = SCR_MeasureString(1, 0, appVersion.size(), appVersion.data(), fontHandle());

		int footerY = baseFooterY;

		if (clock_->integer) {
			char buffer[64];
			const int len = Com_Time_m(buffer, sizeof(buffer));
			if (len > 0) {
				const int width = SCR_MeasureString(1, 0, len + 1, buffer, fontHandle());
				SCR_DrawStringStretch(vidWidth_ - width, footerY - lineHeight(), 1, UI_RIGHT, len, buffer, COLOR_CYAN, fontHandle());
			}
		}

		SCR_DrawStringStretch(vidWidth_ - versionWidth, footerY, 1, UI_RIGHT, appVersion.size(), appVersion.data(), COLOR_CYAN, fontHandle());
	}

	void Console::drawNotify()
	{
		if (cls.state != ca_active)
			return;
		if (cls.key_dest & (KEY_MENU | KEY_CONSOLE))
			return;
		if (currentHeight_)
			return;

		const int notifyCount = std::clamp(notifyLines_->integer, 0, kConsoleTimes);
		const int startRow = currentIndex_ - notifyCount + 1;
		const int minRow = std::max(0, currentIndex_ - kTotalLines + 1);

		int y = 0;

		for (int i = startRow; i <= currentIndex_; ++i) {
			if (i < minRow)
				continue;

			const unsigned time = notifyTimes_[i & kConsoleTimesMask];
			if (!time)
				continue;

			const float delta = (cls.realtime - time) * 0.001f;
			if (delta >= notifyTime_->value)
				continue;

			const float alpha = 1.0f - delta / notifyTime_->value;
			y += lineHeight();
			drawLine(y, i, alpha, true);
		}

		if (cls.key_dest & KEY_MESSAGE) {
			const char* label = chatMode_ == ChatMode::Team ? "say_team:" : "say:";
			const int skip = chatMode_ == ChatMode::Team ? 11 : 5;
			SCR_DrawStringStretch(charWidth(), y, 1, 0, strlen(label), label, COLOR_WHITE, fontHandle());
			chatPrompt_.inputLine.visibleChars = std::max(1, lineWrapColumns_ - skip + 1);
			IF_Draw(&chatPrompt_.inputLine, skip * charWidth(), y, UI_DRAWCURSOR, fontHandle());
		}
	}

	int Console::drawLine(int y, int row, float alpha, bool notify)
	{
		const ConsoleLine& line = lineForRow(row);
		int x = charWidth();
		int width = lineWrapColumns_;

		if (notify)
			width -= static_cast<int>(line.timestamp.size());

		if (!line.timestamp.empty() && !notify) {
			const color_t tsColor = ColorSetAlpha(timestampColor_, alpha);
			SCR_DrawStringStretch(x, y, 1, 0, line.timestamp.size(), line.timestamp.c_str(), tsColor, fontHandle());
			x += line.timestampPixelWidth;
			width -= static_cast<int>(line.timestamp.size());
		}

		if (width < 1)
			return x;

		int flags = 0;
		color_t drawColor = COLOR_WHITE;

		switch (line.color) {
		case COLOR_INDEX_ALT:
			flags = UI_ALTCOLOR;
			break;
		case COLOR_INDEX_NONE:
			break;
		default:
			drawColor = colorTable[line.color & 7];
			break;
		}

		drawColor.a *= alpha;
		SCR_DrawStringStretch(x, y, 1, flags, line.content.size(), line.content.c_str(), drawColor, fontHandle());
		return x + measureContentPixels(line.content);
	}

	void Console::draw()
	{
		R_SetScale(scale_);
		drawSolid();
		drawNotify();
		R_SetScale(1.0f);
	}

	void Console::run()
	{
		if (cls.disable_screen) {
			destinationHeight_ = currentHeight_ = 0.0f;
			return;
		}

		if (!(cls.key_dest & KEY_MENU)) {
			if (cls.state == ca_disconnected) {
				destinationHeight_ = currentHeight_ = 1.0f;
				return;
			}
			if (cls.state > ca_disconnected && cls.state < ca_active) {
				destinationHeight_ = currentHeight_ = 0.5f;
				return;
			}
		}

		if (cls.key_dest & KEY_CONSOLE)
			destinationHeight_ = Cvar_ClampValue(height_, kMinConsoleHeight, 1.0f);
		else
			destinationHeight_ = 0.0f;

		if (speed_->value <= 0.0f) {
			currentHeight_ = destinationHeight_;
			return;
		}

		CL_AdvanceValue(&currentHeight_, destinationHeight_, speed_->value);
	}

	qhandle_t Console::fontHandle() const noexcept
	{
		if (registeredFont_)
			return registeredFont_;
		return SCR_DefaultFontHandle();
	}

	int Console::lineHeight() const noexcept
	{
		return SCR_FontLineHeight(1, fontHandle());
	}

	int Console::charWidth() const noexcept
	{
		return std::max(1, SCR_MeasureString(1, UI_IGNORECOLOR, 1, " ", fontHandle()));
	}

	int Console::availablePixelWidth() const noexcept
	{
		return std::max(0, vidWidth_ - charWidth() * kPromptPadding);
	}

	void Console::registerMedia()
	{
		registeredFont_ = R_RegisterFont(font_->string);
		if (!registeredFont_) {
			if (strcmp(font_->string, font_->default_string)) {
				Cvar_Reset(font_);
				registeredFont_ = R_RegisterFont(font_->default_string);
			}
			if (!registeredFont_)
				registeredFont_ = R_RegisterFont("conchars");
			if (!registeredFont_)
				Com_Error(ERR_FATAL, "%s", Com_GetLastError());
		}

		backgroundImage_ = R_RegisterPic(background_->string);
		if (!backgroundImage_ && strcmp(background_->string, background_->default_string)) {
			Cvar_Reset(background_);
			backgroundImage_ = R_RegisterPic(background_->default_string);
		}

		checkResize();
	}

	void Console::checkResize()
	{
		scale_ = R_ClampScale(scaleCvar_);

		vidWidth_ = Q_rint(r_config.width * scale_);
		vidHeight_ = Q_rint(r_config.height * scale_);

		const int widthInPixels = availablePixelWidth();
		const int charWidthPixels = std::max(charWidth(), 1);
		lineWrapColumns_ = std::clamp(widthInPixels / charWidthPixels, 0, kLegacyLineWidth);

		const int visibleChars = std::max(lineWrapColumns_, 1);
		prompt_.inputLine.visibleChars = visibleChars;
		prompt_.widthInChars = visibleChars;
		chatPrompt_.inputLine.visibleChars = visibleChars;

		if (timestamps_->integer) {
			char temp[kLegacyLineWidth];
			const int timestampChars = Com_FormatLocalTime(temp, lineWrapColumns_, timestampsFormat_->string);
			prompt_.widthInChars = std::max(1, prompt_.widthInChars - timestampChars);
		}
	}

	void Console::clearNotify()
	{
		notifyTimes_.fill(0);
	}

	void Console::interactiveMode()
	{
		if (mode_ == ConsoleMode::Popup)
			mode_ = ConsoleMode::Default;
	}

	void Console::executePrompt()
	{
		const char* cmd = Prompt_Action(&prompt_);
		interactiveMode();

		if (!cmd) {
			Con_Printf("]\n");
			return;
		}

		const bool hasBackslash = cmd[0] == '\\' || cmd[0] == '/';

		if (mode_ == ConsoleMode::Remote) {
			CL_SendRcon(&remoteAddress_, remotePassword_.c_str(), cmd + hasBackslash);
		}
		else {
			if (!hasBackslash && cls.state == ca_active) {
				switch (autoChat_->integer) {
				case static_cast<int>(ChatMode::Default):
					Cbuf_AddText(&cmd_buffer, "cmd say ");
					break;
				case static_cast<int>(ChatMode::Team):
					Cbuf_AddText(&cmd_buffer, "cmd say_team ");
					break;
				}
			}
			Cbuf_AddText(&cmd_buffer, cmd + hasBackslash);
			Cbuf_AddText(&cmd_buffer, "\n");
		}

		Con_Printf("]%s\n", cmd);

		if (cls.state == ca_disconnected)
			SCR_UpdateScreen();
	}

	void Console::paste(char* (*provider)(void))
	{
		interactiveMode();

		if (!provider)
			return;

		char* data = provider();
		if (!data)
			return;

		char* cursor = data;
		while (*cursor) {
			const int c = *cursor++;
			switch (c) {
			case '\n':
				if (*cursor)
					executePrompt();
				break;
			case '\r':
			case '\t':
				IF_CharEvent(&prompt_.inputLine, ' ');
				break;
			default:
				IF_CharEvent(&prompt_.inputLine, Q_isprint(c) ? c : '?');
				break;
			}
		}

		Z_Free(data);
	}

	void Console::sendChat(const char* msg)
	{
		if (!msg || !*msg)
			return;

		const char* suffix = chatMode_ == ChatMode::Team ? "_team" : "";
		CL_ClientCommand(va("say%s \"%s\"", suffix, msg));
	}

	void Console::clear()
	{
		for (auto& line : lines_)
			line.clear();
		currentIndex_ = displayIndex_ = 0;
		contentPixelWidth_ = 0;
		notifyTimes_.fill(0);
		pendingNewline_ = '\r';
	}

	void Console::searchUp()
	{
		char buffer[kLegacyLineWidth + 1];
		const char* needle = prompt_.inputLine.text;
		int top = currentIndex_ - kTotalLines + 1;
		if (top < 0)
			top = 0;
		if (!*needle)
			return;

		for (int row = displayIndex_ - 1; row >= top; --row) {
			const ConsoleLine& line = lineForRow(row);
			const std::string& content = line.content;
			const int length = std::min(static_cast<int>(content.size()), kLegacyLineWidth);
			for (int i = 0; i < length; ++i)
				buffer[i] = Q_charascii(content[i]);
			buffer[length] = 0;
			if (Q_stristr(buffer, needle)) {
				displayIndex_ = row;
				break;
			}
		}
	}

	void Console::searchDown()
	{
		char buffer[kLegacyLineWidth + 1];
		const char* needle = prompt_.inputLine.text;
		if (!*needle)
			return;

		for (int row = displayIndex_ + 1; row <= currentIndex_; ++row) {
			const ConsoleLine& line = lineForRow(row);
			const std::string& content = line.content;
			const int length = std::min(static_cast<int>(content.size()), kLegacyLineWidth);
			for (int i = 0; i < length; ++i)
				buffer[i] = Q_charascii(content[i]);
			buffer[length] = 0;
			if (Q_stristr(buffer, needle)) {
				displayIndex_ = row;
				break;
			}
		}
	}

	void Console::keyEvent(int key)
	{
		switch (key) {
		case '`':
		case '~':
		case K_ESCAPE:
			toggle(ConsoleMode::Default, ChatMode::None);
			return;
		case 'l':
			if (Key_IsDown(K_CTRL)) {
				clear();
				return;
			}
			break;
		case 'c':
			if (Key_IsDown(K_CTRL)) {
				paste(vid ? vid->get_clipboard_data : nullptr);
				return;
			}
			break;
		case 'v':
			if (Key_IsDown(K_CTRL)) {
				paste(vid ? vid->get_clipboard_data : nullptr);
				return;
			}
			break;
		case 'y':
			if (Key_IsDown(K_CTRL)) {
				paste(vid ? vid->get_clipboard_data : nullptr);
				return;
			}
			break;
		case K_ENTER:
		case K_KP_ENTER:
			executePrompt();
			return;
		case K_TAB:
			Prompt_CompleteCommand(&prompt_, Key_IsDown(K_SHIFT));
			return;
		case 'r':
			if (Key_IsDown(K_CTRL)) {
				Prompt_CompleteHistory(&prompt_, false);
				return;
			}
			break;
		case 's':
			if (Key_IsDown(K_CTRL)) {
				Prompt_CompleteHistory(&prompt_, true);
				return;
			}
			break;
		case 'p':
			if (Key_IsDown(K_CTRL)) {
				Prompt_HistoryUp(&prompt_);
				if (scroll_->integer & 1)
					displayIndex_ = currentIndex_;
				return;
			}
			break;
		case 'n':
			if (Key_IsDown(K_CTRL)) {
				Prompt_HistoryDown(&prompt_);
				if (scroll_->integer & 1)
					displayIndex_ = currentIndex_;
				return;
			}
			break;
		case 'f':
			if (Key_IsDown(K_CTRL)) {
				searchDown();
				return;
			}
			break;
		case 'b':
			if (Key_IsDown(K_CTRL)) {
				searchUp();
				return;
			}
			break;
		case K_PGUP:
		case K_MWHEELUP:
			displayIndex_ -= Key_IsDown(K_CTRL) ? kScrollLargeStep : kScrollSmallStep;
			if (displayIndex_ < 0)
				displayIndex_ = 0;
			return;
		case K_PGDN:
		case K_MWHEELDOWN:
			displayIndex_ += Key_IsDown(K_CTRL) ? kScrollLargeStep : kScrollSmallStep;
			if (displayIndex_ > currentIndex_)
				displayIndex_ = currentIndex_;
			return;
		case K_HOME:
			if (Key_IsDown(K_CTRL)) {
				displayIndex_ = 0;
				return;
			}
			break;
		case K_END:
			if (Key_IsDown(K_CTRL)) {
				displayIndex_ = currentIndex_;
				return;
			}
			displayIndex_ = currentIndex_;
			return;
		default:
			break;
		}

		if (IF_KeyEvent(&prompt_.inputLine, key)) {
			Prompt_ClearState(&prompt_);
			interactiveMode();
		}

		if (scroll_->integer & 1)
			displayIndex_ = currentIndex_;
	}

	void Console::charEvent(int key)
	{
		if (IF_CharEvent(&prompt_.inputLine, key))
			interactiveMode();
	}

	void Console::messageKeyEvent(int key)
	{
		if (key == 'l' && Key_IsDown(K_CTRL)) {
			IF_Clear(&chatPrompt_.inputLine);
			return;
		}

		if (key == K_ENTER || key == K_KP_ENTER) {
			if (const char* cmd = Prompt_Action(&chatPrompt_))
				sendChat(cmd);
			Key_SetDest(Key_FromMask(cls.key_dest & ~KEY_MESSAGE));
			return;
		}

		if (key == K_ESCAPE) {
			Key_SetDest(Key_FromMask(cls.key_dest & ~KEY_MESSAGE));
			IF_Clear(&chatPrompt_.inputLine);
			return;
		}

		if (key == 'r' && Key_IsDown(K_CTRL)) {
			Prompt_CompleteHistory(&chatPrompt_, false);
			return;
		}

		if (key == 's' && Key_IsDown(K_CTRL)) {
			Prompt_CompleteHistory(&chatPrompt_, true);
			return;
		}

		if (key == K_UPARROW || (key == 'p' && Key_IsDown(K_CTRL))) {
			Prompt_HistoryUp(&chatPrompt_);
			return;
		}

		if (key == K_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL))) {
			Prompt_HistoryDown(&chatPrompt_);
			return;
		}

		if (IF_KeyEvent(&chatPrompt_.inputLine, key))
			Prompt_ClearState(&chatPrompt_);
	}

	void Console::messageCharEvent(int key)
	{
		IF_CharEvent(&chatPrompt_.inputLine, key);
	}

	void Console::messageMode(ChatMode mode)
	{
		if (cls.state != ca_active || cls.demo.playback) {
			Com_Printf("You must be in a level to chat.\n");
			return;
		}

		if (cls.key_dest & KEY_CONSOLE)
			close(true);

		chatMode_ = mode;
		IF_Replace(&chatPrompt_.inputLine, COM_StripQuotes(Cmd_RawArgs()));
		Key_SetDest(Key_FromMask(cls.key_dest | KEY_MESSAGE));
	}

	void Console::remoteMode()
	{
		if (Cmd_Argc() != 3) {
			Com_Printf("Usage: %s <address> <password>\n", Cmd_Argv(0));
			return;
		}

		netadr_t adr;
		if (!NET_StringToAdr(Cmd_Argv(1), &adr, PORT_SERVER)) {
			Com_Printf("Bad address: %s\n", Cmd_Argv(1));
			return;
		}

		if (!(cls.key_dest & KEY_CONSOLE))
			toggle(ConsoleMode::Remote, ChatMode::None);
		else {
			mode_ = ConsoleMode::Remote;
			chatMode_ = ChatMode::None;
		}

		remoteAddress_ = adr;
		remotePassword_ = Cmd_Argv(2);
	}

	void Console::dumpToFile(const char* name) const
	{
		if (!name || !*name)
			return;

                char path[MAX_OSPATH];
                qhandle_t file = FS_EasyOpenFile(path, sizeof(path), FS_MODE_WRITE | FS_FLAG_TEXT,
                        "condumps/", name, ".txt");
                if (!file)
                        return;

                char buffer[kLegacyLineWidth + 1];

                for (int i = currentIndex_ - kTotalLines + 1; i <= currentIndex_; ++i) {
			if (i < 0)
				continue;
			if (currentIndex_ - i >= kTotalLines)
				continue;

			const ConsoleLine& line = lineForRow(i);
			std::string combined = line.combined();
			int len = std::min(static_cast<int>(combined.size()), kLegacyLineWidth);

			for (int j = 0; j < len; ++j)
				buffer[j] = Q_charascii(combined[j]);

			buffer[len++] = '\n';

                        const int written = FS_Write(buffer, static_cast<size_t>(len), file);
                        if (written != len) {
                                Com_EPrintf("Error writing %s\n", path);
                                FS_CloseFile(file);
                                return;
                        }
                }

                const int closeResult = FS_CloseFile(file);
                if (closeResult)
                        Com_EPrintf("Error writing %s\n", path);
                else
                        Com_Printf("Dumped console text to %s.\n", path);
        }

	void Console::refreshTimestampColor()
	{
		if (!SCR_ParseColor(timestampsColor_->string, &timestampColor_)) {
			Com_WPrintf("Invalid value '%s' for '%s'\n", timestampsColor_->string, timestampsColor_->name);
			Cvar_Reset(timestampsColor_);
			timestampColor_ = ColorRGB(170, 170, 170);
		}
	}

} // namespace

// Public API wrappers -------------------------------------------------------

void Con_Init(void)
{
	Console::instance().init();
}

void Con_PostInit(void)
{
	Console::instance().postInit();
}

void Con_Shutdown(void)
{
	Console::instance().shutdown();
}

void Con_SetColor(color_index_t color)
{
	Console::instance().setColor(color);
}

void CL_LoadState(load_state_t state)
{
	Console::instance().loadState(state);
}

void Con_Print(const char* text)
{
	Console::instance().print(text ? text : "");
}

void Con_Printf(const char* fmt, ...)
{
	char buffer[MAXPRINTMSG];
	va_list args;
	va_start(args, fmt);
	Q_vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	Con_Print(buffer);
}

void Con_DrawConsole(void)
{
	Console::instance().draw();
}

void Con_RunConsole(void)
{
	Console::instance().run();
}

void Con_RegisterMedia(void)
{
	Console::instance().registerMedia();
}

void Con_CheckResize(void)
{
	Console::instance().checkResize();
}

void Con_ClearNotify_f(void)
{
	Console::instance().clearNotify();
}

void Con_ClearTyping(void)
{
	Console::instance().clearTyping();
}

void Con_Close(bool force)
{
	Console::instance().close(force);
}

void Con_Popup(bool force)
{
	Console::instance().popup(force);
}

void Con_SkipNotify(bool skip)
{
	Console::instance().skipNotify(skip);
}

void Con_Clear_f(void)
{
	Console::instance().clear();
}

void Con_Dump_f(void)
{
	const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
	if (!name)
		name = "condump.txt";
	Console::instance().dumpToFile(name);
}

void Con_MessageMode_f(void)
{
	Console::instance().messageMode(ChatMode::Default);
}

void Con_MessageMode2_f(void)
{
	Console::instance().messageMode(ChatMode::Team);
}

void Con_RemoteMode_f(void)
{
	Console::instance().remoteMode();
}

void Con_ToggleConsole_f(void)
{
	Console::instance().toggle(ConsoleMode::Default, ChatMode::None);
}

void Key_Console(int key)
{
	Console::instance().keyEvent(key);
}

void Char_Console(int key)
{
	Console::instance().charEvent(key);
}

void Key_Message(int key)
{
	Console::instance().messageKeyEvent(key);
}

void Char_Message(int key)
{
	Console::instance().messageCharEvent(key);
}

namespace {

	static void Con_Dump_c(genctx_t* ctx, int argnum)
	{
		if (argnum == 1)
			FS_File_g("condumps", ".txt", FS_SEARCH_STRIPEXT, ctx);
	}

	static void CL_RemoteMode_c(genctx_t* ctx, int argnum)
	{
		(void)ctx;
		(void)argnum;
	}

} // namespace

