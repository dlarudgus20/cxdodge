#include <windows.h>
#include <math.h>
#include <list>

const float PI = 3.141592653589793238;
const LPCTSTR lpszClass = L"cxdodge";
const int WIDTH = 320, HEIGHT = 240;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

inline void DrawPointEllipse(HDC hdc, int x, int y, int radius)
{
	Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
}
template <typename T>
inline T pow2(const T &x) { return x*x; }

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdParam, int nCmdShow)
{
	WNDCLASS wc;
	HWND hWnd;
	MSG msg;

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = lpszClass;
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&wc);

	hWnd = CreateWindow(lpszClass, lpszClass,
		WS_BORDER | WS_OVERLAPPED | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
		NULL, (HMENU)NULL, hInstance, NULL);
	if (hWnd != NULL)
	{
		ShowWindow(hWnd, nCmdShow);

		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return msg.wParam;
	}
	else
	{
		return -1;
	}
}

struct Bullet
{
	float x, y;
	float dx, dy;
};

HBITMAP hBitmap = NULL;
HBRUSH hbrBullet, hbrPlayer;

bool bLose = false;
std::list<Bullet> bullets;
POINT player;

const int DRAW_RADIUS = 3;
const int HIT_RADIUS = 1;

const int PLAYER_SPEED = 1;
const int BULLET_SPEED = 1;

const int CREATION_GAP = 75;

const int MOVE_TIMER_ID = 1;
const int MOVE_TIMER_FREQ = 13;
const int CREATION_TIMER_ID = 2;
const int CREATION_TIMER_FREQ = 675;

LRESULT OnCreate(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnTimer(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnDestroy(HWND hWnd, WPARAM wParam, LPARAM lParam);

void CreateBullet(const RECT &rt, int x, int y);
bool CheckLose();

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
#define ONMSG(msg, fn) case msg: return fn(hWnd, wParam, lParam)
	switch (iMsg)
	{
		ONMSG(WM_CREATE, OnCreate);
		ONMSG(WM_PAINT, OnPaint);
		ONMSG(WM_TIMER, OnTimer);
		ONMSG(WM_DESTROY, OnDestroy);
	}
#undef ONMSG
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

LRESULT OnCreate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HBITMAP hOldBit;
	HDC hdc, hMemDC;
	RECT rt;

	hdc = GetDC(hWnd);
	hMemDC = CreateCompatibleDC(hdc);
	hBitmap = CreateCompatibleBitmap(hdc, WIDTH, HEIGHT);
	hOldBit = (HBITMAP)SelectObject(hMemDC, hBitmap);
	GetClientRect(hWnd, &rt);
	FillRect(hMemDC, &rt, (HBRUSH)GetStockObject(BLACK_BRUSH));
	SelectObject(hMemDC, hOldBit);
	DeleteDC(hMemDC);
	ReleaseDC(hWnd, hdc);

	hbrBullet = CreateSolidBrush(RGB(255, 255, 255));
	hbrPlayer = CreateSolidBrush(RGB(0, 255, 52));

	SetTimer(hWnd, MOVE_TIMER_ID, MOVE_TIMER_FREQ, NULL);
	SetTimer(hWnd, CREATION_TIMER_ID, CREATION_TIMER_FREQ, NULL);

	player.x = rt.right / 2;
	player.y = rt.bottom / 2;

	return 0;
}

LRESULT OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	BeginPaint(hWnd, &ps);

	if (hBitmap != NULL)
	{
		HBITMAP hOldBit;
		HDC hdc, hMemDC;
		RECT rt;

		hdc = GetDC(hWnd);
		hMemDC = CreateCompatibleDC(hdc);
		hBitmap = CreateCompatibleBitmap(hdc, WIDTH, HEIGHT);
		hOldBit = (HBITMAP)SelectObject(hMemDC, hBitmap);

		GetClientRect(hWnd, &rt);
		FillRect(hMemDC, &rt, (HBRUSH)GetStockObject(BLACK_BRUSH));

		HPEN hOldPen = (HPEN)SelectObject(hMemDC, GetStockObject(NULL_PEN));
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hbrPlayer);
		DrawPointEllipse(hMemDC, player.x, player.y, DRAW_RADIUS);

		SelectObject(hMemDC, hbrBullet);
		for (const Bullet &b : bullets)
			DrawPointEllipse(hMemDC, (int)b.x, (int)b.y, DRAW_RADIUS);

		SelectObject(hMemDC, hOldBrush);
		SelectObject(hMemDC, hOldPen);

		BitBlt(hdc, 0, 0, WIDTH, HEIGHT, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBit);
		DeleteDC(hMemDC);
		ReleaseDC(hWnd, hdc);
	}

	EndPaint(hWnd, &ps);
	return 0;
}

LRESULT OnTimer(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (bLose)
		return 0;

	RECT rt;
	int x, y;

	switch (wParam)
	{
		case MOVE_TIMER_ID:
			if (GetAsyncKeyState(VK_LEFT) < 0)
				player.x -= PLAYER_SPEED;
			if (GetAsyncKeyState(VK_UP) < 0)
				player.y -= PLAYER_SPEED;
			if (GetAsyncKeyState(VK_RIGHT) < 0)
				player.x += PLAYER_SPEED;
			if (GetAsyncKeyState(VK_DOWN) < 0)
				player.y += PLAYER_SPEED;

			for (Bullet &b : bullets)
			{
				b.x += b.dx;
				b.y += b.dy;
			}

			bLose = CheckLose();
			InvalidateRect(hWnd, NULL, FALSE);
			break;
		case CREATION_TIMER_ID:
			GetClientRect(hWnd, &rt);
			for (x = 0; x < rt.right; x += CREATION_GAP)
			{
				CreateBullet(rt, x, 0);
			}
			for (y = x - rt.right; y < rt.bottom; y += CREATION_GAP)
			{
				CreateBullet(rt, rt.right, y);
			}
			for (x = rt.right - (y - rt.bottom); x >= 0; x -= CREATION_GAP)
			{
				CreateBullet(rt, x, rt.bottom);
			}
			for (y = rt.right - (-x); y >= 0; y -= CREATION_GAP)
			{
				CreateBullet(rt, 0, y);
			}

			bLose = CheckLose();
			InvalidateRect(hWnd, NULL, FALSE);
			break;
	}
	return 0;
}

// x = a cost
// y = b sint
void CreateBullet(const RECT &rt, int x, int y)
{
	Bullet b;
	b.x = x;
	b.y = y;

	float d_x = player.x - x;
	float d_y = player.y - y;
	float d_length = sqrt(pow2(d_x) + pow2(d_y));

	//float a = (float)rt.bottom / rt.right;
	//float d_y0 = d_y / a;
	//float theta = fmod(atan2(d_y0, d_x), PI / 2);
	float speed = BULLET_SPEED/* * pow2(tan(-fabs(theta - PI / 4)))*/;

	b.dx = d_x / d_length * speed;
	b.dy = d_y / d_length * speed;

	bullets.push_back(b);
}

bool CheckLose()
{
	for (Bullet &b : bullets)
	{
		if (pow2((int)b.x - player.x) + pow2((int)b.y - player.y) <= pow2(HIT_RADIUS))
		{
			return true;
		}
	}
	return false;
}

LRESULT OnDestroy(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	KillTimer(hWnd, MOVE_TIMER_ID);
	KillTimer(hWnd, CREATION_TIMER_ID);

	DeleteObject(hBitmap);
	hBitmap = NULL;

	PostQuitMessage(0);
	return 0;
}
