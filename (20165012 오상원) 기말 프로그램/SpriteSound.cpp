#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <stdio.h>
#include "ddutil.h"
#include <dsound.h>
#include "dsutil.h"
#include <vector>



#define _GetKeyState( vkey ) HIBYTE(GetAsyncKeyState( vkey ))
#define _GetKeyPush( vkey )  LOBYTE(GetAsyncKeyState( vkey ))
#define MAXBULLET 6
HWND MainHwnd;

LPDIRECTDRAW         DirectOBJ;
LPDIRECTDRAWSURFACE  RealScreen;
LPDIRECTDRAWSURFACE  BackScreen;
LPDIRECTDRAWSURFACE  SpriteImage;
LPDIRECTDRAWSURFACE  BackGround;
LPDIRECTDRAWSURFACE  StartUI;
LPDIRECTDRAWSURFACE  ReadyUI;
LPDIRECTDRAWSURFACE  WinUI;
LPDIRECTDRAWSURFACE  LoseUI;

LPDIRECTDRAWCLIPPER	ClipScreen;

// gamestart = 0, gamePlay =1, gamewin =2 gamelose = 3 ;   
int gameStatus = 0;
int gFullScreen=0, Click=0;
int GameTotalFrame =0;
int BulletCount = 6;
int MouseX, MouseY;
int gWidth = 640, gHeight = 480;
int CharactorPosX = 50, CharactorPosY = 70;
bool retry = false;

////////////////////

LPDIRECTSOUND       SoundOBJ = NULL;
LPDIRECTSOUNDBUFFER SoundDSB = NULL;
DSBUFFERDESC        DSB_desc;

HSNDOBJ Sound[10];
struct DelayedData
{
	DelayedData(int arg_time, int arg_x,int arg_y):time(arg_time),x(arg_x),y(arg_y){}
	int	time = 0;
	int x = 0 ;
	int	y = 0;
};

std::vector<DelayedData> EnemyPosQ;
std::vector<DelayedData> EnemyPosTrailQ;

std::vector<DelayedData> BulletPosQ;
std::vector<DelayedData> BulletPosTrailQ;
extern void CommInit(int argc, char **argv);
extern void CommSend(char *sending);
extern void CommRecv(char *recvData);



BOOL _InitDirectSound( void )
{
    if ( DirectSoundCreate(NULL,&SoundOBJ,NULL) == DS_OK )
    {
        if (SoundOBJ->SetCooperativeLevel(MainHwnd,DSSCL_PRIORITY)!=DS_OK) return FALSE;

        memset(&DSB_desc,0,sizeof(DSBUFFERDESC));
        DSB_desc.dwSize = sizeof(DSBUFFERDESC);
        DSB_desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;

        if (SoundOBJ->CreateSoundBuffer(&DSB_desc,&SoundDSB,NULL)!=DS_OK) return FALSE;
        SoundDSB -> SetVolume(DSBVOLUME_MAX); // DSBVOLUME_MIN
        SoundDSB -> SetPan(DSBPAN_RIGHT);
        return TRUE;
    }
    return FALSE;
}

void _Play( int num )
{
    SndObjPlay( Sound[num], NULL );
}

//kind: move = 0 , click = 1, shotPayback =2, calladmin= 3; 
void _SendData(char* str, int Kind, int posx,int posy) 
{
	sprintf(str, "%d %d %d", Kind, posx, posy);
	CommSend(str);
}


////////////////////////


BOOL Fail( HWND hwnd )
{
    ShowWindow( hwnd, SW_HIDE );
    MessageBox( hwnd, "DIRECT X 초기화에 실패했습니다.", "게임 디자인", MB_OK );
    DestroyWindow( hwnd );
    return FALSE;
}

void _ReleaseAll( void )
{
    if ( DirectOBJ != NULL )
    {
        if ( RealScreen != NULL )
        {
            RealScreen->Release();
            RealScreen = NULL;
        }
        if ( SpriteImage != NULL )
        {
            SpriteImage->Release();
            SpriteImage = NULL;
        }
        if ( BackGround != NULL )
        {
            BackGround->Release();
            BackGround = NULL;
        }
        DirectOBJ->Release();
        DirectOBJ = NULL;
    }
}

long FAR PASCAL WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	char temp[120];

    switch ( message )
    {

        case    WM_MOUSEMOVE    :   MouseX = LOWORD(lParam);
                                    MouseY = HIWORD(lParam);
                                    break;

		case	WM_LBUTTONDOWN	: 	Click=1;
			if (gameStatus != 1) 
			{
				retry = true;
				break;
			}

			if (BulletCount < 1) 
			{
				_Play(6);
			}
			else 
			{
				_Play(2);
				_SendData(temp, 1, MouseX, MouseY);
				BulletCount--;
			}
									break;

        case    WM_DESTROY      :  _ReleaseAll();
                                    PostQuitMessage( 0 );
                                    break;
		case	WM_KEYDOWN:
			switch (wParam)
			{
			case VK_ESCAPE:
			case VK_F12:
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				return 0;

			case 0x41://A left
				CharactorPosX -= 20;									
				_SendData(temp, 0, CharactorPosX, CharactorPosY);
				return 0;

			case 0x44:// D right
				CharactorPosX += 20;
				_SendData(temp, 0, CharactorPosX, CharactorPosY);
				return 0;

			case 0x57:// W up
				CharactorPosY -= 20;
				_SendData(temp, 0, CharactorPosX, CharactorPosY);
				return 0;

			case 0x53:// S down
				CharactorPosY += 20;
				_SendData(temp, 0, CharactorPosX, CharactorPosY);
				return 0;

			case VK_SPACE:// 
				break;
			}
			break;

    }
    return DefWindowProc( hWnd, message, wParam, lParam );
}

BOOL _GameMode( HINSTANCE hInstance, int nCmdShow, int x, int y, int bpp )
{
    HRESULT result;
    WNDCLASS wc;
    DDSURFACEDESC ddsd;
    DDSCAPS ddscaps;
    LPDIRECTDRAW pdd;

    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "EXAM3";
    RegisterClass( &wc );

	if((MainHwnd=CreateWindow("EXAM3", "(20165012)오상원", WS_OVERLAPPEDWINDOW, 0, 0, x, 
									y, NULL, NULL, hInstance, NULL))==NULL)
			ExitProcess(1);
	SetWindowPos(MainHwnd, NULL, 100, 100, x, y, SWP_NOZORDER);

    SetFocus( MainHwnd );
    ShowWindow( MainHwnd, nCmdShow );
    UpdateWindow( MainHwnd );
//    ShowCursor( FALSE );

    result = DirectDrawCreate( NULL, &pdd, NULL );
    if ( result != DD_OK ) return Fail( MainHwnd );

    result = pdd->QueryInterface(IID_IDirectDraw, (LPVOID *) &DirectOBJ);
    if ( result != DD_OK ) return Fail( MainHwnd );


	// 윈도우 핸들의 협력 단계를 설정한다.
	if(gFullScreen){
	    result = DirectOBJ->SetCooperativeLevel( MainHwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
		if ( result != DD_OK ) return Fail( MainHwnd );

		result = DirectOBJ->SetDisplayMode( x, y, bpp);
		if ( result != DD_OK ) return Fail( MainHwnd );

		memset( &ddsd, 0, sizeof(ddsd) );
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
		ddsd.dwBackBufferCount = 1;

	    result = DirectOBJ -> CreateSurface( &ddsd, &RealScreen, NULL );
	   if ( result != DD_OK ) return Fail( MainHwnd );

		memset( &ddscaps, 0, sizeof(ddscaps) );
		ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
		result = RealScreen -> GetAttachedSurface( &ddscaps, &BackScreen );
		if ( result != DD_OK ) return Fail( MainHwnd );
	}
	else{
	    result = DirectOBJ->SetCooperativeLevel( MainHwnd, DDSCL_NORMAL );
		if ( result != DD_OK ) return Fail( MainHwnd );

		memset( &ddsd, 0, sizeof(ddsd) );
	    ddsd.dwSize = sizeof( ddsd );
		ddsd.dwFlags = DDSD_CAPS;
	    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		ddsd.dwBackBufferCount = 0;

		result = DirectOBJ -> CreateSurface( &ddsd, &RealScreen, NULL );
	    if(result != DD_OK) return Fail(MainHwnd);

		memset( &ddsd, 0, sizeof(ddsd) );
		ddsd.dwSize = sizeof(ddsd);
	    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		ddsd.dwWidth = x; 
		ddsd.dwHeight = y;
		result = DirectOBJ->CreateSurface( &ddsd, &BackScreen, NULL );
		if ( result != DD_OK ) return Fail( MainHwnd );

		result = DirectOBJ->CreateClipper( 0, &ClipScreen, NULL);
		if ( result != DD_OK ) return Fail( MainHwnd );

		result = ClipScreen->SetHWnd( 0, MainHwnd );
		if ( result != DD_OK ) return Fail( MainHwnd );

		result = RealScreen->SetClipper( ClipScreen );
		if ( result != DD_OK ) return Fail( MainHwnd );

		SetWindowPos(MainHwnd, NULL, 100, 100, x, y, SWP_NOZORDER | SWP_NOACTIVATE); 
	}


    return TRUE;
}

void comHandle(char *recvBuf){

	//마우스 데이터는 저장후 세팅해둠
	int kind, rcv_x, rcv_y;
	sscanf(recvBuf, "%d %d %d", &kind, &rcv_x, &rcv_y);

	//총알 발사 데이터랑 이동 데이터랑 분리.
	DelayedData newdata(GameTotalFrame, 0, 0);
	
	switch (kind)
	{
	case 0://사람 위치.
		newdata.x = rcv_x;
		newdata.y = rcv_y;
		EnemyPosQ.push_back(newdata);
		break;
	case 1:// 총알
		newdata.x = rcv_x;
		newdata.y = rcv_y;
		BulletPosQ.push_back(newdata);
		break;
	case 2://총알 맞은거 피드백.
		if (BulletCount < MAXBULLET) 
		{
			BulletCount++;
			_Play(4);
		}
		if (rcv_x > 0) 
		{
			gameStatus = 2;
			_Play(7);
		}
		break;
	default:
		break;
	}

	//MouseX= enemyPosx;
	//MouseY= enemyPosy;
	//Click=clickc;



}

// 숫자 표시

void SetNumber(int num, int x, int y)
{
	RECT SpriteRect;
	SpriteRect.left = 35 * num;
	SpriteRect.top = 310;
	SpriteRect.right = SpriteRect.left + 35;
	SpriteRect.bottom = 354;
	BackScreen->BltFast(x - 40, y - 15, SpriteImage, &SpriteRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

void mouseAnimate() 
{
	RECT BackRect = { 0, 0, 640, 480 }, SpriteRect;

	BackScreen->BltFast(0, 0, BackGround, &BackRect, DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY);

	// 마우스 애니 애니메이팅 부분
	if (Click)
		SpriteRect.left = 100;
	else
		SpriteRect.left = 0;

	SpriteRect.top = 80;
	SpriteRect.right = SpriteRect.left + 100;
	SpriteRect.bottom = 170;

	if (MouseX <= 50) MouseX = 50;
	if (MouseX > 590) MouseX = 590;
	if (MouseY <= 35) MouseY = 35;
	if (MouseY > 445) MouseY = 445;

	BackScreen->BltFast(MouseX - 40, MouseY - 25, SpriteImage, &SpriteRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
	SetNumber(BulletCount, MouseX + 70, MouseY + 55);

}

void charactorPlace() 
{
	RECT SpriteRect;
	static int BasicCount = 0;
	static bool isUP = false;
	static int Frame = 0;

	SpriteRect.left = (int)(Frame / 2) * 100;
	SpriteRect.top = 0;
	SpriteRect.right = SpriteRect.left + 100;
	SpriteRect.bottom = 70;

	if (Click) {
		if (++Frame >= 8) {
			Frame = 0;
			Click = 0;
		}
	}
	else
	{
		// 위아래로 움직임+ 현재 위치 전송
		if (BasicCount++ >= 40) {

			if (isUP) isUP = false;
			else isUP = true;
			char temp[120];
			_SendData(temp, 0, CharactorPosX, CharactorPosY);
			BasicCount = 0;
		}
		if (isUP) { SpriteRect.top = 5; SpriteRect.bottom = 75; }
	}
	if (CharactorPosX <= 50) CharactorPosX = 50;
	if (CharactorPosX > 590) CharactorPosX = 590;
	if (CharactorPosY <= 35) CharactorPosY = 35;
	if (CharactorPosY > 445) CharactorPosY = 445;
	BackScreen->BltFast(CharactorPosX - 50, CharactorPosY - 35, SpriteImage, &SpriteRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}


//총알 자국들 확인및 피격 처리 하는 곳
void checkBulletTrail() 
{
	while (!BulletPosQ.empty())
	{
		int delayedTime = GameTotalFrame - BulletPosQ[0].time;
		if (delayedTime > 50)
		{
			char temp[120];
			DelayedData newdata(GameTotalFrame, BulletPosQ[0].x, BulletPosQ[0].y);

			if (CharactorPosX > BulletPosQ[0].x - 70 && CharactorPosX < BulletPosQ[0].x + 70
				&& CharactorPosY>BulletPosQ[0].y - 70 && CharactorPosY < BulletPosQ[0].y + 70)
			{
				_SendData(temp, 2, 1, 1);
				_Play(5);
				gameStatus = 3;
			}
			else {
				_SendData(temp, 2, 0, 1);
				_Play(8);
			}
			BulletPosTrailQ.push_back(newdata);
			BulletPosQ.erase(BulletPosQ.begin());

		}
		else
			break;
	}
	RECT SpriteRect;
	SpriteRect.left = 100;
	SpriteRect.top = 160;
	SpriteRect.right = SpriteRect.left + 130;
	SpriteRect.bottom = 300;
	while (!BulletPosTrailQ.empty())
	{
		int delayedTime = GameTotalFrame - BulletPosTrailQ[0].time;
		if (delayedTime > 20)
		{
			BulletPosTrailQ.erase(BulletPosTrailQ.begin());
		}
		else
			break;
	}

	for (int i = 0; i < BulletPosTrailQ.size(); i++)
		BackScreen->BltFast(BulletPosTrailQ[i].x - 60, BulletPosTrailQ[i].y - 45, SpriteImage, &SpriteRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

//적 움직임 처리 후 나중에 보내는 곳 
void checkEnemyTrail() 
{
	while (!EnemyPosQ.empty())
	{
		int delayedTime = GameTotalFrame - EnemyPosQ[0].time;
		if (delayedTime > 30)
		{
			char temp[120];
			DelayedData newdata(GameTotalFrame, EnemyPosQ[0].x, EnemyPosQ[0].y);
			EnemyPosTrailQ.push_back(newdata);
			EnemyPosQ.erase(EnemyPosQ.begin());
		}
		else
			break;
	}

	RECT SpriteRect;
	SpriteRect.left = 0;
	SpriteRect.top = 230;
	SpriteRect.right = SpriteRect.left + 100;
	SpriteRect.bottom = 300;
	while (!EnemyPosTrailQ.empty())
	{
		int delayedTime = GameTotalFrame - EnemyPosTrailQ[0].time;
		if (delayedTime > 2)
		{
			EnemyPosTrailQ.erase(EnemyPosTrailQ.begin());
		}
		else
			break;
	}



	for (int i = 0; i < EnemyPosTrailQ.size(); i++)
		BackScreen->BltFast(EnemyPosTrailQ[i].x - 50, EnemyPosTrailQ[i].y -5 , SpriteImage, &SpriteRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

void StartGame() 
{
	RECT BackRect = { 0, 0, 640, 480 };
	char temp[100];

	if (Click == 1) 
	{
		_SendData(temp, 1, 1, 0);

		gameStatus = 1;

		srand(GameTotalFrame);
		CharactorPosX = ((rand() % 10) * 64);
		CharactorPosY = (rand() % 10) * 48;
	}
	BackScreen->BltFast(0, 0, StartUI, &BackRect, DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY);
}

void MainGame() 
{
	retry = false;
	//마우스 표시
	mouseAnimate();

	//캐릭터 배치 및 애니메이팅 부분
	charactorPlace();

	// 적의 총알이 딜레이되어서 도착 하는 것을 계산하는 부분. 
	checkBulletTrail();

	// 적의 움직임이 딜레이되어서 도착 하는 것을 계산하는 부분. 
	checkEnemyTrail();


}
void WinGame() 
{
	RECT BackRect = { 0, 0, 640, 480 }, SpriteRect;
	if (retry) 
	{
		gameStatus = 0;
		Click = 0;
		retry = false;
	}

	BackScreen->BltFast(0, 0, WinUI, &BackRect, DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY);

}
void LoseGame()
{

	RECT BackRect = { 0, 0, 640, 480 }, SpriteRect;

	if (retry) 
	{
		gameStatus = 0;
		Click = 0;
		retry = false;
	}

	BackScreen->BltFast(0, 0, LoseUI, &BackRect, DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY);

}



void CALLBACK _GameProc(HWND hWnd, UINT message, UINT wParam, DWORD lParam)
{
	GameTotalFrame++;
	
	switch (gameStatus)
	{
	case 0:
		StartGame();
		break;
	case 1:
		MainGame();
		break;
	case 2: 
		WinGame();
		break;
	case 3:
		LoseGame();
		break;
	default:
		break;
	}

	if(gFullScreen)
		RealScreen->Flip(NULL, DDFLIP_WAIT);
	else{
		RECT WinRect, SrcRect={0, 0, 640, 480};

		GetWindowRect(MainHwnd, &WinRect);
		RealScreen->Blt(&WinRect, BackScreen, &SrcRect, DDBLT_WAIT, NULL ); 
	}
}



int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    MSG msg;

    if ( !_GameMode(hInstance, nCmdShow, 640, 480, 32) ) return FALSE;

    SpriteImage = DDLoadBitmap( DirectOBJ, "sprite_char1.BMP", 0, 0 );
    BackGround  = DDLoadBitmap( DirectOBJ, "sprite_back.BMP", 0, 0 );
	StartUI = DDLoadBitmap(DirectOBJ, "sprite_Start.BMP", 0, 0);
	ReadyUI = DDLoadBitmap(DirectOBJ, "sprite_Ready.BMP", 0, 0);
	WinUI = DDLoadBitmap(DirectOBJ, "sprite_Win.BMP", 0, 0);
	LoseUI = DDLoadBitmap(DirectOBJ, "sprite_Lose.BMP", 0, 0);
    DDSetColorKey( SpriteImage, RGB(0,0,0) );

	SetTimer(MainHwnd, 1, 30, _GameProc);

	CommInit(NULL, NULL);


///////////////////

    if ( _InitDirectSound() )
    {
        Sound[0] = SndObjCreate(SoundOBJ,"EAST.WAV",1);
        Sound[1] = SndObjCreate(SoundOBJ,"LAND.WAV",2);
        Sound[2] = SndObjCreate(SoundOBJ,"GUN1.WAV",2);
        Sound[3] = SndObjCreate(SoundOBJ,"KNIFE1.WAV",2);
        Sound[4] = SndObjCreate(SoundOBJ,"RELOAD.WAV",2);
        Sound[5] = SndObjCreate(SoundOBJ,"DAMAGE2.WAV",2);
		Sound[6] = SndObjCreate(SoundOBJ, "TICK.WAV", 2);
		Sound[7] = SndObjCreate(SoundOBJ, "Jingle_Win.WAV", 2);
		Sound[8] = SndObjCreate(SoundOBJ, "RICOCHET.WAV", 2);
        SndObjPlay( Sound[0], DSBPLAY_LOOPING );
    }
//////////////////

    while ( !_GetKeyState(VK_ESCAPE) )
    {
        if ( PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) )
        {
            if ( !GetMessage(&msg, NULL, 0, 0) ) return msg.wParam;

            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
//        else _GameProc();
    }
    DestroyWindow( MainHwnd );

    return TRUE;
}
