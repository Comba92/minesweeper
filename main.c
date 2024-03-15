#include <raylib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

// 4:3 = 640x480, 800x600, 1024x768 -  16:9 = 1280x720, 1920x1080

const int CELL_SIZE = 30;

static const int NEIGHBOURS[8][2] = {
  // DIRECTIONS
  {0, -1}, {0, 1}, {-1, 0}, {1, 0},
  // DIAGONALS
  {-1, -1}, {1, -1}, {-1, 1}, {1,  1}
};

typedef enum {
  EASY = 0, MEDIUM = 1, HARD = 2
} Difficulties;

typedef struct {
  int width, height, mines;
} Setup;
static const Setup SETUPS[3] = {
  (Setup) {.width = 12, .height = 12, .mines = 20},
  (Setup) {.width = 16, .height = 16, .mines = 40},
  (Setup) {.width = 30, .height = 16, .mines = 99},
};

typedef enum {
  EMPTY,
  MINE
} CellType;

typedef enum {
  HIDDEN,
  VISIBLE,
  FLAGGED
} CellState;

typedef enum {
  RUNNING, WON, LOST
} GameState;

typedef struct {
  CellType type;
  CellState state;
  unsigned int mines;
} Cell;

typedef struct {
  Cell* cells;
  int width;
  int height;
  int mines;
} Grid;

Cell newCell(CellType t) {
  return (Cell) { .type = t, .state = HIDDEN, .mines = 0 };
}

Grid newGrid(Setup setup) {
  Grid grid;
  grid.width = setup.width;
  grid.height = setup.height;
  grid.mines = setup.mines;
  grid.cells = (Cell*) malloc(grid.width * grid.height * sizeof(Cell));

  return grid;
}

void deleteGrid(Grid grid) {
  free(grid.cells);
}

int getIndex(Grid grid, int x, int y) {
  return (x < 0 || x >= grid.width || y < 0 || y >= grid.height)
    ? -1 
    : grid.width * y + x;
}

Cell* getCell(Grid grid, int x, int y) {
  int idx = getIndex(grid, x, y);
  if (idx == -1) return NULL;
  return &grid.cells[idx];
}

unsigned int countCellMinesClose(Grid grid, int x, int y) {
  unsigned int count = 0;
  for (int n=0; n<8; n++) {
    Cell* cell = getCell(grid, x + NEIGHBOURS[n][0], y + NEIGHBOURS[n][1]);
    if (cell && cell->type == MINE) count++;
  }

  return count;
}

void computeCells(Grid grid) {
   for (int y = 0; y < grid.height; y++) {
    for (int x = 0; x < grid.width; x++) {
      Cell* cell = getCell(grid, x, y);

      if (cell->type != MINE) {
        cell->mines = countCellMinesClose(grid, x, y);
      }
    }
   }
}

void initRandom(Grid grid) {
  srand(time(NULL));

  for (int y = 0; y < grid.height; y++) {
    for (int x = 0; x < grid.width; x++) {
      Cell* cell = getCell(grid, x, y);
      *cell = newCell(EMPTY);
    }
  }

  for(int m=0; m < grid.mines; m++) {
    do {
      int idx = rand() % (grid.width * grid.height + 1);

      if ( grid.cells[idx].type == EMPTY ) {
        grid.cells[idx] = newCell(MINE);
        break;
      }
    } while(true);
  }

  computeCells(grid);
}

void drawGrid(Grid grid) {
  char buff[4];
  const int offset = (int) CELL_SIZE / 5;

  for (int y = 0; y < grid.height; y++) {
    for (int x = 0; x < grid.width; x++) {
      Cell* cell = getCell(grid, x, y);

      switch (cell->state) {
        case VISIBLE: {
          if (cell->type == MINE) {
            DrawText("B", x * CELL_SIZE + offset, y * CELL_SIZE + offset, 20, RED);
          }
          else {
            DrawText(
              itoa(cell->mines, buff, 10), 
              x * CELL_SIZE + offset,
              y * CELL_SIZE + offset,
              20, WHITE
            );
          }
        } break;

        case FLAGGED:
          DrawText("F", x * CELL_SIZE + offset, y * CELL_SIZE + offset, 20, ORANGE);
          break;
        default: {}
      }
    }
  }
}

Vector2 mouseScreenPositionToWorldPosition() {
  Vector2 m = GetMousePosition();

  return (Vector2) {
    .x = (int)m.x / CELL_SIZE * CELL_SIZE, 
    .y = (int)m.y / CELL_SIZE * CELL_SIZE,
  };
}

void checkDFS(Grid grid, int x, int y) {
  Cell* curr = getCell(grid, x, y);

  curr->state = VISIBLE;

  for(int n=0; n<4; n++) {
    int nx = x +  NEIGHBOURS[n][0];
    int ny = y + NEIGHBOURS[n][1];
    Cell* cell = getCell(grid, nx, ny);
    if (!cell) continue;

    if (cell->state != VISIBLE && cell->mines == 0) checkDFS(grid, nx, ny);
  }

  for(int n=0; n<8; n++) {
    int nx = x + NEIGHBOURS[n][0];
    int ny = y + NEIGHBOURS[n][1];
    Cell* cell = getCell(grid, nx, ny);
    if (!cell) continue;
    
    if (cell->mines > 0) cell->state = VISIBLE;
  }
}

void checkCell(Grid grid, Vector2 mworld) {
  int x = (int)(mworld.x / CELL_SIZE);
  int y = (int)(mworld.y / CELL_SIZE);

  Cell* cell = getCell(grid, x, y);
  if (!cell) return;

  switch (cell->type) {
    case EMPTY: {
      if (cell->state != VISIBLE) {
          if (cell->mines == 0) checkDFS(grid, x, y);
          else cell->state = VISIBLE;
        }
    } break;

    case MINE: {
      cell->state = VISIBLE;
    } break;
  }
}

void flagCell(Grid grid, Vector2 mworld) {
  Cell* cell = getCell(grid, (int)(mworld.x / CELL_SIZE), (int)(mworld.y / CELL_SIZE));
  if (!cell) return;

  switch(cell->state) {
    case HIDDEN:
      cell->state = FLAGGED;
      break;

    case FLAGGED:
      cell->state = HIDDEN;
      break;

    default: {}
  }
}

void showAllMines(Grid grid) {
  for (int y = 0; y < grid.height; y++) {
    for (int x = 0; x < grid.width; x++) {
      Cell* cell = getCell(grid, x, y);
      if (cell->type == MINE) cell->state = VISIBLE;
    }
  }
}

GameState checkWin(Grid grid) {
  int count = 0;
  char buff[16];

  for (int y = 0; y < grid.height; y++) {
    for (int x = 0; x < grid.width; x++) {
      Cell* cell = getCell(grid, x, y);
      if (!cell) continue;

      if (cell->type == MINE && cell->state == VISIBLE) {
        showAllMines(grid);
        return LOST;
      }

      count += cell->type == EMPTY && cell->state == VISIBLE ? 1 : 0;
    }
  }

  bool hasWon = count >= grid.width*grid.height - grid.mines;

  itoa(count, buff, 10);
  strcat(buff, " safe cells");
  DrawText(buff, 620, 480, 20, hasWon ? GREEN : WHITE);

  return hasWon ? WON : RUNNING;
}

void drawLines(Grid grid) {
  for (int y = 0; y <= grid.height; y++) {
    DrawLine(0, y * CELL_SIZE, grid.width * CELL_SIZE, y * CELL_SIZE, GRAY);
  }

    for (int x = 0; x <= grid.width; x++) {
    DrawLine(x * CELL_SIZE, 0, x * CELL_SIZE, grid.height * CELL_SIZE, GRAY);
  }
}

int main() {
  InitWindow(800, 600, "MINESWEEPER");
  SetTargetFPS(30);

  Grid grid = newGrid(SETUPS[MEDIUM]);
  initRandom(grid);

  int hasWon = RUNNING;
  char buff[16];

  while(!WindowShouldClose()) {
    BeginDrawing();
      ClearBackground(BLACK);
      drawGrid(grid);
      drawLines(grid);

      DrawText("Left click:\ncheck boxes", 620, 200, 20, WHITE);
      DrawText("Right click:\nset/unset flags", 620, 240, 20, WHITE);

      itoa(grid.mines, buff, 10);
      strcat(buff, " mines");
      DrawText(buff, 620, 460, 20, RED);

      hasWon = checkWin(grid);
      DrawText(hasWon == WON ? "YOU WON" : "", 620, 500, 20, GREEN);
      DrawText(hasWon == LOST ? "YOU LOST" : "", 620, 520, 20, RED);

      if (IsKeyPressed(KEY_R)) {
        hasWon = RUNNING;
        initRandom(grid);
      }

      if (hasWon != RUNNING) {
        DrawText("Press R\nto play again.", 620, 400, 20, WHITE);
      } else {
        Vector2 mworld = mouseScreenPositionToWorldPosition();
        DrawRectangleLines(mworld.x, mworld.y, CELL_SIZE, CELL_SIZE, RED);

        if (IsMouseButtonPressed(0)) {
          checkCell(grid, mworld);
        }

        if (IsMouseButtonPressed(1)) {
          flagCell(grid, mworld);
        }
      }

    EndDrawing();
  }

  deleteGrid(grid);
  CloseWindow();
}