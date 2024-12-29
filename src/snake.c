#include "console.h"
#include "timer.h"
#include "keyboard.h"

#define WIDTH 20
#define HEIGHT 20
#define SNAKE_MAX_LENGTH 100

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point body[SNAKE_MAX_LENGTH];
    int length;
    Point direction;
} Snake;

static Snake snake;
static Point food;
static int game_over;

static unsigned int seed = 123456789;

void srand(unsigned int new_seed) {
    seed = new_seed;
}

int rand() {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

void snake_init() {
    snake.length = 1;
    snake.body[0].x = WIDTH / 2;
    snake.body[0].y = HEIGHT / 2;
    snake.direction.x = 1;
    snake.direction.y = 0;
    food.x = rand() % WIDTH;
    food.y = rand() % HEIGHT;
    game_over = 0;
}

void snake_draw() {
    console_clear(COLOR_WHITE, COLOR_BLACK);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int is_snake = 0;
            for (int i = 0; i < snake.length; i++) {
                if (snake.body[i].x == x && snake.body[i].y == y) {
                    is_snake = 1;
                    break;
                }
            }
            if (is_snake) {
                console_putchar('O');
            } else if (food.x == x && food.y == y) {
                console_putchar('X');
            } else {
                console_putchar(' ');
            }
        }
        console_putchar('\n');
    }
}

void snake_update() {
    if (game_over) return;

    Point new_head = {
        .x = snake.body[0].x + snake.direction.x,
        .y = snake.body[0].y + snake.direction.y
    };

    if (new_head.x < 0 || new_head.x >= WIDTH || new_head.y < 0 || new_head.y >= HEIGHT) {
        game_over = 1;
        return;
    }

    for (int i = 0; i < snake.length; i++) {
        if (snake.body[i].x == new_head.x && snake.body[i].y == new_head.y) {
            game_over = 1;
            return;
        }
    }

    for (int i = snake.length; i > 0; i--) {
        snake.body[i] = snake.body[i - 1];
    }

    snake.body[0] = new_head;

    if (new_head.x == food.x && new_head.y == food.y) {
        snake.length++;
        food.x = rand() % WIDTH;
        food.y = rand() % HEIGHT;
    }
}

void snake_handle_input(char key) {
    switch (key) {
        case 'w':
            if (snake.direction.y == 0) {
                snake.direction.x = 0;
                snake.direction.y = -1;
            }
            break;
        case 's':
            if (snake.direction.y == 0) {
                snake.direction.x = 0;
                snake.direction.y = 1;
            }
            break;
        case 'a':
            if (snake.direction.x == 0) {
                snake.direction.x = -1;
                snake.direction.y = 0;
            }
            break;
        case 'd':
            if (snake.direction.x == 0) {
                snake.direction.x = 1;
                snake.direction.y = 0;
            }
            break;
    }
}

void snake_game() {
    snake_init();
    while (!game_over) {
        snake_draw();
        snake_update();
        char key = kb_getchar();
        snake_handle_input(key);
        // sleep(0.1);
    }
    console_clear(COLOR_WHITE, COLOR_BLACK);
    printf("Game Over!\n");
}