#include "pong.h"
#include "console.h"
#include "keyboard.h"
#include "vesa.h"
#include "timer.h"
#include "liballoc.h"
#include "string.h"

extern uint32_t g_width;
extern uint32_t g_height;

#define PADDLE_WIDTH  4
#define PADDLE_HEIGHT 40
#define BALL_SIZE     5
#define PADDLE_SPEED  10
#define BALL_SPEED    5

typedef struct {
    uint32_t x, y;
} Point;

static uint32_t left_paddle_y, right_paddle_y;
static Point ball;
static int ball_vel_x, ball_vel_y;

static void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) {
        for (uint32_t j = 0; j < h; j++) {
            uint32_t px = x + i;
            uint32_t py = y + j;
            if (px < g_width && py < g_height)
                vbe_putpixel(px, py, color);
        }
    }
}

static void draw_game() {
    console_clear();
    draw_rect(10, left_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0xFFFFFF);
    draw_rect(g_width - 10 - PADDLE_WIDTH, right_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0xFFFFFF);
    draw_rect(ball.x, ball.y, BALL_SIZE, BALL_SIZE, 0xFFFFFF);
    vesa_swap_buffers();
}

void pong_game() {
    left_paddle_y = (g_height - PADDLE_HEIGHT) / 2;
    right_paddle_y = left_paddle_y;
    ball.x = g_width / 2;
    ball.y = g_height / 2;
    ball_vel_x = BALL_SPEED;
    ball_vel_y = BALL_SPEED;
    
    int running = 1;
    while (running) {
        if (kbhit()) {
            char c = kb_getchar();
            if (c == 'w' && left_paddle_y > 0) {
                left_paddle_y = (left_paddle_y > PADDLE_SPEED) ? left_paddle_y - PADDLE_SPEED : 0;
            } else if (c == 's' && left_paddle_y < g_height - PADDLE_HEIGHT) {
                left_paddle_y += PADDLE_SPEED;
            } else if (c == 'q') {
                running = 0;
            }
        }
        
        if (ball.y < right_paddle_y + (PADDLE_HEIGHT / 2) && right_paddle_y > 0) {
            right_paddle_y -= PADDLE_SPEED;
        } else if (ball.y > right_paddle_y + (PADDLE_HEIGHT / 2) && right_paddle_y < g_height - PADDLE_HEIGHT) {
            right_paddle_y += PADDLE_SPEED;
        }
        
        ball.x += ball_vel_x;
        ball.y += ball_vel_y;
        
        if (ball.y == 0 || ball.y >= g_height - BALL_SIZE) {
            ball_vel_y = -ball_vel_y;
        }
        
        if (ball.x <= 10 + PADDLE_WIDTH &&
            ball.y + BALL_SIZE >= left_paddle_y &&
            ball.y <= left_paddle_y + PADDLE_HEIGHT) {
            ball_vel_x = -ball_vel_x;
            ball.x = 10 + PADDLE_WIDTH;
        }
        
        if (ball.x + BALL_SIZE >= g_width - 10 - PADDLE_WIDTH &&
            ball.y + BALL_SIZE >= right_paddle_y &&
            ball.y <= right_paddle_y + PADDLE_HEIGHT) {
            ball_vel_x = -ball_vel_x;
            ball.x = g_width - 10 - PADDLE_WIDTH - BALL_SIZE;
        }
        
        if (ball.x >= g_width || ball.x == 0) {
            ball.x = g_width / 2;
            ball.y = g_height / 2;
            ball_vel_x = (rand() % 2) ? BALL_SPEED : -BALL_SPEED;
            ball_vel_y = (rand() % 2) ? BALL_SPEED : -BALL_SPEED;
        }
        
        draw_game();
    }
}
