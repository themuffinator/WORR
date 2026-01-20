#include "../cg_local.h"
#include "ui_cgame_access.h"

namespace ui {
bool CgameIsInGame()
{
	return cgi.CL_FrameValid();
}

const char *CgameConfigString(int index)
{
	return cgi.get_configString(index);
}
} // namespace ui
