#include "starter.h"
#include <iostream>
#include <vector>

// sounds
#include <maxmod9.h>
#include "soundbank.h"
#include "soundbank_bin.h"

// fonts
#include "Cglfont.h"
#include "font_si.h"
#include "font_16x16.h"

// Texture Packer auto-generated UV coords
#include "uvcoord_font_si.h"
#include "uvcoord_font_16x16.h"

#define HALF_WIDTH (SCREEN_WIDTH / 2)
#define HALF_HEIGHT (SCREEN_HEIGHT / 2)

glImage FontImages[FONT_SI_NUM_IMAGES];
glImage FontBigImages[FONT_16X16_NUM_IMAGES];

Cglfont Font;
Cglfont FontBig;

mm_sound_effect collisionSound;

const int WHITE = RGB15(255, 255, 255);
const int RED = RGB15(255, 0, 0);
const int BLUE = RGB15(0, 0, 255);

bool isGamePaused;
bool isAutoPlayMode = true;

Rectangle player = {HALF_WIDTH, SCREEN_HEIGHT - 16, 35, 8, WHITE};
Rectangle ball = {HALF_WIDTH, HALF_HEIGHT, 8, 8, WHITE};

const int PLAYER_SPEED = 5;

int ballVelocityX = 2;
int ballVelocityY = 2;

int playerScore;
int playerLives = 2;

std::vector<Rectangle> createBricks()
{
	std::vector<Rectangle> bricks;
	bricks.reserve(49);

	int brickPoints = 10;
	int positionX;
	int positionY = 20;

	for (int row = 0; row < 7; row++)
	{
		positionX = 0;

		for (int column = 0; column < 7; column++)
		{
			unsigned int color = RED;

			if (row % 2 == 0)
			{
				color = BLUE;
			}

			Rectangle actualBrick = {(float)positionX, (float)positionY, 35, 8, color, false, brickPoints};

			bricks.push_back(actualBrick);
			positionX += 37;
		}

		brickPoints--;
		positionY += 10;
	}

	return bricks;
}

std::vector<Rectangle> bricks = createBricks();

void update()
{
	int keyHeld = keysHeld();

	if (keyHeld & KEY_LEFT && player.x > 0)
	{
		player.x -= PLAYER_SPEED;
	}

	else if (keyHeld & KEY_RIGHT && player.x < SCREEN_WIDTH - player.w)
	{
		player.x += PLAYER_SPEED;
	}

	if (ball.y > SCREEN_HEIGHT + ball.h)
	{
		ball.x = HALF_WIDTH;
		ball.y = HALF_HEIGHT;

		ballVelocityX *= -1;

		if (playerLives > 0)
		{
			playerLives--;
		}
	}

	if (ball.x < 0 || ball.x > SCREEN_WIDTH - ball.w)
	{
		ballVelocityX *= -1;
		mmEffectEx(&collisionSound);
	}

	else if (hasCollision(player, ball) || ball.y < 0)
	{
		ballVelocityY *= -1;
		mmEffectEx(&collisionSound);
	}

	for (Rectangle &brick : bricks)
	{
		if (!brick.isDestroyed && hasCollision(brick, ball))
		{
			mmEffectEx(&collisionSound);

			ballVelocityY *= -1;
			brick.isDestroyed = true;
			playerScore += brick.brickPoints;

			break;
		}
	}

	ball.x += ballVelocityX;
	ball.y += ballVelocityY;
}

void renderTopScreen()
{
	lcdMainOnBottom();
	vramSetBankC(VRAM_C_LCD);
	vramSetBankD(VRAM_D_SUB_SPRITE);
	REG_DISPCAPCNT = DCAP_BANK(2) | DCAP_ENABLE | DCAP_SIZE(3);

	glBegin2D();

	glColor(RGB15(0, 31, 31));

	std::string scoreString = "SCORE: " + std::to_string(playerScore);

	Font.Print(20, SCREEN_HEIGHT - 16, scoreString.c_str());

	std::string livesString = "LIVES: " + std::to_string(playerLives);

	Font.Print(HALF_WIDTH + 40, SCREEN_HEIGHT - 16, livesString.c_str());

	if (isGamePaused)
	{
		Font.PrintCentered(0, HALF_HEIGHT, "GAME PAUSED");
	}

	glEnd2D();
}

void renderBottomScreen()
{
	lcdMainOnTop();
	vramSetBankD(VRAM_D_LCD);
	vramSetBankC(VRAM_C_SUB_BG);
	REG_DISPCAPCNT = DCAP_BANK(3) | DCAP_ENABLE | DCAP_SIZE(3);
	
	glBegin2D();

	for (Rectangle &brick : bricks)
	{
		if (!brick.isDestroyed)
		{
			drawRectangle(brick);
		}
	}

	drawRectangle(player);
	drawRectangle(ball);

	glEnd2D();
}

int main(int argc, char *argv[])
{
	videoSetMode(MODE_5_3D);
	videoSetModeSub(MODE_5_2D);

	initSubSprites();
	mmInitDefaultMem((mm_addr)soundbank_bin);

	bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

	glScreen2D();

	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankE(VRAM_E_TEX_PALETTE);

	Font.Load(FontImages,
			  FONT_SI_NUM_IMAGES,
			  font_si_texcoords,
			  GL_RGB256,
			  TEXTURE_SIZE_64,
			  TEXTURE_SIZE_128,
			  GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT,
			  256,
			  (u16 *)font_siPal,
			  (u8 *)font_siBitmap);

	FontBig.Load(FontBigImages,
				 FONT_16X16_NUM_IMAGES,
				 font_16x16_texcoords,
				 GL_RGB256,
				 TEXTURE_SIZE_64,
				 TEXTURE_SIZE_512,
				 GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT,
				 256,
				 (u16 *)font_siPal,
				 (u8 *)font_16x16Bitmap);

	mmLoad(MOD_FLATOUTLIES);

	mmStart(MOD_FLATOUTLIES, MM_PLAY_LOOP);

	mmSetModuleVolume(200);

	mmLoadEffect(SFX_MAGIC);

	collisionSound = {
		{SFX_MAGIC},
		(int)(1.0f * (1 << 10)),
		0,
		75,
		255,
	};

	int frame = 0;

	touchPosition touch;

	while (true)
	{
		frame++;

		scanKeys();

		touchRead(&touch);

		if (touch.px > 0 && touch.py > 0 && touch.px < SCREEN_WIDTH - player.w)
		{
			player.x = touch.px;
		}

		int keyDown = keysDown();

		if (keyDown & KEY_START)
		{
			isGamePaused = !isGamePaused;

			mmEffectEx(&collisionSound);
		}

		if (keyDown & KEY_A)
		{
			isAutoPlayMode = !isAutoPlayMode;
		}

		if (isAutoPlayMode && ball.x < SCREEN_WIDTH - player.w)
		{
			player.x = ball.x;
		}

		if (!isGamePaused)
		{
			update();
		}

		while (REG_DISPCAPCNT & DCAP_ENABLE);

		if ((frame & 1) == 0)
		{
			renderBottomScreen();

		}
		else
		{
			renderTopScreen();
		}

		glFlush(0);
		swiWaitForVBlank();
	}
}