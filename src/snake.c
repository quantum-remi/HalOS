// #include "console.h"
// #include "timer.h"
// #include "keyboard.h"

// #define WIDTH 20
// #define HEIGHT 20
// #define SNAKE_MAX_LENGTH 100

// typedef struct {
//     int x, y;
// } Point;

// typedef struct {
//     Point body[SNAKE_MAX_LENGTH];
//     int length;
//     Point direction;
// } Snake;

// static Snake snake;
// static Point food;
// static int game_over;
// static int score;

// static unsigned int seed = 123456789;

// void srand(unsigned int new_seed) {
//     seed = new_seed;
// }

// int rand() {
//     seed = seed * 1103515245 + 12345;
//     return (seed / 65536) % 32768;
// }

// void snake_init() {
//     snake.length = 1;
//     snake.body[0].x = WIDTH / 2;
//     snake.body[0].y = HEIGHT / 2;
//     snake.direction.x = 1;
//     snake.direction.y = 0;
//     food.x = rand() % WIDTH;
//     food.y = rand() % HEIGHT;
//     game_over = 0;
//     score = 0;
// }

// void snake_draw() {
//     console_clear(COLOR_WHITE, COLOR_BLACK);
//     for (int y = 0; y < HEIGHT; y++) {
//         for (int x = 0; x < WIDTH; x++) {
//             int is_snake = 0;
//             for (int i = 0; i < snake.length; i++) {
//                 if (snake.body[i].x == x && snake.body[i].y == y) {
//                     is_snake = 1;
//                     break;
//                 }
//             }
//             if (is_snake) {
//                 console_putchar('O');
//             } else if (food.x == x && food.y == y) {
//                 console_putchar('X');
//             } else {
//                 console_putchar(' ');
//             }
//         }
//         console_putchar('\n');
//     }
//     printf("Score: %d\n", score);
// }

// void snake_update() {
//     if (game_over) return;

//     Point new_head = {
//         .x = snake.body[0].x + snake.direction.x,
//         .y = snake.body[0].y + snake.direction.y
//     };

//     if (new_head.x < 0 || new_head.x >= WIDTH || new_head.y < 0 || new_head.y >= HEIGHT) {
//         game_over = 1;
//         return;
//     }

//     for (int i = 0; i < snake.length; i++) {
//         if (snake.body[i].x == new_head.x && snake.body[i].y == new_head.y) {
//             game_over = 1;
//             return;
//         }
//     }

//     for (int i = snake.length; i > 0; i--) {
//         snake.body[i] = snake.body[i - 1];
//     }

//     snake.body[0] = new_head;

//     if (new_head.x == food.x && new_head.y == food.y) {
//         snake.length++;
//         score++;
//         food.x = rand() % WIDTH;
//         food.y = rand() % HEIGHT;
//     }
// }

// void snake_handle_input(char key) {
//     switch (key) {
//         case 'w':
//             if (snake.direction.y == 0) {
//                 snake.direction.x = 0;
//                 snake.direction.y = -1;
//             }
//             break;
//         case 's':
//             if (snake.direction.y == 0) {
//                 snake.direction.x = 0;
//                 snake.direction.y = 1;
//             }
//             break;
//         case 'a':
//             if (snake.direction.x == 0) {
//                 snake.direction.x = -1;
//                 snake.direction.y = 0;
//             }
//             break;
//         case 'd':
//             if (snake.direction.x == 0) {
//                 snake.direction.x = 1;
//                 snake.direction.y = 0;
//             }
//             break;
//     }
// }

// int snake_collision() {
//     Point head = snake.body[0];

//     // Check for collision with walls
//     if (head.x < 0 || head.x >= WIDTH || head.y < 0 || head.y >= HEIGHT) {
//         return 1;
//     }

//     // Check for collision with itself
//     for (int i = 1; i < snake.length; i++) {
//         if (snake.body[i].x == head.x && snake.body[i].y == head.y) {
//             return 1;
//         }
//     }

//     return 0;
// }

// void snake_game() {
//     snake_init();
//     int update_counter = 0;
//     const int update_threshold = 25; // Adjust this value to control the game speed
//     const int sleep_time = 10000; // Define a constant for the sleep time

//     while (!game_over) {
//         // Check for input more frequently
//         if (kbhit()) {
//             snake_handle_input(kb_getchar());
//         }

//         // Update the game state at a fixed interval
//         if (update_counter >= update_threshold) {
//             snake_update();
//             snake_draw(); // Draw the game state after updating
//             update_counter = 0;
//         }

//         // Increment the counter
//         update_counter++;

//         // Add a small delay to reduce CPU usage
//         usleep(sleep_time); // Use the defined constant for sleep time

//         // Check for game over conditions (e.g., snake collision with wall or itself)
//         if (snake_collision()) {
//             game_over = 1;
//         }
//     }

//     console_clear(COLOR_WHITE, COLOR_BLACK);
//     printf("Game Over! Final Score: %d\n", score);
// }

#include "console.h" 
#include "timer.h"
#include "keyboard.h"
#include "vesa.h"
#include "vga.h"

static int g_start_x;
static int g_start_y;

// Game constants
#define CELL_SIZE 16  
#define BORDER_SIZE 2 
#define BOARD_WIDTH 25  
#define BOARD_HEIGHT 20 
#define SNAKE_MAX_LENGTH 100

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// Colors
#define COLOR_BORDER    VBE_RGB(128, 128, 128)
#define COLOR_SNAKE     VBE_RGB(0, 255, 0)    
#define COLOR_FOOD      VBE_RGB(255, 0, 0)     
#define COLOR_BG        VBE_RGB(0, 0, 0)      
#define COLOR_SCORE     VBE_RGB(255, 255, 255)

// Random number generator
static unsigned int seed = 123456789;
static int rand() {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

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
static int score;

// Function declarations
static void init_graphics(void);
static void draw_rect(int x, int y, int w, int h, uint32 color);
static void draw_border(void);
static void snake_init(void);
static void draw_game(void);
static void snake_update(void);
static void snake_handle_input(char key);
static int snake_collision(void);

static void snake_update() {
    if (game_over) return;

    Point new_head = {
        .x = snake.body[0].x + snake.direction.x,
        .y = snake.body[0].y + snake.direction.y
    };

    // Check collisions
    if (snake_collision()) {
        game_over = 1;
        return;
    }

    // Move body
    for (int i = snake.length; i > 0; i--) {
        snake.body[i] = snake.body[i - 1];
    }
    snake.body[0] = new_head;

    // Check food collision
    if (new_head.x == food.x && new_head.y == food.y) {
        snake.length++;
        score++;
        food.x = rand() % BOARD_WIDTH;
        food.y = rand() % BOARD_HEIGHT;
    }
}

static void snake_handle_input(char key) {
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

static int snake_collision() {
    Point head = snake.body[0];

    // Wall collision
    if (head.x < 0 || head.x >= BOARD_WIDTH || 
        head.y < 0 || head.y >= BOARD_HEIGHT) {
        return 1;
    }

    // Self collision
    for (int i = 1; i < snake.length; i++) {
        if (snake.body[i].x == head.x && 
            snake.body[i].y == head.y) {
            return 1;
        }
    }

    return 0;
}

// Update draw_rect to use centered coordinates
static void draw_rect(int x, int y, int w, int h, uint32 color) {
    int start_x = (SCREEN_WIDTH - ((BOARD_WIDTH * CELL_SIZE) + (BORDER_SIZE * 2))) / 2;
    int start_y = (SCREEN_HEIGHT - ((BOARD_HEIGHT * CELL_SIZE) + (BORDER_SIZE * 2))) / 2;
    
    x += start_x;
    y += start_y;
    
    for(int i = 0; i < w; i++) {
        for(int j = 0; j < h; j++) {
            vbe_putpixel(x + i, y + j, color);
        }
    }
}

// Draw the game board border
static void draw_border() {
    int width = BOARD_WIDTH * CELL_SIZE + (BORDER_SIZE * 2);
    int height = BOARD_HEIGHT * CELL_SIZE + (BORDER_SIZE * 2);
    
    // Top & bottom borders
    draw_rect(0, 0, width, BORDER_SIZE, COLOR_BORDER);
    draw_rect(0, height - BORDER_SIZE, width, BORDER_SIZE, COLOR_BORDER);
    
    // Left & right borders
    draw_rect(0, 0, BORDER_SIZE, height, COLOR_BORDER);
    draw_rect(width - BORDER_SIZE, 0, BORDER_SIZE, height, COLOR_BORDER);
}

static void snake_init() {
    snake.length = 1;
    snake.body[0].x = BOARD_WIDTH / 2;
    snake.body[0].y = BOARD_HEIGHT / 2;
    snake.direction.x = 1;
    snake.direction.y = 0;
    food.x = rand() % BOARD_WIDTH;
    food.y = rand() % BOARD_HEIGHT;
    game_over = 0;
    score = 0;
}

static void draw_game() {
    // Clear screen
    draw_rect(BORDER_SIZE, BORDER_SIZE, 
              BOARD_WIDTH * CELL_SIZE,
              BOARD_HEIGHT * CELL_SIZE, 
              COLOR_BG);
    
    // Draw snake
    for(int i = 0; i < snake.length; i++) {
        draw_rect(BORDER_SIZE + (snake.body[i].x * CELL_SIZE),
                 BORDER_SIZE + (snake.body[i].y * CELL_SIZE),
                 CELL_SIZE - 1, CELL_SIZE - 1,
                 COLOR_SNAKE);
    }
    
    // Draw food
    draw_rect(BORDER_SIZE + (food.x * CELL_SIZE),
             BORDER_SIZE + (food.y * CELL_SIZE), 
             CELL_SIZE - 1, CELL_SIZE - 1,
             COLOR_FOOD);
}

// Rest of snake_update() and snake_handle_input() remain same

// Add cleanup function
static void init_graphics() {
    if(vesa_init(SCREEN_WIDTH, SCREEN_HEIGHT, 32) < 0) {
        console_printf("Failed to initialize VESA graphics\n");
        return;
    }

    // Calculate board position to center it
    int board_width = (BOARD_WIDTH * CELL_SIZE) + (BORDER_SIZE * 2);
    int board_height = (BOARD_HEIGHT * CELL_SIZE) + (BORDER_SIZE * 2);
    
    // Store globally for use in drawing functions
    g_start_x = (SCREEN_WIDTH - board_width) / 2;
    g_start_y = (SCREEN_HEIGHT - board_height) / 2;

    // Clear screen
    for(int x = 0; x < SCREEN_WIDTH; x++) {
        for(int y = 0; y < SCREEN_HEIGHT; y++) {
            vbe_putpixel(x, y, COLOR_BG);
        }
    }
}

static void force_text_mode() {
    // Force VGA text mode 0x03
    __asm__ __volatile__ ("int $0x10" : : "a"(0x0003));
    
    // Reinitialize VGA subsystem
    console_init(COLOR_WHITE, COLOR_BLACK); 
}

static void cleanup_graphics() {
    // Switch back to text mode
    force_text_mode();
    
    // Small delay for mode switch
    for(int i = 0; i < 1000000; i++) { 
        __asm__ volatile("nop"); 
    }
    
    // Clear screen and reinit console
    console_clear(COLOR_WHITE, COLOR_BLACK);
}

void snake_game() {
    console_clear(COLOR_WHITE, COLOR_BLACK);
    // Initialize graphics
    init_graphics();
    snake_init();
    draw_border();
    
    int update_counter = 0;
    const int update_threshold = 25;
    const int sleep_time = 10000;

    while(!game_over) {
        if(kbhit()) {
            snake_handle_input(kb_getchar());
        }
        
        if(update_counter >= update_threshold) {
            snake_update();
            draw_game();
            update_counter = 0;
        }
        
        update_counter++;
        usleep(sleep_time);
    }

    // Proper cleanup sequence
     // When game over:
    cleanup_graphics();
    
    // Clear screen again
    console_clear(COLOR_WHITE, COLOR_BLACK);
    
    // Show game over
    console_printf("Game Over! Final Score: %d\n", score);
    console_printf("Press any key to continue...\n");
    
    // Wait for key and clear again
    kb_getchar();
    console_clear(COLOR_WHITE, COLOR_BLACK);
}