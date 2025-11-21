#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

/*
==============================================================================
MenuItem

Interface describing the ownership and lifecycle expectations for UI menu
items. Texture handles and children are stored via shared ownership to keep
resources alive as long as a menu item exists. Callbacks are supplied via
std::function so callers can pass lambdas or other RAII-friendly callables
without leaking state.
==============================================================================
*/
class MenuItem {
public:
	using TextureHandle = std::shared_ptr<void>;
	using Callback = std::function<void(MenuItem &)>;

	struct MenuEvent {
		enum class Type {
			Key,
			Pointer,
			Controller
		};

		Type type{Type::Key};
		int key{0};
		int x{0};
		int y{0};
	};

	MenuItem(std::string name,
		TextureHandle texture,
		Callback onActivate,
		std::vector<std::shared_ptr<MenuItem>> children = {});

	MenuItem(const MenuItem &) = delete;
	MenuItem(MenuItem &&) = delete;
	MenuItem &operator=(const MenuItem &) = delete;
	MenuItem &operator=(MenuItem &&) = delete;

	virtual ~MenuItem();

	const std::string &GetName() const;
	const TextureHandle &GetTexture() const;
	const std::vector<std::shared_ptr<MenuItem>> &GetChildren() const;

	void SetActivateCallback(Callback callback);

protected:
	void AddChild(std::shared_ptr<MenuItem> child);
	void RemoveChild(const std::shared_ptr<MenuItem> &child);
	void ForEachChild(const std::function<void(const std::shared_ptr<MenuItem> &)> &visitor) const;
	void TriggerActivate();

	virtual void Draw() const = 0;
	virtual bool HandleEvent(const MenuEvent &event) = 0;
	virtual bool Activate() = 0;
	virtual void SetFocus(bool hasFocus) = 0;
	virtual bool HasFocus() const = 0;
	virtual void OnAttach() = 0;
	virtual void OnDetach() = 0;

private:
	std::string m_name;
	TextureHandle m_texture;
	Callback m_onActivate;
	std::vector<std::shared_ptr<MenuItem>> m_children;
};
