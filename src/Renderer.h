#pragma once
#include <windows.h>
#include "Store.h"

namespace Renderer
{
    void Paint(HDC hdc, const RECT& rc, UINT dpi, const Store& store);
}
