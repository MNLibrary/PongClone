#include <SDL.h>
#include <stdio.h>
#include <string>
#include <cmath>
#include <random>
#include <time.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

bool InitSDL();
void Cleanup();
SDL_Surface* LoadSurface(std::string path);

SDL_Window* gWindow = NULL;
SDL_Surface* gScreenSurface = NULL;

bool InitSDL()
{
	bool success = true;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error : %s\n", SDL_GetError());
	}
	else
	{
		gWindow = SDL_CreateWindow("Pong",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			SDL_WINDOW_SHOWN);
		if (gWindow == NULL)
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
			success = false;
		}
		else
		{
			gScreenSurface = SDL_GetWindowSurface(gWindow);
		}
	}
	return success;
}

void Cleanup()
{
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	SDL_Quit();
}

SDL_Surface* LoadSurface(std::string path)
{
	SDL_Surface* loadedSurface = SDL_LoadBMP(path.c_str());
	if (loadedSurface == NULL)
	{
		printf("Unable to load image %s! SDL Error %s\n", path.c_str(), SDL_GetError());
	}
	return loadedSurface;
}

void ClearSurface(SDL_Surface* surface, Uint32 clearColor)
{
	SDL_Rect rect{ 0,0,surface->w,surface->h };
	SDL_FillRect(surface, &rect, clearColor);
}

//---------------
//시스템
enum class KeyCode
{
	UNKNOWN,
	UP,
	DOWN,
	Q,
};
bool gHasSystemInput;
KeyCode gSystemInput;
float gTimeDelta;

bool IsPressed(KeyCode keyCode)
{
	return gHasSystemInput && gSystemInput == keyCode;
}

bool gQuitApplication;

void HandleEvent()
{
	gHasSystemInput = false;
	gQuitApplication = false;

	SDL_Event e;
	while (SDL_PollEvent(&e) != 0)
	{
		if (e.type == SDL_QUIT)
		{
			gQuitApplication = true;
		}
		else if (e.type == SDL_KEYDOWN)
		{
			gHasSystemInput = true;
			switch (e.key.keysym.sym)
			{
			case SDLK_UP:
			case SDLK_w:
				gSystemInput = KeyCode::UP;
				break;
			case SDLK_DOWN:
			case SDLK_s:
				gSystemInput = KeyCode::DOWN;
				break;
			case SDLK_q:
				gSystemInput = KeyCode::Q;
				break;
			default:
				gSystemInput = KeyCode::UNKNOWN;
				break;
			}
		}
	}
}


//---------------
//게임

struct Rect
{
	float X, Y, W, H;
	bool Contains(float x, float y)
	{
		return X <= x && x <= X + W && Y <= y && y <= Y + H;
	}
	float XMin() const  { return X; }
	float YMin() const { return Y; }
	float XMax() const { return X + W; }
	float YMax() const { return Y + H; }
};

struct Stick
{
	float CenterX, CenterY;
	float Length;
};

struct Ball
{
	float CenterX, CenterY;
	float Radius;
	float Speed;
	float DirectionX, DirectionY;
};

struct Stadium
{
	float Width,Height;
};

enum class GameState
{
	INITIALIZE,
	STANDBY,
	PLAYING,
	GAMEOVER,
};

struct UI
{
	bool Show;
	Rect Rect;
	SDL_Surface* Surface;
};

SDL_Surface* gImages[3];

Stick gSticks[2];
Ball gBall;
Stadium gStadium;
GameState gGameState;
UI gTitle;
UI gGameOver;
int gWinner;
float gRenderAreaWidth, gRenderAreaHeight;

void Transform(float x, float y, float* viewPortX, float* viewPortY)
{
	//중심은 (0,0) 평행이동X 회전X 스케일X
	float normalizedX = x / gRenderAreaWidth;
	float normalizedY = y / gRenderAreaHeight;
	*viewPortX = 0.5f + normalizedX;
	*viewPortY = 0.5f + normalizedY;
}

void Transform(const Rect& rect, Rect* viewPortRect)
{
	float minX, minY;
	float maxX, maxY;
	Transform(rect.XMin(), rect.YMin(), &minX, &minY);
	Transform(rect.XMax(), rect.YMax(), &maxX, &maxY);
	viewPortRect->X = minX;
	viewPortRect->Y = minY;
	viewPortRect->W = maxX-minX;
	viewPortRect->H = maxY-minY;
}

Rect GetRect(const Stick& stick)
{
	return Rect
	{
		stick.CenterX - 5.0f,
		stick.CenterY - stick.Length * 0.5f,
		10.0f,
		stick.Length
	};
}

Rect GetRect(const Stadium& stadium)
{
	return Rect
	{
		-stadium.Width * 0.5f,  -stadium.Height * 0.5f,
		stadium.Width,  stadium.Height,
	};
}

Rect GetRect(const Ball& ball)
{
	return Rect
	{
		ball.CenterX - ball.Radius, ball.CenterY - ball.Radius,
		ball.Radius * 2.0f, ball.Radius * 2.0f,
	};
}

SDL_Rect Convert(const Rect& rect, const SDL_Surface* surface)
{
	Rect viewPortRect;
	Transform(rect, &viewPortRect);
	return SDL_Rect
	{
		(int)(viewPortRect.X * surface->w), (int)(viewPortRect.Y * surface->h),
		(int)(viewPortRect.W * surface->w), (int)(viewPortRect.H * surface->h)
	};
}

void DrawLineRect(SDL_Surface* dst, SDL_Rect rect, Uint32 color, float thickness)
{
	float thicknessHalf = thickness * 0.5f;
	SDL_Rect edgeL
	{
		rect.x - thicknessHalf, rect.y - thicknessHalf,
		thickness, rect.h + thickness
	};
	SDL_Rect edgeR
	{
		rect.x + rect.w - thicknessHalf, rect.y - thicknessHalf,
		thickness, rect.h + thickness
	};
	SDL_Rect edgeT
	{
		rect.x - thicknessHalf, rect.y + rect.h - thicknessHalf,
		rect.w + thickness, thickness
	};
	SDL_Rect edgeB
	{
		rect.x - thicknessHalf, rect.y - thicknessHalf,
		rect.w + thickness, thickness
	};
	SDL_FillRect(dst, &edgeL, color);
	SDL_FillRect(dst, &edgeR, color);
	SDL_FillRect(dst, &edgeT, color);
	SDL_FillRect(dst, &edgeB, color);
}

//렌더링 규칙. 추상화까지는 불필요
void RenderStick(SDL_Surface* dst, const Stick& stick)
{
	SDL_Rect renderRect = Convert(GetRect(stick), dst);
	SDL_FillRect(dst, &renderRect, SDL_MapRGB(dst->format, 0xFF, 0xFF, 0xFF));
}

void RenderStadium(SDL_Surface* dst, const Stadium& stadium)
{
	float thickness = 2.0f;
	SDL_Rect renderRect = Convert(GetRect(stadium), dst);
	DrawLineRect(dst, renderRect, SDL_MapRGB(dst->format, 0xFF, 0xFF, 0xFF), 2.0f);
}

void RenderBall(SDL_Surface* dst, const Ball& ball)
{
	SDL_Rect renderRect = Convert(GetRect(ball), dst);
	SDL_FillRect(dst, &renderRect, SDL_MapRGB(dst->format, 0xFF, 0xFF, 0xFF));
}

void RenderUI(SDL_Surface* dst, const UI& ui)
{
	if (ui.Show && ui.Surface != NULL)
	{
		SDL_Rect renderRect = Convert(ui.Rect, dst);
		SDL_BlitScaled(ui.Surface, NULL, dst, &renderRect);
	}
}

//게임로직

void LoadResources()
{
	gImages[0] = LoadSurface("Media/Title.bmp");
	gImages[1] = LoadSurface("Media/Player1Win.bmp");
	gImages[2] = LoadSurface("Media/Player2Win.bmp");
}

void ReleaseResources()
{
	for (int i = 0; i < 3; i++)
	{
		SDL_FreeSurface(gImages[i]);
	}
}

void InitGame()
{
	//중앙이 (0,0)
	gStadium = Stadium{
		480.0f, 360.0f,
	};
	gSticks[0] = Stick{
		-200.0f, 0.0f, 50.0f,
	};
	gSticks[1] = Stick{
		200.0f, 0.0f, 50.0f,
	};
	gBall = Ball{
		0.0f,0.0f,
		3.0f,
		0.0f,
		0.0f,0.0f,
	};
	gGameState = GameState::STANDBY;
	gWinner = -1;
	gRenderAreaWidth = 500.0f;
	gRenderAreaHeight = 380.0f;
	gTitle = UI
	{
		false,
		Rect{-125.0f,-150.0f,250.0f,100.0f},
		gImages[0]
	};
	gGameOver = UI
	{
		false,
		Rect{-125.0f, -150.0f,250.0f,100.0f},
		NULL
	};
}

void GameTick()
{
	if (gGameState == GameState::INITIALIZE)
	{
		InitGame();
		gGameState = GameState::STANDBY;
	}
	else if (gGameState == GameState::STANDBY)
	{
		gTitle.Show = true;
		if (gHasSystemInput)
		{
			gGameState = GameState::PLAYING;
			gTitle.Show = false;
			gBall.Speed = 150.0f;
			gBall.DirectionX = -1.0f;
		}
	}
	else if (gGameState == GameState::PLAYING)
	{
		//플레이어 업데이트
		if (IsPressed(KeyCode::UP))
		{
			gSticks[0].CenterY -= 400.0f * gTimeDelta;
		}
		else if (IsPressed(KeyCode::DOWN))
		{
			gSticks[0].CenterY += 400.0f * gTimeDelta;
		}

		//AI 업데이트
		if (gSticks[1].CenterY < gBall.CenterY )
		{
			gSticks[1].CenterY += 50.0f * gTimeDelta;
		}
		else if (gSticks[1].CenterY > gBall.CenterY)
		{
			gSticks[1].CenterY -= 50.0f * gTimeDelta;
		}

		//공 업데이트
		float cos = gBall.DirectionX / sqrt(gBall.DirectionX * gBall.DirectionX + gBall.DirectionY * gBall.DirectionY);
		float sin = gBall.DirectionY / sqrt(gBall.DirectionX * gBall.DirectionX + gBall.DirectionY * gBall.DirectionY);
		float velocityX = cos * gBall.Speed * gTimeDelta;
		float velocityY = sin * gBall.Speed * gTimeDelta;
		gBall.CenterX += velocityX;
		gBall.CenterY += velocityY;

		//외곽충돌
		Rect ballRect = GetRect(gBall);
		Rect stadiumRect = GetRect(gStadium);

		if (ballRect.YMax() > stadiumRect.YMax())
		{
			gBall.CenterY += gBall.CenterY - stadiumRect.YMax();
			gBall.DirectionY *= -1.0f;
		}
		else if (ballRect.YMin() < stadiumRect.YMin())
		{
			gBall.CenterY += gBall.CenterY - stadiumRect.YMin();
			gBall.DirectionY *= -1.0f;
		}

		if (gSticks[0].CenterY + gSticks[0].Length * 0.5f > stadiumRect.YMax())
		{
			gSticks[0].CenterY = stadiumRect.YMax() - gSticks[0].Length * 0.5f;
		}
		else if (gSticks[0].CenterY - gSticks[0].Length * 0.5f < stadiumRect.YMin())
		{
			gSticks[0].CenterY = stadiumRect.YMin() + gSticks[0].Length * 0.5f;
		}

		if (gSticks[1].CenterY + gSticks[1].Length * 0.5f > stadiumRect.YMax())
		{
			gSticks[1].CenterY = stadiumRect.YMax() - gSticks[1].Length * 0.5f;
		}
		else if (gSticks[1].CenterY - gSticks[1].Length * 0.5f < stadiumRect.YMin())
		{
			gSticks[1].CenterY = stadiumRect.YMin() + gSticks[1].Length * 0.5f;
		}

		//스틱충돌
		if (GetRect(gSticks[0]).Contains(gBall.CenterX, gBall.CenterY) && 
			gBall.DirectionX <= 0)
		{
			gBall.DirectionX *= -1.0f;
			gBall.DirectionY += (rand() % 100 - 50) / 100.0f; //방향 트는거 제대로 만들려면 귀찮음
		}
		if (GetRect(gSticks[1]).Contains(gBall.CenterX, gBall.CenterY) &&
			gBall.DirectionX >= 0)
		{
			gBall.DirectionX *= -1.0f;
			gBall.DirectionY += (rand() % 100 - 50) / 100.0f;
		}

		//게임판정
		if (ballRect.XMax() > stadiumRect.XMax())
		{
			gGameState = GameState::GAMEOVER;
			gWinner = 0;
		}
		else if (ballRect.XMin() < stadiumRect.XMin())
		{
			gGameState = GameState::GAMEOVER;
			gWinner = 1;
		}
	}
	else if (gGameState == GameState::GAMEOVER)
	{
		gGameOver.Show = true;
		gGameOver.Surface = gWinner == 0 ? gImages[1] : gImages[2];
		
		if (gHasSystemInput)
		{
			if (IsPressed(KeyCode::Q))
			{
				gQuitApplication = true;
			}
			else
			{
				gGameState = GameState::INITIALIZE;
			}
		}
	}
}

void GameRender()
{
	//서피스는 좌상단이 오리진
	ClearSurface(gScreenSurface, SDL_MapRGB(gScreenSurface->format, 0x00, 0x00, 0x00));

	//정렬규칙은 수동
	RenderStadium(gScreenSurface, gStadium);
	RenderStick(gScreenSurface, gSticks[0]);
	RenderStick(gScreenSurface, gSticks[1]);
	RenderBall(gScreenSurface, gBall);
	RenderUI(gScreenSurface, gGameOver);
	RenderUI(gScreenSurface, gTitle);

	SDL_UpdateWindowSurface(gWindow);
}

//--------------------------------

int main(int argc, char* argv[])
{
	if (!InitSDL())
	{
		printf("Failed to initialize!\n");
	}
	else
	{
		Uint32 lastTick = SDL_GetTicks();
		srand(time(NULL));

		LoadResources();
		while (!gQuitApplication)
		{
			Uint32 currentTick = SDL_GetTicks();
			gTimeDelta = (currentTick - lastTick) / 1000.0;
			if (gTimeDelta > 1.0 / 60.0)
			{
				HandleEvent();
				GameTick();
				GameRender();
				lastTick = currentTick;
			}
		}
		ReleaseResources();
	}
	Cleanup();

	return 0;
}