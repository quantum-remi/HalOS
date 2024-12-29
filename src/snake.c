// #include "vesa.h"
// #include "timer.h"
// #include "keyboard.h"
// #include "console.h"

// #define WIDTH 20
// #define HEIGHT 20
// #define SNAKE_MAX_LENGTH 100
// #define PIXEL_SIZE 20

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
//     vesa_clear_screen(0x000000); // Clear screen with black color

//     // Draw the snake
//     for (int i = 0; i < snake.length; i++) {
//         vesa_draw_rect(snake.body[i].x * PIXEL_SIZE, snake.body[i].y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, 0x00FF00); // Green color
//     }

//     // Draw the food
//     vesa_draw_rect(food.x * PIXEL_SIZE, food.y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, 0xFF0000); // Red color

//     // Print the score
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
//     vesa_init();
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

//     printf("Game Over! Final Score: %d\n", score);
// }