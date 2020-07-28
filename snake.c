#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define GRASS     '~'
#define WALL      '%'
#define BODY      '^'
#define GRASS_COLOR    1
#define SNAKE_COLOR    2
#define WALL_COLOR     3
#define TROPHY_COLOR   4
#define START_SPEED 120000   //raise to go slower at start of game
#define SPEED_UP     60000   //raise to speed up more as game progresses
#define START_LENGTH     3

typedef enum {
    LEFT,
    RIGHT,
    DOWN,
    UP
} dir;

typedef struct {
    int x;
    int y;
} position;

typedef struct{
    int time_left;
    position pos;
    int was_eaten;
} trophy;

typedef struct{
    int grow_amt;
    position head_pos;
    position* body;
    int total_len;
    dir direction;
} snake;

void check_input(dir*);
void next_move(position*, snake*, trophy*);
void move_pos(position);
void move_snake(snake*);
void draw_game();
int can_move(int, int, snake*, trophy*);
void gameover();
void check_win(int, int);
int draw_trophy(trophy*);
void remove_trophy_if_exists(trophy);
unsigned long get_speed(int, int);

int main() {
    time_t tm;                      //random init
    srand((unsigned) time(&tm));
    //CURSES SETUP////////////////////////////////////////////////////////
    initscr();                      // inititalize the curses library
    start_color();                  // color and setup \/\/\/\/
    init_pair(GRASS_COLOR, COLOR_MAGENTA, COLOR_YELLOW);
    init_pair(SNAKE_COLOR, COLOR_BLACK, COLOR_GREEN);
    init_pair(WALL_COLOR, COLOR_YELLOW, COLOR_CYAN);
    init_pair(TROPHY_COLOR, COLOR_YELLOW, COLOR_RED);
    cbreak();
    curs_set(0);                   // hide the cursor
    noecho();                      // do not echo user input to the screen
    keypad(stdscr, TRUE);          // enables working with the arrow keys
    nodelay(stdscr, TRUE);         // no waiting for input
    //DRAW GAME////////////////////////////////////////////////////////////
    clear();                       // clear the screen
    draw_game();                   // draw the game
    trophy trophy = {0, {-1, -1}, 0};   // intitialize trophy to not on board
    int len_to_win = COLS + LINES - 2;  // win condition & arraylen
    unsigned long total_delay = 0;   // used to measure time for trophy
    //INITIALIZE THE SNAKE/////////////////////////////////////////////////
    snake snake;
    snake.grow_amt = START_LENGTH - 1; // this will be the starting length
    snake.total_len = 1;        // initial length is 1, will grow to above
    position body[len_to_win];  // used array of positions, initialize
    snake.body = body;          // assign array to snake
    snake.body[0].x = snake.head_pos.x = COLS / 2;  // initialize tail and head
    snake.body[0].y = snake.head_pos.y = LINES / 2; //^^^^^^^^^^^^^^^^^^^^^^^^^
    snake.direction = (dir)rand() % 4;  // start direction, random
    ///////////////////////////////////////////////////////////////////////
    while(1) { //GAME LOOP////////////////////////////////////////////////
        ////input and movement ///////////////////////////////////////////
        check_input(&snake.direction);                // get input
        next_move(&snake.head_pos, &snake, &trophy);  // check next move
        move_snake(&snake);                           // move the snake
        refresh();                                    // update the screen
        ////speed logic /////////////////////////////////////////////////
        unsigned long delay = get_speed(len_to_win, snake.total_len); //wait time
        total_delay += delay;                         // count time to wait
        usleep(delay);                                // wait
        ////trophy logic ////////////////////////////////////////////////
        if (!trophy.time_left)                        // checks for trophy
            trophy.time_left = draw_trophy(&trophy);  // adds it if none
        if (total_delay >= 1000000) {                 // sees if second has passed
            trophy.time_left--;                       // reduces timer
            total_delay = 0;                          // reset counter
        }
        check_win(snake.total_len, len_to_win);       // check for win
        char str[2];
        sprintf(str, "%d", trophy.time_left);         // get time left for trophy
        mvprintw(0, 0, str);                          // print in corner
    } ////////////////////////////////////////////////////////////////////                           
}

//gets input, changes direction or loses game, if opposite 
void check_input(dir *direction){ 
    int ch = getch();
    switch (ch) {
        case 'w':
        case 'W':
        case KEY_UP: 
            if((*direction) != DOWN) (*direction) = UP;
            break;
        case 's':
        case 'S':
        case KEY_DOWN:
            if((*direction) != UP) (*direction) = DOWN;
            break;
        case 'a':
        case 'A':
        case KEY_LEFT:
            if(*direction != RIGHT) (*direction) = LEFT;
            break;
        case 'd':
        case 'D':
        case KEY_RIGHT:
            if((*direction) != LEFT) (*direction) = RIGHT;
            break;
        case 'x':
        case 'X':
            endwin();
            exit(EXIT_SUCCESS);
        default: break;
    }
}

//checks the next move in snake's direction, moves head or gameover
void next_move(position *pos, snake *snake, trophy *trophy){
    switch (snake->direction) {
        case UP:
            if(can_move(pos->y -1,pos->x, snake, trophy)) // check if move is legal
                pos->y = pos->y -1; // if so, put the head there (will draw later)
            else gameover();        // otherwise, you lost!
            break;
        case DOWN:                                         //same
            if(can_move(pos->y +1,pos->x, snake, trophy))
                pos->y = pos->y +1;
            else gameover();
            break;
        case LEFT:                                         //same
            if(can_move(pos->y,pos->x-1, snake, trophy))
                pos->x = pos->x -1;
            else gameover();
            break;
        case RIGHT:                                        //same
            if(can_move(pos->y,pos->x+1, snake, trophy))
                pos->x = pos->x +1;
            else gameover();
            break;
    }
}

//draws head tile at given position, should be given head position
void move_pos(position head_pos){
    attron(COLOR_PAIR(SNAKE_COLOR));
    mvaddch(head_pos.y, head_pos.x, BODY); // draws a snake tile at the new head position
    attroff(COLOR_PAIR(SNAKE_COLOR));
}

//draws the background and border of the game
void draw_game() {
    int y;
    attron(COLOR_PAIR(GRASS_COLOR));   // draw all the grass
    for (y = 0; y < LINES; y++) {
        mvhline(y, 0, GRASS, COLS);
    }
    attroff(COLOR_PAIR(GRASS_COLOR));
    attron(COLOR_PAIR(WALL_COLOR));    // draw the border
    border(WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL);
    attroff(COLOR_PAIR(WALL_COLOR));
}

//checks if the position passed is a legal move for the snake
int can_move(int y, int x, snake *snake, trophy *trophy) 
{
    int ret;
    int ch = (mvinch(y,x) & A_CHARTEXT);         // check tile
    if((ret = (ch & A_CHARTEXT) != WALL && (ch & A_CHARTEXT) != BODY)){ // if its not a wall or body, all good
        if((ch & A_CHARTEXT) != GRASS){          // if it's a trophy
            snake->grow_amt += (ch)-48;          // add the number it represents to snake grow amount
            trophy->was_eaten = 1;               // the trophy didn't timeout
            trophy->time_left = 0;               // trophy is gone
        }
    }
    return ret; //value passed to next_move function
}

//end curses and printout
void gameover(){
    endwin();
    printf("You Lose.\n");
    exit(EXIT_SUCCESS);
}

//checks to see if snake length is half perimeter, end curses, printout
void check_win(int curr, int goal){
    if (curr >= goal) { //you won
        endwin();
        printf("YOU WIN!!!!\n");
        exit(EXIT_SUCCESS);
    }
}

//removes old trophy if time has lapsed
void remove_trophy_if_exists(trophy trophy) {
    if (trophy.pos.x != -1 && trophy.pos.y != -1) {
        if (!trophy.was_eaten) {        // checks for time lapse --> not eaten!
            attron(COLOR_PAIR(GRASS_COLOR));
            mvaddch(trophy.pos.y, trophy.pos.x, GRASS);   // place grass to remove the trophy
            attroff(COLOR_PAIR(GRASS_COLOR));
        }
    }
}

//puts new random trophy on screen, records position in trophy struct, and sets timer
int draw_trophy(trophy *trophy) {
    remove_trophy_if_exists((*trophy));
    int y = 0;
    int x = 0;
    while (1) {           // keeps cycling until it finds empty spot
        y = rand() % LINES; // choose random spot
        x = rand() % COLS;
        int ch = (int)mvinch(y, x); // see what's there
        if (((ch & A_CHARTEXT) == GRASS)) {
            break; // it's empty, move on
        }
    }
    int t_val = (rand() % 9) + 1; // get random number between 0-9
    attron(COLOR_PAIR(TROPHY_COLOR));
    mvaddch(y, x, t_val + 48);    // draw value at empty spot
    attroff(COLOR_PAIR(TROPHY_COLOR));
    trophy->pos.x = x;           //record the position
    trophy->pos.y = y;
    trophy->was_eaten = 0;       //reset eaten check
    return abs(t_val - 10);      //this is how many seconds the trophy will remain 
}

//makes speed proportional to percentage complete
unsigned long get_speed(int goal, int curr){
    return (START_SPEED-((curr/goal)*SPEED_UP));
}

//moves snake by drawing new tile at head and erasing tail, if not growing
void move_snake(snake *snake){
    move_pos(snake->head_pos);               // draw new snake tile at head
    if (snake->grow_amt == 0) {              // if not growing, draws a grass tile after tail to erase
        attron(COLOR_PAIR(GRASS_COLOR));
        mvaddch(snake->body[0].y, snake->body[0].x, GRASS); // draw grass (erase old tail space)
        attroff(COLOR_PAIR(GRASS_COLOR));
        for (int i = 0; i < snake->total_len;  i++) {
            if (i == snake->total_len - 1) {  // condition for game start-up, tail is on head
                snake->body[i].x = snake->head_pos.x;
                snake->body[i].y = snake->head_pos.y;
            } else {
                snake->body[i].x = snake->body[i + 1].x; 
                snake->body[i].y = snake->body[i + 1].y;
            }
        }
    } else {                      // the snake is growing, so don't erase anything, add to length
        snake->body[snake->total_len].x = snake->head_pos.x;
        snake->body[snake->total_len].y = snake->head_pos.y;
        snake->total_len++;       
        snake->grow_amt--;        
    }
}