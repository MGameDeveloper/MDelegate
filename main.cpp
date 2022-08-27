
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wingdi.h>
#include <gl/GL.h>
#include <dwmapi.h>
#include <thread>
#include <stdlib.h>

#include <stdio.h>
#include <ctime>
#include <time.h>
#include <cstdlib>
#include <timeapi.h>

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Opengl32.lib")
#pragma comment(lib, "Dwmapi.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Swap(int& v1, int& v2) { int Temp = v1; v1 = v2; v2 = Temp; }

struct agWindowBackground
{
	BITMAPINFO    mBitmapInfo;
	HDC           mBitmapDC   = nullptr;
	HBITMAP       mBitmap     = nullptr;
	HGDIOBJ       mOldBitmap  = nullptr;
	unsigned int* mPixels     = nullptr;
	unsigned int  mWidth      = 0;
	unsigned int  mHeight     = 0;
	unsigned int  mPixelCount = 0;
};

typedef struct agWindow_struct
{
	const wchar_t* Class_Name = nullptr;
	const wchar_t* Title = nullptr;
	unsigned int   Width = 0;
	unsigned int   Height = 0;
} agWindow;

typedef struct agCanvas_struct
{
	unsigned int* Data        = nullptr;
	unsigned int  Width       = 0;
	unsigned int  Height      = 0;
	unsigned int  TotalPixels = 0;
	unsigned int  TextureId   = 0;
} agCanvas;

typedef struct agApp_struct
{
	HINSTANCE  Instance      = nullptr;
	HWND       Handle        = nullptr;
	HDC        DeviceContext = nullptr;
	HGLRC      RenderContext = nullptr;
	agWindow   Window;
	agCanvas   DrawCanvas;
	bool       bClose = false;


	struct
	{
		float x = 0;
		float y = 0;
		float w = 0;
		float h = 0;
	} ViewRect;
	
} agApp;

agApp* G_App = nullptr;


agApp* CreateApp(const wchar_t* InClassName, 
	const wchar_t* InWindowTitle, 
	unsigned int InWindowWidth, 
	unsigned int InWindowHeight,
	unsigned int InTargetWidth,
	unsigned int InTargetHeight)
{
	agApp *App = new agApp;

	WNDCLASS wc = { };
	wc.lpfnWndProc   = WindowProc;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance     = GetModuleHandle(nullptr);
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpszClassName = InClassName;
	wc.hbrBackground = (HBRUSH) ::GetStockObject(NULL_BRUSH);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
		InClassName,
		InWindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, InWindowWidth, InWindowHeight,
		NULL,
		NULL,
		GetModuleHandle(nullptr),
		App
	);

	if (!hwnd)
	{
		printf("Failed to creeate App window.\n");

		delete App;
		return nullptr;
	}

	ShowWindow(hwnd, 1);
	UpdateWindow(hwnd);


	const unsigned int W = InTargetWidth;
	const unsigned int H = InTargetHeight;

	App->DeviceContext          = GetDC(hwnd);
	App->Handle                 = hwnd;
	App->Instance               = GetModuleHandle(nullptr);
	App->Window.Class_Name      = InClassName;
	App->Window.Title           = InWindowTitle;
	App->Window.Width           = InWindowWidth;
	App->Window.Height          = InWindowHeight;
	App->DrawCanvas.Data        = new unsigned int[W * H];
	App->DrawCanvas.Width       = W;
	App->DrawCanvas.Height      = H;
	App->DrawCanvas.TotalPixels = W * H;
	memset(App->DrawCanvas.Data, 0x00000000, sizeof(unsigned int) * App->DrawCanvas.TotalPixels);

	return App;
}

void CreateGraphic()
{
	// Set Pixel Format for OpenGL to be used
	PIXELFORMATDESCRIPTOR pfd;
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.cColorBits = 32;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pfn = ChoosePixelFormat(G_App->DeviceContext, &pfd);
	if (pfn == 0)
	{
		printf("Couldn't find pixel format that matches your specifications.\n");

		delete G_App;
		return;
	}
	SetPixelFormat(G_App->DeviceContext, pfn, &pfd);


	// Create OpenGL context.
	G_App->RenderContext = wglCreateContext(G_App->DeviceContext);
	if (!G_App->RenderContext)
	{
		LPSTR MsgPtr = nullptr;
		DWORD ErrCode = GetLastError();
		unsigned int TextSize = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, ErrCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&MsgPtr,
			0,
			NULL);

		char ErrorMsg[512];
		memset(ErrorMsg, 0, 512);
		memcpy(ErrorMsg, MsgPtr, TextSize);

		printf("Couldn't create OpenGL context.\n");
		printf("Error: %s\n", ErrorMsg);

		LocalFree(MsgPtr);

		return;
	}
	wglMakeCurrent(G_App->DeviceContext, G_App->RenderContext);

	glEnable(GL_TEXTURE_2D); // Turn on texturing
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	//Create Texture On Gpu that Corresponds to DrawCanvas Data format;
	glGenTextures(1, &G_App->DrawCanvas.TextureId);
	glBindTexture(GL_TEXTURE_2D, G_App->DrawCanvas.TextureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// Upload canvas to data to the texture buffer
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, G_App->DrawCanvas.Width, G_App->DrawCanvas.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, G_App->DrawCanvas.Data);


}

void RetainDrawCanvasAspectRatio()
{
	// ri: Ratio of Image
	// wi: Width of Image
	// hi: Height of Image
	float ri, wi, hi;

	// ri: Ratio of Window
    // wi: Width of Window
    // hi: Height of Window
	float rs, ws, hs;

	// wnew: Width result of Viewport
	// hnew: Height result of Viewport
	float wnew, hnew;

	wi = float(G_App->DrawCanvas.Width);
	hi = float(G_App->DrawCanvas.Height);
	ri = wi / hi;
	
	ws = float(G_App->Window.Width);
	hs = float(G_App->Window.Height);
	rs = ws / hs;

	if (rs > ri)
	{
		wnew = G_App->ViewRect.w = wi * (hs/hi);
		hnew = G_App->ViewRect.h = hs;
	}
	else
	{
		wnew = G_App->ViewRect.w = ws;
		hnew = G_App->ViewRect.h = hi * (ws/wi);
	}

	// For Centering the Viewport in the window.
	G_App->ViewRect.x = (ws - wnew) * 0.5f;
	G_App->ViewRect.y = (hs - hnew) * 0.5f;
}

void ClearFrame(unsigned char InRed = 0, unsigned char InGreen = 0, unsigned char InBlue = 0, unsigned char InAlpha = 0)
{
	unsigned int Clear_COLOR = (InAlpha << 24) | (InRed << 16) | (InGreen << 8) | (InBlue << 0);
	for (int y = 0; y < G_App->DrawCanvas.Height; ++y)
	{
		for (int x = 0; x < G_App->DrawCanvas.Width; ++x)
		{
			G_App->DrawCanvas.Data[y * G_App->DrawCanvas.Width + x] = Clear_COLOR;
		}
	}

}

void PresentFrame()
{
	// Upload canvas to data to the texture buffer
	glBindTexture(GL_TEXTURE_2D, G_App->DrawCanvas.TextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, G_App->DrawCanvas.Width, G_App->DrawCanvas.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, G_App->DrawCanvas.Data);


	RetainDrawCanvasAspectRatio();
	glViewport(G_App->ViewRect.x, G_App->ViewRect.y, G_App->ViewRect.w, G_App->ViewRect.h);
	glClearColor(0.2f, 0.f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// BEGIN:: Draw Command
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-1.f, -1.f, 0.0f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-1.f, 1.f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(1.f, 1.f, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(1.f, -1.f, 0.0f);

	glEnd();

	SwapBuffers(G_App->DeviceContext);
	DwmFlush();
	// END:: Draw Command
}




// Draw Functions
#define COLOR(R, G, B, A) ((A << 24)|(B << 16)|(G << 8)|(R << 0))
enum EColor : unsigned int
{
	EBlack   = COLOR(0, 0, 0, 255, 255),
	EWhite   = COLOR(255, 255, 255, 255),
	ERed 	 = COLOR(255, 0, 0, 255),
	ELime 	 = COLOR(0, 255, 0, 255),
	EBlue 	 = COLOR(0, 0, 255, 255),
	EYellow  = COLOR(255, 255, 0, 255),
	ECyan    = COLOR(0, 255, 255, 255),
	EMagenta = COLOR(255, 0, 255, 255),
	ESilver  = COLOR(192, 192, 192, 255),
	EGray 	 = COLOR(128, 128, 128, 255),
	EMaroon  = COLOR(128, 0, 0, 255),
	EOlive 	 = COLOR(128, 128, 0, 255),
	EGreen 	 = COLOR(0, 128, 0, 255),
	EPurple  = COLOR(128, 0, 128, 255),
	ETeal 	 = COLOR(0, 128, 128, 255),
	ENavy 	 = COLOR(0, 0, 128, 255),
};

void SetPixel(int InX, int InY, unsigned int InCOLOR)
{
	int PixelIndex = InY * G_App->DrawCanvas.Width + InX;

	if (PixelIndex >= G_App->DrawCanvas.TotalPixels || PixelIndex < 0) return;

	G_App->DrawCanvas.Data[PixelIndex] = InCOLOR;
}

void DrawLine(int InX, int InY, int InX2, int InY2, unsigned int InColor)
{
	// Bresenham's Line Drawing Algorithm : Uses Integer Calculation ^_^.

	int X               = 0;
	int Y               = 0;
	int X2              = 0;
	int Y2              = 0;
	int DeltaX          = InX2 - InX;
	int DeltaY          = InY2 - InY;
	int AbsDeltaX       = abs(DeltaX);
	int AbsDeltaY       = abs(DeltaY);
	int P               = 0;

	if (AbsDeltaX >= AbsDeltaY)
		P = 2 * AbsDeltaY - AbsDeltaX;
	else
		P = 2 * AbsDeltaX - AbsDeltaY;

	if (AbsDeltaX > AbsDeltaY)
	{
		// Swap Line's Points if Second Point's X is Less Than First Point's X
		if (DeltaX < 0)
		{
			X  = InX2;
			Y  = InY2;
			X2 = InX;
		}
		else
		{
			X  = InX;
			X2 = InX2;
			Y  = InY;
		}

		SetPixel(X, Y, InColor);
		while (X < X2)
		{
			X++;

			if (P < 0)
			{
				P = P + 2 * AbsDeltaY;
			}
			else
			{
				if (DeltaX < 0 && DeltaY > 0 || DeltaY < 0 && DeltaX > 0)
					Y--;
				else
					Y++;

				P = P + 2 * (AbsDeltaY - AbsDeltaX);
			}

			SetPixel(X, Y, InColor);
		}
	}
	else
	{
		// Swap Line's Points if Second Point's Y is Less Than First Point's Y
		if (DeltaY < 0)
		{
			Y  = InY2;
			Y2 = InY;
			X  = InX2;
		}
		else
		{
			Y  = InY;
			Y2 = InY2;
			X  = InX;
		}

		SetPixel(X, Y, InColor);
		while (Y < Y2)
		{
			Y++;

			if (P < 0)
			{
				P = P + 2 * AbsDeltaX;
			}
			else
			{
				if (DeltaX < 0 && DeltaY > 0 || DeltaX > 0 && DeltaY < 0)
					X--;
				else
					X++;

				P = P + 2 * (AbsDeltaX - AbsDeltaY);
			}

			SetPixel(X, Y, InColor);
		}
	}
}
void DrawCircle(int InX, int InY, int InRadius, unsigned int InColor)
{
	int X = 0;
	int Y = InRadius;
	int P = 3 - 2 * InRadius;

	static auto SetCirclePixel = [](int X, int Y, int X2, int Y2, unsigned int InColor)->void
	{
		// Put Current Pixel Using Circle Symmetery Property

		SetPixel(X + X2, Y - Y2, InColor); // 1 Octant 
		SetPixel(X + X2, Y + Y2, InColor); // 4 Octant
		SetPixel(X - X2, Y + Y2, InColor); // 8 Octant
		SetPixel(X - X2, Y - Y2, InColor); // 5 Octant
								
		SetPixel(X + Y2, Y - X2, InColor); // 2 Octant
		SetPixel(X + Y2, Y + X2, InColor); // 3 Octant
		SetPixel(X - Y2, Y + X2, InColor); // 6 Octant
		SetPixel(X - Y2, Y - X2, InColor); // 7 Octant
	};

	while (X <= Y)
	{
		SetCirclePixel(InX, InY, X, Y, InColor);
	
		if (P < 0)
		{
			P += 4 * X + 6;
		}
		else
		{
			Y--;
			P += 4 * (X - Y) + 10;
		}
		X++;
	}
}
void DrawFillCircle(int InX, int InY, int InRadius, unsigned int InColor)
{
	int X = 0;
	int Y = InRadius;
	int P = 3 - 2 * InRadius;

	auto DrawCircleLine = [&](int BeginX, int EndX, int LineY, unsigned int InColor)
	{
		for (int i = BeginX; i <= EndX; ++i)
		{
			SetPixel(i, LineY, InColor);
		}
	};


	while (X <= Y)
	{
		DrawCircleLine(InX - Y, InX + Y, InY + X, InColor);
		DrawCircleLine(InX - Y, InX + Y, InY - X, InColor);
		DrawCircleLine(InX - X, InX + X, InY + Y, InColor);
		DrawCircleLine(InX - X, InX + X, InY - Y, InColor);

		if (P < 0)
		{
			P += 4 * X + 6;
		}
		else
		{
			P += 4 * (X - Y) + 10;
			Y--;
		}

		X++;
	}
}
void DrawRect(int InX, int InY, int InWidth, int InHeight, unsigned int InColor)
{
	DrawLine(InX, InY, InX + InWidth, InY, InColor);                       // Top Line
	DrawLine(InX, InY + InHeight, InX + InWidth, InY + InHeight, InColor); // Bottom Line
	
	DrawLine(InX + InWidth, InY, InX + InWidth, InY + InHeight, InColor); // Right Line
	DrawLine(InX, InY, InX, InY + InHeight, InColor);                     // Left Line
}
void DrawFillRect(int InX, int InY, int InWidth, int InHeight, unsigned int InColor)
{
	int EndX = InX + InWidth;
	int EndY = InY + InHeight;

	for (int y = InY; y <= EndY; ++y)
		for (int x = InX; x <= EndX; ++x)
			SetPixel(x, y, InColor);
}
void DrawTriangle(int InX, int InY, int InX2, int InY2, int InX3, int InY3, unsigned int InColor)
{
	DrawLine(InX,  InY,  InX2, InY2, InColor);
	DrawLine(InX2, InY2, InX3, InY3, InColor);
	DrawLine(InX3, InY3, InX,  InY, InColor);
}
void DrawFillTriangle(int InX, int InY, int InX1, int InY1, int InX2, int InY2, unsigned int InColor)
{	
	if (InY  > InY1) { Swap(InY, InY1);  Swap(InX, InX1);  }
	if (InY  > InY2) { Swap(InY, InY2);  Swap(InX, InX2);  }
	if (InY1 > InY2) { Swap(InY1, InY2); Swap(InX1, InX2); }

	int x1, y1, xe1, ye1, dx1, dy1, abdx1, abdy1, p1, xlen1, ylen1, xinc1 = 1, yinc1 = 1;
	int x2, y2, xe2, ye2, dx2, dy2, abdx2, abdy2, p2, xlen2, ylen2, xinc2 = 1, yinc2 = 1;

	auto DrawTriLine   = [](int xinc, int yinc, int& x, int& y, int& xe, int& ye, int dx, int dy, int abdx, int abdy, int& p/*, unsigned int COLOR*/)
	{
		int xlen = abs(xe - x);
		int ylen = abs(ye - y);

		if (abdx >= abdy)
		{
			for (int i = 0; i < xlen; ++i)
			{
				//SetPixel(x, y, COLOR);
				x += xinc;
				if (p < 0)
					p += 2 * abdy;
				else
				{
					if (dy < 0)
						y--;
					else
						y++;
					p += 2 * (abdy - abdx);
					return;
				}
			}
		}
		else
		{
			for (int i = 0; i < ylen; ++i)
			{
				//SetPixel(x, y, COLOR);
				y += yinc;
				if (p < 0)
					p += 2 * abdx;
				else
				{
					if (dx < 0)
						x--;
					else
						x++;
					p += 2 * (abdx - abdy);
				}
				return;
			}
		}
	};
	auto ScanLineTri   = [](int x, int xe, int y, int InColor)
	{
		for (int i = x; i < xe; ++i)
			SetPixel(i, y, InColor);
	};
	auto InitTriLine_1 = [&](int x, int y, int xe, int ye)
	{
		x1 = x; y1 = y; xe1 = xe; ye1 = ye; dx1 = xe1 - x1; dy1 = ye1 - y1; abdx1 = abs(dx1); abdy1 = abs(dy1), xlen1 = abdx1, ylen1 = abdy1; xinc1 = 1; yinc1 = 1;
		if (abdx1 >= abdy1)
		{
			p1 = 2 * (abdy1 - abdx1);
			if (dx1 < 0)
				xinc1 *= -1;

		}
		else
		{
			p1 = 2 * (abdx1 - abdy1);
			if (dy1 < 0)
				yinc1 *= -1;
		}
	};
	auto InitTriLine_2 = [&](int x, int y, int xe, int ye)
	{
		x2 = x; y2 = y; xe2 = xe; ye2 = ye; dx2 = xe2 - x2; dy2 = ye2 - y2; abdx2 = abs(dx2); abdy2 = abs(dy2), xlen2 = abdx2, ylen2 = abdy2; xinc2 = 1; yinc2 = 1;
		if (abdx2 >= abdy2)
		{
			p2 = 2 * (abdy2 - abdx2);
			if (dx2 < 0)
				xinc2 *= -1;

		}
		else
		{
			p2 = 2 * (abdx2 - abdy2);
			if (dy2 < 0)
				yinc2 *= -1;
		}
	};

	int ScanLineNum = 0;
	if (InY == InY1) // Top Flat
	{
		InitTriLine_1(InX2, InY2, min(InX, InX1), InY);
		InitTriLine_2(InX2, InY2, max(InX, InX1), InY1);

		ScanLineNum = InY2 - InY1;
		for (int i = 0; i < ScanLineNum; ++i)
		{
			DrawTriLine(xinc1, yinc1, x1, y1, xe1, ye1, dx1, dy1, abdx1, abdy1, p1 /*, RED*/  );
			DrawTriLine(xinc2, yinc2, x2, y2, xe2, ye2, dx2, dy2, abdx2, abdy2, p2 /*, WHITE*/);

			ScanLineTri(x1, x2, y1, InColor);
		}
	}
	else if (InY1 == InY2) // Bottom Flat
	{
		InitTriLine_1(InX, InY, min(InX1, InX2), InY1);
		InitTriLine_2(InX, InY, max(InX1, InX2), InY2);

		ScanLineNum = InY2 - InY;
		for (int i = 0; i < ScanLineNum; ++i)
		{
			DrawTriLine(xinc1, yinc1, x1, y1, xe1, ye1, dx1, dy1, abdx1, abdy1, p1 /*, RED*/  );
			DrawTriLine(xinc2, yinc2, x2, y2, xe2, ye2, dx2, dy2, abdx2, abdy2, p2 /*, WHITE*/);

			ScanLineTri(x1, x2, y1, InColor);
		}
	}
	else  // General Triangle
	{
		// First Triangle
		InitTriLine_1(InX, InY, InX1, InY1);
		InitTriLine_2(InX, InY, InX2, InY2);

		ScanLineNum = InY1 - InY;
		for (int i = 0; i < ScanLineNum; ++i)
		{
			DrawTriLine(xinc1, yinc1, x1, y1, xe1, ye1, dx1, dy1, abdx1, abdy1, p1 /*, RED*/  );
			DrawTriLine(xinc2, yinc2, x2, y2, xe2, ye2, dx2, dy2, abdx2, abdy2, p2 /*, WHITE*/);

			if (x1 > x2)
				ScanLineTri(x2, x1, y1, InColor);
			else
				ScanLineTri(x1, x2, y1, InColor);
		}

		// Second Triangle
		InitTriLine_1(InX1, InY1, InX2, InY2);
		ScanLineNum = InY2 - InY1;
		for (int i = 0; i < ScanLineNum; ++i)
		{
			DrawTriLine(xinc1, yinc1, x1, y1, xe1, ye1, dx1, dy1, abdx1, abdy1, p1 /*, RED*/  );
			DrawTriLine(xinc2, yinc2, x2, y2, xe2, ye2, dx2, dy2, abdx2, abdy2, p2 /*, WHITE*/);

			if (x1 > x2)
				ScanLineTri(x2, x1, y1, InColor);
			else
				ScanLineTri(x1, x2, y1, InColor);
		}
	}
}

// A Fast Bresenham Type Algorithm For Drawing Ellipses. By John Kennedy. Santa Monica College
// http://160592857366.free.fr/joe/ebooks/ShareData/A%20Fast%20Bresenham%20Type%20Algorithm%20for%20Drawing%20Ellipses.pdf
void DrawEllipse(int InX, int InY, int InXRadius, int InYRadius, unsigned int InColor)
{
	int _2xr         = 2 * InXRadius * InXRadius;
	int _2yr         = 2 * InYRadius * InYRadius;
	int X            = InXRadius;
	int Y            = 0;
	int XChange      = InYRadius * InYRadius * (1 - 2 * InXRadius);
	int YChange      = InXRadius * InXRadius;
	int EllipseError = 0;
	int StoppingX    = _2yr * InXRadius;
	int StoppingY    = 0;

	auto Plot4EllipsePoints = [&](int x, int y)
	{
		SetPixel(InX + X, InY + Y, InColor);
		SetPixel(InX - X, InY + Y, InColor);
		SetPixel(InX - X, InY - Y, InColor);
		SetPixel(InX + X, InY - Y, InColor);
	};

	// First Points
	while (StoppingX >= StoppingY)
	{
		Plot4EllipsePoints(X, Y);

		Y++;
		StoppingY    += _2xr;
		EllipseError += YChange;
		YChange      += _2xr;

		if ((2 * EllipseError + XChange) > 0)
		{
			X--;
			StoppingX    -= _2yr;
			EllipseError += XChange;
			XChange      += _2yr;
		}
	}

	// Second Points
	X            = 0;
	Y            = InYRadius;
	XChange      = InYRadius * InYRadius;
	YChange      = InXRadius * InXRadius * (1 - 2 * InYRadius);
	EllipseError = 0;
	StoppingX    = 0;
	StoppingY    = _2xr * InYRadius;

	while (StoppingX <= StoppingY)
	{
		Plot4EllipsePoints(X, Y);
		
		X++;
		StoppingX += _2yr;
		EllipseError += XChange;
		XChange += _2yr;

		if ((2 * EllipseError + YChange) > 0)
		{
			Y--;
			StoppingY    -= _2xr;
			EllipseError += YChange;
			YChange      += _2xr;
		}
	}
}
void DrawFillEllipse(int InX, int InY, int InXRadius, int InYRadius, unsigned int InColor)
{
	int _2xr = 2 * InXRadius * InXRadius;
	int _2yr = 2 * InYRadius * InYRadius;
	int X = InXRadius;
	int Y = 0;
	int XChange = InYRadius * InYRadius * (1 - 2 * InXRadius);
	int YChange = InXRadius * InXRadius;
	int EllipseError = 0;
	int StoppingX = _2yr * InXRadius;
	int StoppingY = 0;

	auto DrawEllipseLine = [&](int BeginX, int EndX, int LineY, unsigned int Color)
	{
		for (int i = BeginX; i < EndX; ++i)
		{
			SetPixel(i, LineY, Color);
		}
	};

	// First Points
	while (StoppingX >= StoppingY)
	{
		DrawEllipseLine(InX - X, InX + X, InY + Y, InColor);
		DrawEllipseLine(InX - X, InX + X, InY - Y, InColor);

		Y++;
		StoppingY += _2xr;
		EllipseError += YChange;
		YChange += _2xr;

		if ((2 * EllipseError + XChange) > 0)
		{
			X--;
			StoppingX -= _2yr;
			EllipseError += XChange;
			XChange += _2yr;
		}
	}

	// Second Points
	X = 0;
	Y = InYRadius;
	XChange = InYRadius * InYRadius;
	YChange = InXRadius * InXRadius * (1 - 2 * InYRadius);
	EllipseError = 0;
	StoppingX = 0;
	StoppingY = _2xr * InYRadius;

	while (StoppingX <= StoppingY)
	{
		DrawEllipseLine(InX - X, InX + X, InY + Y, InColor);
		DrawEllipseLine(InX - X, InX + X, InY - Y, InColor);

		X++;
		StoppingX += _2yr;
		EllipseError += XChange;
		XChange += _2yr;

		if ((2 * EllipseError + YChange) > 0)
		{
			Y--;
			StoppingY -= _2xr;
			EllipseError += YChange;
			YChange += _2xr;
		}
	}
}


int CenterX = 100;
int CenterY = 100;
int TargetX = 0;
int TargetY = 0;
int Y_Var   = 5;
int X_Var   = 10;
int Boundry = 11;

struct v2
{
	int x = 0;
	int y = 0;
};

v2 CubicCurve(v2 p0, v2 p1, v2 p2, v2 p3, float t)
{
	auto Lerp = [](v2 p0, v2 p1, float t) -> v2
	{
		v2 r =
		{
			p0.x + (p1.x - p0.x) * t,
			p0.y + (p1.y - p0.y) * t
		};

		return r;
	};

	auto QuadraticCurve = [&Lerp](v2 p0, v2 p1, v2 p2, float t) -> v2
	{
		v2 FirstPoint  = Lerp(p0, p1, t);
		v2 SecontPoint = Lerp(p1, p2, t);
		v2 ResultPoint = Lerp(FirstPoint, SecontPoint, t);

		return ResultPoint;
	};

	v2 FirstPoint  = QuadraticCurve(p0, p1, p2, t);
	v2 SecondPoint = QuadraticCurve(p1, p2, p3, t);

	return Lerp(FirstPoint, SecondPoint, t);
}


v2 Javidx9Curve(v2* p, unsigned int pn, float t)
{
	int p0, p1, p2, p3;
	p1 = (int)t + 1;
	p2 = p1 + 1;
	p3 = p2 + 1;
	p0 = p1 - 1;

	float tt = t * t;
	float ttt = tt * t;

	float q1 = -ttt + 2.0f * tt - t;
	float q2 = 3.0f * ttt - 5.0f * tt + 2.0f;
	float q3 = -3.0f * ttt + 4.0f * tt + t;
	float q4 = ttt - tt;

	float tx = 0.5 * (p[p0].x * q1 + p[p1].x * q2 + p[p2].x * q3 + p[p3].x * q4);
	float ty = 0.5 * (p[p0].y * q1 + p[p1].y * q2 + p[p2].y * q3 + p[p3].y * q4);
	
	v2 r;
	r.x = tx;
	r.y = ty;

	return r;
}


void UpdateFrame()
{
	//static float radius = 50;
	//static float CenterX = 115;
	//static float CenterY = 110;
	//static float CircleX = 0;
	//static float CircleY = 0;
	//static float ElapsedTime = 0.0f;
	//static float Length = 70;
	//
	//static uint32_t RandCOLOR = ECyan;
	//ElapsedTime += 0.03f;
	//
	//CircleX = CenterX + (radius * cos(ElapsedTime));
	//CircleY = CenterY + (radius * sin(ElapsedTime));
	//
	//DrawFillRect(25, 25, 210, 210, ENavy);
	//
	//DrawLine(25,  25,  CircleX, CircleY, ERed);
	//DrawLine(210, 25,  CircleX, CircleY, EBlue);
	//DrawLine(210, 210, CircleX, CircleY, EGreen);
	//DrawLine(25,  210, CircleX, CircleY, EMagenta);
	//
	//DrawFillCircle(25,  25,  10, COLOR(0,   255, 100, 255));
	//DrawFillCircle(210, 25,  10, COLOR(0,   255, 255, 255));
	//DrawFillCircle(210, 210, 10, COLOR(100, 255, 0,   255));
	//DrawFillCircle(25,  210, 10, COLOR(255, 255, 0,   255));
	//
	//DrawCircle(25,  25,  5, COLOR(40,  200, 50,  255));
	//DrawCircle(210, 25,  5, COLOR(80,  110, 200, 255));
	//DrawCircle(210, 210, 5, COLOR(50,  100, 100, 255));
	//DrawCircle(25,  210, 5, COLOR(100, 50,  255, 255));
	//
	//DrawFillCircle(CircleX, CircleY, 15, COLOR(255, 0, 255, 255));
	//DrawCircle(CircleX, CircleY, 20, COLOR(255, 0, 0, 255));
	//DrawTriangle(CircleX , CircleY - 20, CircleX + 15, CircleY + 15, CircleX - 15, CircleY + 15, COLOR(40, 200, 200, 255));
	//DrawRect(CircleX - 5, CircleY - 5, 10, 10, COLOR(0, 0, 0, 0));
	//
	//
	//DrawFillTriangle(CircleX, CircleY, 150, 150, 95, 150, EColor::EMaroon);
	//DrawFillTriangle(50, 50, CircleX, CircleY, 10, 60, ECyan);
	//DrawFillTriangle(CircleX, CircleY, 100, 100, 40, 100, EYellow);
	//DrawFillTriangle(200, 200, 256, 170, CircleX, CircleY, EBlue);
	//
	//DrawTriangle(110, 110, 150, 150, 95, 150, EColor::ETeal);
	//DrawTriangle(20, 30, 100, 100, 40, 100, EColor::EOlive);
	//DrawTriangle(50, 50, 100, 90, 10, 60, EColor::ELime);
	//DrawTriangle(200, 200, 256, 170, CircleX, CircleY, EColor::EGray);
	//
	//DrawFillEllipse(CircleX, CircleY, 45, 15, EColor::EGray);
	//DrawFillEllipse(CircleX, CircleY, 15, 45, EColor::EOlive);
	//DrawEllipse(100, 100, 50, 20, EColor::EWhite);
	//DrawEllipse(180, 180, 10, 50, EColor::EMaroon);
	//
	//SetPixel(CircleX, CircleY, COLOR(255, 255, 0, 255));

	//static v2 p0 = { 0,   120};
	//static v2 p1 = { 100,  0 };
	//static v2 p2 = { 150, 200 };
	//static v2 p3 = { 256, 0 };
	//
	//float tf = 1.f * 0.016666f;
	//static float t = tf;
	//
	//while (t <= 1.0f)
	//{
	//	DrawFillCircle(p0.x, p0.y, 2, EColor::EBlack);
	//	DrawFillCircle(p1.x, p1.y, 2, EColor::EBlack);
	//	DrawFillCircle(p2.x, p2.y, 2, EColor::EBlack);
	//	DrawFillCircle(p3.x, p3.y, 2, EColor::EBlack);
	//
	//	v2 r = CubicCurve(p0, p1, p2, p3, t);
	//	DrawFillCircle(r.x, r.y, 2, EColor::EBlue);
	//
	//	t += tf;
	//}
	//t = 0;
	//
	//
	//DrawFillRect(200, 200, 8, 8, EColor::ELime);
	//DrawRect(200, 200, 8, 8, EColor::ENavy);

	v2 p[4] = 
	{
		{100, 100},
		{150, 50},
		{150, 150},
		{200, 100 }
	};


	float t = 0.0f;
	while (t < 1.0f)
	{
		t += 0.005f;
		v2 r1 = Javidx9Curve(p, 4, t);

		t += 0.005f;
		v2 r2 = Javidx9Curve(p, 4, t);

		DrawLine(r1.x, r1.y, r2.x, r2.y, EGreen);
	}

	float tf = 0.005f;
	t = 0.0f;
	while (t < 1.0f)
	{
		t += tf;
		v2 r1 = CubicCurve(p[0], p[1], p[2], p[3], t);
		SetPixel(r1.x, r1.y, EColor::ECyan);

		t += tf;
		v2 r2 = CubicCurve(p[0], p[1], p[2], p[3], t);

		DrawLine(r1.x, r1.y, r2.x, r2.y, EColor::EPurple);
	}
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void StartApp()
{
	RetainDrawCanvasAspectRatio();
	CreateGraphic();

	while(!G_App->bClose)
	{ 
		ClearFrame(0, 0, 0, 255);


		UpdateFrame();

	   
		PresentFrame();
		Sleep(10);
	}

	wglDeleteContext(G_App->RenderContext);
	PostMessage(G_App->Handle, WM_DESTROY, 0, 0);
}

int main(int argc, char** argv) 
{
	srand(time(0));

	//G_App = CreateApp(L"Sample Window Class", L"AG_App", 1200, 720, 11, 11);
	G_App = CreateApp(L"Sample Window Class", L"AG_App", 1200, 720, 256, 240);
	if (!G_App)
	{
		printf("Failed to Create App\n");
		return -1;
	}

	std::thread t = std::thread(&StartApp);
	
	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	t.join();

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static int MouseDelta = 0;

	switch (uMsg)
	{
	case WM_CLOSE:
		G_App->bClose = true;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		DestroyWindow(G_App->Handle);
		return 0;

	case WM_SIZE:
		if (G_App)
		{
			G_App->Window.Width  = (unsigned int)LOWORD(lParam);
			G_App->Window.Height = (unsigned int)HIWORD(lParam);
		}
		break;

	case WM_MOUSEMOVE:
		break;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_UP:
			if (Y_Var - 1 >= 0)
				Y_Var -= 1;
			break;

		case VK_DOWN:
			if (Y_Var + 1 < Boundry)
				Y_Var += 1;
			
			break;

		case VK_LEFT:
			if (X_Var - 1 >= 0)
				X_Var -= 1;
			break;

		case VK_RIGHT:
			if (X_Var + 1 < Boundry)
				X_Var += 1;
			break;
		}
		break;

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}