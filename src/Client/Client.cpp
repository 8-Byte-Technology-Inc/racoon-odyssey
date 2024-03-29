/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/

#include "pch.h"

#include "MainClass.h"

#include "Client.h"

INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	HRESULT hr = S_OK;

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Begin initialization.
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// Instantiate the window manager class.
	std::shared_ptr<MainClass> winMain = std::shared_ptr<MainClass>(new MainClass());

	// Create a window.
	winMain->Initialize();

	// run until they want to exit.
	winMain->Run();

	// Cleanup is handled in destructors.
	return hr;
}
