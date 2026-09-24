#include <M5Unified.h>
#include <bitset>
#include <array>

constexpr int screenHeight = 240;
constexpr int screenWidth = 320;

constexpr int pixelsPerTile = 2;
constexpr int boardWidth = screenWidth / pixelsPerTile;
constexpr int boardHeight = screenHeight / pixelsPerTile;
constexpr int totalTiles = boardWidth * boardHeight;

constexpr uint16_t colorDead = TFT_BLACK;
constexpr uint16_t colorAlive = TFT_ORANGE;
constexpr uint16_t colorCursor = TFT_RED;

constexpr unsigned long debounceInterval = 35;
constexpr unsigned long simInterval = 100;

constexpr bool enableDualCore = true;

// game state
int cursorX = boardWidth / 2;
int cursorY = boardHeight / 2;

using Board = std::array<std::bitset<boardWidth>, boardHeight>;

Board boards[3];

Board& boardInit = boards[0];
Board* boardCurr = &boards[1];
Board* boardPrev = &boards[2];

unsigned long lastButtonTime = 0;
uint8_t gamepadStatePrev = 0xFF;

unsigned long lastSimTime = 0;
bool runSim = false;
int simStepCount = 0;

enum GamepadButton : uint8_t 
{
  Right  = 3,
  Left   = 2,
  Down   = 1,
  Up     = 0,
  Start  = 7,
  Select = 6,
  B      = 5,
  A      = 4,
};

constexpr uint8_t gamepadAddress = 0x08;

uint8_t readGamepad() 
{
  uint8_t state = 0xFF;

  if (M5.In_I2C.start(gamepadAddress, true, 100000))
  {
    M5.In_I2C.read(&state, 1, true);
    M5.In_I2C.stop();
  }

  return state;
}


bool isPressed(uint8_t gamepadState, GamepadButton button)
{
  // these are "active low", 0 is pressed 1 is not pressed
  return !(gamepadState & (1 << button));
}

bool buttonPress(uint8_t gamepadState, GamepadButton button)
{
  if (isPressed(gamepadState, button) && !isPressed(gamepadStatePrev, button))
  {
    lastButtonTime = millis();
    return true;
  }
  return false;
}

static void updateCursor(int moveX, int moveY, bool toggleTile)
{
  // clear the prev spot
  int pixelX = cursorX * pixelsPerTile;
  int pixelY = cursorY * pixelsPerTile;

  if (boardInit[cursorY][cursorX])
    M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, colorAlive);
  else
    M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, colorDead);

  cursorX += moveX;
  cursorY += moveY;

  if (cursorX < 0) cursorX = boardWidth - 1;
  if (cursorX > (boardWidth - 1)) cursorX = 0;
  if (cursorY < 0) cursorY = boardHeight - 1;
  if (cursorY > (boardHeight - 1)) cursorY = 0;

  pixelX = cursorX * pixelsPerTile;
  pixelY = cursorY * pixelsPerTile;

  if (toggleTile)
    boardInit[cursorY][cursorX] = !boardInit[cursorY][cursorX];

  uint16_t tileColor = boardInit[cursorY][cursorX] ? colorAlive: colorDead;

  M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
  M5.Display.drawRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, colorCursor);
}

static void clearCursor()
{
  int pixelX = cursorX * pixelsPerTile;
  int pixelY = cursorY * pixelsPerTile;
  uint16_t tileColor = boardInit[cursorY][cursorX] ? colorAlive: colorDead;
  M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
}

void setup() {
  // put your setup code here, to run once:

  M5.begin();
  M5.Display.fillScreen(TFT_BLACK);

  updateCursor(0, 0, false);
}

static int getAliveNeighborCount(Board& board, int x, int y)
{
  int left = x == 0 ? boardWidth - 1 : x - 1;
  int right = x == (boardWidth - 1) ? 0 : x + 1;
  int top = y == 0 ? boardHeight - 1 : y - 1;
  int bottom = y == (boardHeight - 1) ? 0 : y + 1;

  int total = 0;

  // top
  total += board[top][left];
  total += board[top][x];
  total += board[top][right];

  // mid
  total += board[y][left];
  total += board[y][right];

  // bottom
  total += board[bottom][left];
  total += board[bottom][x];
  total += board[bottom][right];

  return total;
}

void simStep()
{
  M5.Display.startWrite();
  for (int y = 0; y < boardHeight; y++)
  {
    for (int x = 0; x < boardWidth; x++)
    {
      int aliveNeighborCount = getAliveNeighborCount(*boardPrev, x, y);
      bool prevCellAlive = (*boardPrev)[y][x];
      bool currCellAlive = prevCellAlive;

      // Conway's Game of Life
      if (prevCellAlive)
      {
        if (aliveNeighborCount < 2)
          currCellAlive = false;
        else if (aliveNeighborCount > 3)
          currCellAlive = false;
      }
      else
      {
        if (aliveNeighborCount == 3)
        {
          currCellAlive = true;
        }
      }

      (*boardCurr)[y][x] = currCellAlive;
    
      // draw it
      if (currCellAlive != prevCellAlive)
      {
        int pixelX = x * pixelsPerTile;
        int pixelY = y * pixelsPerTile;
        uint16_t tileColor = currCellAlive ? colorAlive: colorDead;
        M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
      }
    }
  }

  M5.Display.endWrite();
}

void simTask(int start, int end)
{
  for (int y = start; y < end; y++)
  {
    for (int x = 0; x < boardWidth; x++)
    {
      int aliveNeighborCount = getAliveNeighborCount(*boardPrev, x, y);
      bool prevCellAlive = (*boardPrev)[y][x];
      bool currCellAlive = prevCellAlive;

      // Conway's Game of Life
      if (prevCellAlive)
      {
        if (aliveNeighborCount < 2)
          currCellAlive = false;
        else if (aliveNeighborCount > 3)
          currCellAlive = false;
      }
      else
      {
        if (aliveNeighborCount == 3)
        {
          currCellAlive = true;
        }
      }

      (*boardCurr)[y][x] = currCellAlive;
    }
  }
}

void simTaskTop(void* arg) {
    TaskHandle_t caller = static_cast<TaskHandle_t>(arg);

    simTask(0, boardHeight / 2);

    xTaskNotifyGive(caller);
    vTaskDelete(nullptr);
}

void simTaskBottom(void* arg) {
    TaskHandle_t caller = static_cast<TaskHandle_t>(arg);

    simTask(boardHeight / 2, boardHeight);

    xTaskNotifyGive(caller);
    vTaskDelete(nullptr);
}

void drawBoardCurr()
{
  M5.Display.startWrite();
  for (int y = 0; y < boardHeight; y++)
  {
    for (int x = 0; x < boardWidth; x++)
    {
      bool prevCellAlive = (*boardPrev)[y][x];
      bool currCellAlive = (*boardCurr)[y][x];

      // draw it
      if (currCellAlive != prevCellAlive)
      {
        int pixelX = x * pixelsPerTile;
        int pixelY = y * pixelsPerTile;
        uint16_t tileColor = currCellAlive ? colorAlive: colorDead;
        M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
      }
    }
  }

  M5.Display.endWrite();
}

void randomBoard()
{
  for (int y = 0; y < boardHeight; ++y) {
    for (int x = 0; x < boardWidth; ++x) {
      boardInit[y][x] = random(100) < 30;

      int pixelX = x * pixelsPerTile;
      int pixelY = y * pixelsPerTile;
      uint16_t tileColor = boardInit[y][x] ? colorAlive: colorDead;
      M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
    }
  }

  updateCursor(0, 0, false);
}

void reset()
{
  simStepCount = 0;
  
  for (int y = 0; y < boardHeight; y++)
  {
    for (int x = 0; x < boardWidth; x++)
    {
      bool alive = boardInit[y][x];
      int pixelX = x * pixelsPerTile;
      int pixelY = y * pixelsPerTile;
      uint16_t tileColor = alive ? colorAlive: colorDead;
      M5.Display.fillRect(pixelX, pixelY, pixelsPerTile, pixelsPerTile, tileColor);
    }
  }

  updateCursor(0, 0, false);
}

void clearBoard(Board& board)
{
  for (int i = 0; i < boardHeight; i++)
  {
    board[i].reset();
  }
}

void copyBoard(Board& src, Board& dst)
{
  for (int i = 0; i < boardHeight; i++)
  {
    for (int j = 0; j < boardWidth; j++)
    {
      dst[i][j] = src[i][j];
    }
  }
}

void loop() {

  uint8_t gamepadState = readGamepad();

  bool buttonPressAllowed = false;
  unsigned long now = millis();

  if ((now - lastButtonTime) > debounceInterval)
  {
    buttonPressAllowed = true;
  }

  if (buttonPressAllowed)
  {
    int moveX = 0;
    int moveY = 0;
    bool toggleTile = false;

    if (buttonPress(gamepadState, GamepadButton::Left))
    {
      moveX--;
    }
    if (buttonPress(gamepadState, GamepadButton::Right))
    {
      moveX++;
    }
    if (buttonPress(gamepadState, GamepadButton::Up))
    {
      moveY--;
    }
    if (buttonPress(gamepadState, GamepadButton::Down))
    {
      moveY++;
    }
    if (buttonPress(gamepadState, GamepadButton::A))
    {
      toggleTile = true;
    }
    if (buttonPress(gamepadState, GamepadButton::Start))
    {
      if (runSim)
      {
        reset();
        runSim = false;
      }
      else
      {
        clearCursor();
        runSim = true;
      }
    }
    if (buttonPress(gamepadState, GamepadButton::Select))
    {
      if (!runSim)
        randomBoard();
    }

    if (!runSim && (moveX != 0 || moveY != 0 || toggleTile))
      updateCursor(moveX, moveY, toggleTile);
  }

  gamepadStatePrev = gamepadState;

  //bool canRunSimStep = (now - lastSimTime) > simInterval;
  bool canRunSimStep = true;

  if (runSim && canRunSimStep)
  {
    if (simStepCount == 0)
    {
      copyBoard(boardInit, *boardPrev);
      clearBoard(*boardCurr);
    }

    if (enableDualCore)
    {
      TaskHandle_t caller = xTaskGetCurrentTaskHandle();

      BaseType_t top = xTaskCreatePinnedToCore(simTaskTop, "taskTop", 4096, caller, 1, nullptr, 0);
      BaseType_t bottom = xTaskCreatePinnedToCore(simTaskBottom, "taskBottom", 4096, caller, 1, nullptr, 1);

      configASSERT(top == pdPASS && bottom == pdPASS);

      ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
      ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

      drawBoardCurr();
    }
    else
    {
      simStep();
    }
   
    Board* temp = boardCurr;
    boardCurr = boardPrev;
    boardPrev = temp;

    lastSimTime = now;
    simStepCount++;
  }
}
