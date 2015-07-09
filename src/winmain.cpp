#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <functional>
#include <algorithm>
#include <list>

//#define DISABLE_RESTART

const float PI = 3.141592653589793238;

const LPCTSTR lpszClass = L"cxdodge";
const LPCTSTR lpszTitle = L"cxdodge (right click or type 'q' for quit)";
const int WIDTH = 320, HEIGHT = 240;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

inline void DrawPointEllipse(HDC hdc, int x, int y, int radius)
{
	Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
}
template <typename T>
inline T pow2(const T &x) { return x*x; }

inline int range(int min, int value, int max) { return (min < value) ? (value < max ? value : max) : min; }

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

	hWnd = CreateWindow(lpszClass, lpszTitle,
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
	bool bHit;
};

HBITMAP hBitmap = NULL;
HBRUSH hbrBullet, hbrBulletHit;
HBRUSH hbrPlayer;

DWORD StartTick, GameTime;
bool bLose;
std::list<Bullet> bullets;
POINT player;

const int DRAW_RADIUS = 3;
const int HIT_RADIUS = 2;
const int MOVE_LIMIT = 2;

const int PLAYER_SPEED = 1;
const int BULLET_FULL_ARRIVAL_TIME = 150;

const int CREATION_GAP = 95;

const int MOVE_TIMER_ID = 1;
const int MOVE_TIMER_FREQ = 13;
const int CREATION_TIMER_ID = 2;
const int CREATION_TIMER_FREQ = 675;

const UINT WM_MY_TRYSAVERANK = WM_USER + 1;

LRESULT OnCreate(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnTimer(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnChar(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnLButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnRButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnMyTrySaveRank(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT OnDestroy(HWND hWnd, WPARAM wParam, LPARAM lParam);

void Reset(const RECT &rt);
void UpdateGameTime();
void CreateBullet(const RECT &rt, int x, int y);
bool CheckLose();
void TrySaveAndNotifyRank(HWND hWnd, int time);

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
#define ONMSG(msg, fn) case msg: return fn(hWnd, wParam, lParam)
	switch (iMsg)
	{
		ONMSG(WM_CREATE, OnCreate);
		ONMSG(WM_PAINT, OnPaint);
		ONMSG(WM_TIMER, OnTimer);
		ONMSG(WM_CHAR, OnChar);
		ONMSG(WM_LBUTTONDOWN, OnLButtonDown);
		ONMSG(WM_RBUTTONDOWN, OnRButtonDown);
		ONMSG(WM_MY_TRYSAVERANK, OnMyTrySaveRank);
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
	GetClientRect(hWnd, &rt);

	hdc = GetDC(hWnd);
	hMemDC = CreateCompatibleDC(hdc);
	hBitmap = CreateCompatibleBitmap(hdc, rt.right, rt.bottom);
	hOldBit = (HBITMAP)SelectObject(hMemDC, hBitmap);
	FillRect(hMemDC, &rt, (HBRUSH)GetStockObject(BLACK_BRUSH));
	SelectObject(hMemDC, hOldBit);
	DeleteDC(hMemDC);
	ReleaseDC(hWnd, hdc);

	hbrBullet = CreateSolidBrush(RGB(255, 255, 255));
	hbrBulletHit = CreateSolidBrush(RGB(255, 52, 25));
	hbrPlayer = CreateSolidBrush(RGB(0, 255, 255));

	SetTimer(hWnd, MOVE_TIMER_ID, MOVE_TIMER_FREQ, NULL);
	SetTimer(hWnd, CREATION_TIMER_ID, CREATION_TIMER_FREQ, NULL);

	Reset(rt);

	return 0;
}

void Reset(const RECT &rt)
{
	StartTick = GetTickCount();
	GameTime = 0;
	bLose = false;
	player.x = rt.right / 2;
	player.y = rt.bottom / 2;
	bullets.clear();
}

void UpdateGameTime()
{
	GameTime = GetTickCount() - StartTick;
}

LRESULT OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	BeginPaint(hWnd, &ps);

	if (hBitmap != NULL)
	{
		if (!bLose)
			UpdateGameTime();

		HBITMAP hOldBit;
		HDC hMemDC;

		RECT rt;
		GetClientRect(hWnd, &rt);

		hMemDC = CreateCompatibleDC(ps.hdc);
		hOldBit = (HBITMAP)SelectObject(hMemDC, hBitmap);

		FillRect(hMemDC, &rt, (HBRUSH)GetStockObject(BLACK_BRUSH));

		HPEN hOldPen = (HPEN)SelectObject(hMemDC, GetStockObject(NULL_PEN));
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hbrBullet);
		for (const Bullet &b : bullets)
		{
			if (!b.bHit)
				DrawPointEllipse(hMemDC, (int)b.x, (int)b.y, DRAW_RADIUS);
		}

		SelectObject(hMemDC, hbrPlayer);
		DrawPointEllipse(hMemDC, player.x, player.y, DRAW_RADIUS);

		if (bLose)
		{
			for (const Bullet &b : bullets)
			{
				if (b.bHit)
				{
					HBRUSH old = (HBRUSH)SelectObject(hMemDC, hbrBulletHit);
					DrawPointEllipse(hMemDC, (int)b.x, (int)b.y, DRAW_RADIUS);
					SelectObject(hMemDC, old);
				}
			}
		}

		SelectObject(hMemDC, hOldBrush);
		SelectObject(hMemDC, hOldPen);

		SetTextColor(hMemDC, RGB(0, 255, 52));
		SetBkMode(hMemDC, TRANSPARENT);
		TCHAR str[256];
		wsprintf(str, L"%5d.%03d sec", GameTime / 1000, GameTime % 1000);
#ifndef DISABLE_RESTART
		if (bLose)
		{
			lstrcat(str, L" - click or type ENTER for restart");
		}
#endif
		TextOut(hMemDC, 0, 0, str, lstrlen(str));

		BitBlt(ps.hdc, 0, 0, rt.right, rt.bottom, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBit);
		DeleteDC(hMemDC);
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

	GetClientRect(hWnd, &rt);

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

			player.x = range(MOVE_LIMIT, player.x, rt.right - MOVE_LIMIT);
			player.y = range(MOVE_LIMIT, player.y, rt.bottom - MOVE_LIMIT);

			for (auto it = bullets.begin(); it != bullets.end(); )
			{
				it->x += it->dx;
				it->y += it->dy;

				if ((it->x < 0 || it->x > rt.right) || (it->y < 0 || it->y > rt.bottom))
				{
					bullets.erase(it++);
				}
				else
				{
					++it;
				}
			}
			break;
		case CREATION_TIMER_ID:
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
			break;
	}

	bLose = CheckLose();
	if (bLose)
		PostMessage(hWnd, WM_MY_TRYSAVERANK, 0, 0);
	InvalidateRect(hWnd, NULL, FALSE);

	return 0;
}

LRESULT OnChar(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (wParam == L'q' || wParam == L'Q' || wParam == L'ㅂ' || wParam == L'ㅃ')
	{
		SendMessage(hWnd, WM_CLOSE, 0, 0);
	}
	else if (wParam == L'\r')
	{
		if (bLose)
		{
			RECT rt;
			GetClientRect(hWnd, &rt);
			Reset(rt);
		}
	}
	return 0;
}

LRESULT OnLButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
#ifndef DISABLE_RESTART
	if (bLose)
	{
		RECT rt;
		GetClientRect(hWnd, &rt);
		Reset(rt);
	}
#endif
	return 0;
}

LRESULT OnRButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	SendMessage(hWnd, WM_CLOSE, 0, 0);
	return 0;
}

void CreateBullet(const RECT &rt, int x, int y)
{
	const float kappa = 0.35f;

	Bullet ret;
	ret.x = x;
	ret.y = y;
	ret.bHit = false;

	float d_x = player.x - x;
	float d_y = player.y - y;
	float d_length = sqrt(pow2(d_x) + pow2(d_y));

	float a = 1.0f / (rt.right / 2);
	float b = 1.0f / (rt.bottom / 2);

	float x0 = fabs(a * (x - rt.right / 2)), y0 = fabs(b * (y - rt.bottom / 2));
	float t = x0;
	x0 = fmax(x0, y0);
	y0 = fmin(t, y0);

	t = BULLET_FULL_ARRIVAL_TIME * (1 - kappa);
	float calc = (kappa / sqrtf(pow2(y0) + 1)) - 1;
	float v_x = calc / t;
	float v_y = (calc * y0) / t;

	float speed = sqrtf(pow2(v_x / a) + pow2(v_y / b));

	ret.dx = d_x / d_length * speed;
	ret.dy = d_y / d_length * speed;

	bullets.push_back(ret);
}

bool CheckLose()
{
	bool ret = false;
	for (Bullet &b : bullets)
	{
		if (pow2((int)b.x - player.x) + pow2((int)b.y - player.y) <= pow2(HIT_RADIUS))
		{
			b.bHit = true;
			ret = true;
		}
	}
	return ret;
}

LRESULT OnMyTrySaveRank(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	TrySaveAndNotifyRank(hWnd, GameTime);
	return 0;
}

LRESULT OnDestroy(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	KillTimer(hWnd, MOVE_TIMER_ID);
	KillTimer(hWnd, CREATION_TIMER_ID);

	DeleteObject(hbrBullet);
	DeleteObject(hbrPlayer);
	DeleteObject(hBitmap);
	hBitmap = NULL;

	PostQuitMessage(0);
	return 0;
}

void TrySaveAndNotifyRank(HWND hWnd, int time)
{
	const char * const fname = "rank.txt";

	int score[5] = { 0 }, idx = 0;
	int * const begin = &score[0];
	int * const end = &score[sizeof(score) / sizeof(score[0])];

	FILE *pf = fopen(fname, "r");
	if (pf == NULL)
		pf = fopen(fname, "w+");

	if (pf != NULL)
	{
		while (!ferror(pf))
		{
			fscanf(pf, "%d", &score[idx++]);
			if (idx >= 5)
				break;
		}
		if (!(!feof(pf) && idx == 5))
		{
			for (int &s : score)
				s = 0;
		}

		fclose(pf);
	}

	auto it = std::upper_bound(begin, end, time, std::greater<int>());
	int rank = -1;
	if (it != end)
	{
		rank = it - begin + 1;

		for (int tmp = time; it != end; ++it)
		{
			std::swap(*it, tmp);
		}

		pf = fopen(fname, "w");
		if (pf != NULL)
		{
			idx = 0;
			while (!ferror(pf))
			{
				fprintf(pf, "%d ", score[idx++]);
				if (idx >= 5)
					break;
			}
			if (ferror(pf))
			{
				MessageBox(hWnd, L"랭킹 저장 중 에러가 발생하였습니다.", L"에러", MB_ICONERROR | MB_OK);
			}
			fclose(pf);
		}
		else
		{
			MessageBox(hWnd, L"랭킹 파일을 열 수 없습니다.", L"에러", MB_ICONERROR | MB_OK);
		}
	}

	TCHAR msg[512];
	wsprintf(msg,
		L"1위; %5d.%03d sec\n"
		L"2위; %5d.%03d sec\n"
		L"3위; %5d.%03d sec\n"
		L"4위; %5d.%03d sec\n"
		L"5위; %5d.%03d sec\n",
		score[0] / 1000, score[0] % 1000,
		score[1] / 1000, score[1] % 1000,
		score[2] / 1000, score[2] % 1000,
		score[3] / 1000, score[3] % 1000,
		score[4] / 1000, score[4] % 1000
		);
	if (rank != -1)
	{
		TCHAR msg2[128];
		wsprintf(msg2, L"\n\n%d위를 달성했습니다!", rank);
		lstrcat(msg, msg2);
	}

	MessageBox(hWnd, msg, L"순위표", MB_OK);
}
