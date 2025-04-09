#include "pong.h"
#include "console.h"
#include "keyboard.h"
#include "vesa.h"
#include "timer.h"
#include "liballoc.h"
#include "string.h"
#include "math.h"

extern uint32_t g_width;
extern uint32_t g_height;

#define PADDLE_WIDTH 4
#define PADDLE_HEIGHT 40
#define BALL_SIZE 5
#define PADDLE_SPEED 10
#define FRAME_DELAY 15
#define BORDER_SIZE 2

typedef struct
{
    uint32_t x, y;
} Point;

static uint32_t left_paddle_y, right_paddle_y;
static Point ball;
static int ball_vel_x, ball_vel_y;
static int player_score = 0;
static int ai_score = 0;
static int ball_base_speed = 5;
static float ball_speed_multiplier = 1.0;

static void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t i = 0; i < w; i++)
    {
        for (uint32_t j = 0; j < h; j++)
        {
            uint32_t px = x + i;
            uint32_t py = y + j;
            if (px < g_width && py < g_height)
                vbe_putpixel(px, py, color);
        }
    }
}

static void draw_game()
{
    // Clear screen
    draw_rect(0, 0, g_width, g_height, 0x000000);

    // Draw borders
    draw_rect(0, 0, g_width, BORDER_SIZE, 0xFFFFFF);                      // Top
    draw_rect(0, g_height - BORDER_SIZE, g_width, BORDER_SIZE, 0xFFFFFF); // Bottom
    draw_rect(0, 0, BORDER_SIZE, g_height, 0xFFFFFF);                     // Left
    draw_rect(g_width - BORDER_SIZE, 0, BORDER_SIZE, g_height, 0xFFFFFF); // Right

    // Draw paddles and ball
    draw_rect(10, left_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0xFFFFFF);
    draw_rect(g_width - 10 - PADDLE_WIDTH, right_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0xFFFFFF);
    draw_rect(ball.x, ball.y, BALL_SIZE, BALL_SIZE, 0xFFFFFF);

    // console_printf("Player: %d  AI: %d", player_score, ai_score);

    vesa_swap_buffers();
}

void pong_game()
{
    left_paddle_y = (g_height - PADDLE_HEIGHT) / 2;
    right_paddle_y = left_paddle_y;
    ball.x = g_width / 2;
    ball.y = g_height / 2;
    ball_vel_x = ball_base_speed;
    ball_vel_y = ball_base_speed;

    int running = 1;
    while (running)
    {
        if (kbhit())
        {
            char c = kb_getchar();
            if (c == 'w')
            {
                // Clamped upward movement
                left_paddle_y = (left_paddle_y > PADDLE_SPEED) ? left_paddle_y - PADDLE_SPEED : 0;
            }
            else if (c == 's')
            {
                // Clamped downward movement
                uint32_t new_y = left_paddle_y + PADDLE_SPEED;
                left_paddle_y = (new_y < g_height - PADDLE_HEIGHT) ? new_y : g_height - PADDLE_HEIGHT;
            }
            else if (c == 'q')
            {
                console_clear();
                console_printf("Exiting Pong game...\n");
                console_printf("Press any key to continue...\n");
                while (!kbhit())
                {
                    usleep(10000);
                }
                kb_getchar();
                console_clear();
                running = 0;
                ball_speed_multiplier = 1.0;
                ball_base_speed = 5;
                return;
            }
        }

        // AI movement (clamped)
        uint32_t paddle_center = right_paddle_y + PADDLE_HEIGHT / 2;
        uint32_t ball_center = ball.y + BALL_SIZE / 2;

        if (ball_center < paddle_center - 5)
        {
            right_paddle_y = (right_paddle_y > PADDLE_SPEED) ? right_paddle_y - PADDLE_SPEED : 0;
        }
        else if (ball_center > paddle_center + 5)
        {
            uint32_t new_y = right_paddle_y + PADDLE_SPEED;
            right_paddle_y = (new_y < g_height - PADDLE_HEIGHT) ? new_y : g_height - PADDLE_HEIGHT;
        }

        // Ball movement
        ball.x += ball_vel_x * ball_speed_multiplier;
        ball.y += ball_vel_y * ball_speed_multiplier;

        // Vertical collision
        if (ball.y <= BORDER_SIZE || ball.y + BALL_SIZE >= g_height - BORDER_SIZE)
        {
            ball_vel_y = -ball_vel_y;
            ball.y += ball_vel_y; // Prevent sticking
        }

        // Paddle collision
        if (ball.x <= 10 + PADDLE_WIDTH &&
            ball.y < left_paddle_y + PADDLE_HEIGHT &&
            ball.y + BALL_SIZE > left_paddle_y)
        {
            ball_vel_x = abs(ball_vel_x);
            ball.x = 10 + PADDLE_WIDTH + 1;
            ball_speed_multiplier *= 1.05;
            int hit_offset = (ball.y + BALL_SIZE / 2) - (left_paddle_y + PADDLE_HEIGHT / 2);
            ball_vel_y = hit_offset / 3;
        }

        if (ball.x + BALL_SIZE >= g_width - 10 - PADDLE_WIDTH &&
            ball.y < right_paddle_y + PADDLE_HEIGHT &&
            ball.y + BALL_SIZE > right_paddle_y)
        {
            ball_vel_x = -abs(ball_vel_x);
            ball.x = g_width - 10 - PADDLE_WIDTH - BALL_SIZE - 1;
            ball_speed_multiplier *= 1.05;
            int hit_offset = (ball.y + BALL_SIZE / 2) - (right_paddle_y + PADDLE_HEIGHT / 2);
            ball_vel_y = hit_offset / 3;
        }

        // // Speed clamping
        if (abs(ball_vel_y) > ball_base_speed * 2)
        {
            ball_vel_y = (ball_vel_y > 0) ? ball_base_speed * 2 : -ball_base_speed * 2;
        }

        draw_game();
        usleep(FRAME_DELAY);
    }
}