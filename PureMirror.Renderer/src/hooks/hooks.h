#pragma once

namespace Hooks {
	void Init( );
	void Free( );

	inline bool bShuttingDown;

    inline int vulkanCounter;
    inline int dxd12Counter;
}

namespace H = Hooks;
