#include <Arduino.h>
#include <M5Unified.h>
#include <bitset>
#include <array>

constexpr int screenHeight = 240;
constexpr int screenWidth = 320;

constexpr int pixelsPerTile = 2;
constexpr int boardWidth = screenWidth / pixelsPerTile;
constexpr int boardHeight = screenHeight / pixelsPerTile;

constexpr uint16_t colorSand = TFT_YELLOW;
constexpr uint16_t colorEmpty = TFT_BLACK;

using Board = std::array<std::bitset<boardWidth>, boardHeight>;

Board boards[1];

Board& board = boards[0];

bool traverseDir = false;

// cube the size of 1/3 the screen
void initBoard()
{
  M5.Display.startWrite();
  for (int y = 0; y < boardHeight; y++)
  {
    for (int x = 0; x < boardWidth; x++)
    {
        if (y > (boardHeight / 3) && y < (boardHeight * 2 / 3) && 
        x > (boardWidth / 3) && x < (boardWidth * 2 / 3))
        {
          board[y][x] = 1;
          int pixelX = x * pixelsPerTile;
          int pixelY = y * pixelsPerTile;

          M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, colorSand);
        }
        
    }
  }

  M5.Display.endWrite();
}

void setup()
{
  M5.begin();
  M5.Display.fillScreen(TFT_BLACK);
  initBoard();
}

void simStep()
{
  M5.Display.startWrite();

  // skip the bottom row since those ones can't move
  for (int y = (boardHeight - 2); y >= 0; y--)
  {
    int startX = traverseDir ? 0 : boardWidth - 1;
    int endX = traverseDir ? boardWidth : -1;
    int increment = traverseDir ? 1 : -1;

    traverseDir = !traverseDir;

    for (int x = startX; x != endX; x = x + increment)
    {
      if (!board[y][x])
        continue; // empty cell

      bool belowEmpty = !board[y + 1][x];

      int newX = x;
      int newY = y;

      if (belowEmpty)
      {
        newX = x;
        newY = y + 1;
      }
      else
      {
        // flip a coin to select left vs right
        bool left = random(2);

        bool canGoLeft = x > 0 && !board[y + 1][x - 1];
        bool canGoRight = x < (boardWidth - 1) && !board[y + 1][x + 1];

        if (left)
        {
          if (canGoLeft)
          {
            newX = x - 1;
            newY = y + 1;
          }
          else if (canGoRight)
          {
            newX = x + 1;
            newY = y + 1;       
          }
        }
        else
        {
          if (canGoRight)
          {
            newX = x + 1;
            newY = y + 1;
          }
          else if (canGoLeft)
          {
            newX = x - 1;
            newY = y + 1;
          }
        }
      }

      bool changed = newX != x || newY != y;

      if (changed)
      {
        board[y][x] = 0;
        board[newY][newX] = 1;

        int pixelX = x * pixelsPerTile;
        int pixelY = y * pixelsPerTile;

        int newPixelX = newX * pixelsPerTile;
        int newPixelY = newY * pixelsPerTile;

        M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, colorEmpty);
        M5.Display.fillRect(newPixelX, newPixelY, pixelsPerTile, pixelsPerTile, colorSand);
      }
    }
  }

  M5.Display.endWrite();
}

void loop() {
  simStep();
}