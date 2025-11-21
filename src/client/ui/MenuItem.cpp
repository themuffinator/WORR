#include "MenuItem.h"

#include <algorithm>
#include <utility>

/*
==============
MenuItem

Constructs a menu item with ownership-aware resources and optional children.
==============
*/
MenuItem::MenuItem(std::string name,
	TextureHandle texture,
	Callback onActivate,
	std::vector<std::shared_ptr<MenuItem>> children)
	: m_name(std::move(name)),
	m_texture(std::move(texture)),
	m_onActivate(std::move(onActivate)),
	m_children(std::move(children))
{
}

/*
==============
~MenuItem

Virtual destructor to ensure derived classes release resources cleanly.
==============
*/
MenuItem::~MenuItem() = default;

/*
==============
GetName

Returns the immutable name assigned during construction.
==============
*/
const std::string &MenuItem::GetName() const
{
	return m_name;
}

/*
==============
GetTexture

Provides read-only access to the retained texture handle. The shared pointer
keeps the underlying texture alive for the lifetime of the menu item.
==============
*/
const MenuItem::TextureHandle &MenuItem::GetTexture() const
{
	return m_texture;
}

/*
==============
GetChildren

Exposes shared ownership of child items. Holding on to the returned references
keeps children alive as long as the caller retains a shared_ptr reference.
==============
*/
const std::vector<std::shared_ptr<MenuItem>> &MenuItem::GetChildren() const
{
	return m_children;
}

/*
==============
SetActivateCallback

Updates the activation callback. The std::function allows callers to provide
RAII-friendly functors that manage captured resources safely.
==============
*/
void MenuItem::SetActivateCallback(Callback callback)
{
	m_onActivate = std::move(callback);
}

/*
==============
AddChild

Appends a child to the ownership-tracking collection. Shared ownership ensures
children remain valid independently of the menu hierarchy.
==============
*/
void MenuItem::AddChild(std::shared_ptr<MenuItem> child)
{
	m_children.emplace_back(std::move(child));
}

/*
==============
RemoveChild

Removes a matching child instance from the container while leaving other
shared references untouched.
==============
*/
void MenuItem::RemoveChild(const std::shared_ptr<MenuItem> &child)
{
	m_children.erase(
		std::remove(m_children.begin(), m_children.end(), child),
		m_children.end());
}

/*
==============
ForEachChild

Invokes a visitor for every child entry, preserving shared ownership during the
iteration.
==============
*/
void MenuItem::ForEachChild(const std::function<void(const std::shared_ptr<MenuItem> &)> &visitor) const
{
	for (const auto &child : m_children)
	{
		visitor(child);
	}
}

/*
==============
TriggerActivate

Invokes the activation callback when set. The callback owns its captured state
through std::function and follows normal RAII semantics.
==============
*/
void MenuItem::TriggerActivate()
{
	if (m_onActivate)
	{
		m_onActivate(*this);
	}
}
