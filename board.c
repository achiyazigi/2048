#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define MAX_VAL_DIGITS 7
#define CHANCE_FOR_2 0.8f

void init()
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    init_pair(0, COLOR_WHITE, -1);
    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_MAGENTA, -1);
    init_pair(3, COLOR_BLUE, -1);
    init_pair(4, COLOR_CYAN, -1);
    init_pair(5, COLOR_YELLOW, -1);
    init_pair(6, COLOR_BLACK, COLOR_WHITE);
    init_pair(7, COLOR_GREEN, COLOR_WHITE);
    init_pair(8, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(9, COLOR_BLUE, COLOR_WHITE);
    init_pair(10, COLOR_CYAN, COLOR_WHITE);
    init_pair(11, COLOR_YELLOW, COLOR_WHITE);
    init_pair(12, COLOR_BLACK, COLOR_YELLOW);
    init_pair(13, COLOR_GREEN, COLOR_YELLOW);
}

typedef struct
{
    int value;
    int color_idx;
} Cell;

typedef struct
{
    int width;
    int height;
    Cell *cells;
    bool simulation;
    int scores;
    int last_generated_y;
    int last_generated_x;
} Board;

void board_init(Board *b, int height, int width)
{
    b->width = width;
    b->height = height;
    b->cells = calloc(b->width * b->height, sizeof(*b->cells));
    b->simulation = 0;
    b->scores = 0;
    b->last_generated_y = -1;
    b->last_generated_x = -1;
}

void board_free(Board *b)
{
    free(b->cells);
}

static int log2i(int x)
{
    int targetlevel = 0;
    while (x >>= 1)
        ++targetlevel;
    return targetlevel;
}

void board_set_cell_val(Board *b, int y, int x, int value)
{
    if (!b->simulation)
        b->cells[y * b->width + x] = (Cell){.value = value, .color_idx = log2i(value) % 14};
}

int board_get_cell_val(Board *b, int y, int x)
{
    return b->cells[y * b->width + x].value;
}

bool board_cell_dir(Board *b, int y, int x, int vert, int horz, bool *added_indices)
{
    if (y + vert >= b->height || y + vert < 0 || x + horz >= b->width || x + horz < 0)
    {
        return 0;
    }
    int cell_value = board_get_cell_val(b, y, x);
    int cell_ni_value = board_get_cell_val(b, y + vert, x + horz);

    if (cell_value == 0)
    {
        return 0;
    }

    if (cell_ni_value == 0)
    {
        board_set_cell_val(b, y + vert, x + horz, cell_value);
        board_set_cell_val(b, y, x, 0);
        board_cell_dir(b, y + vert, x + horz, vert, horz, added_indices);
        return 1;
    }
    if (cell_value == cell_ni_value)
    {
        if (added_indices[(y + vert) * b->width + x + horz])
        {
            return 0;
        }
        added_indices[(y + vert) * b->width + x + horz] = 1;
        board_set_cell_val(b, y + vert, x + horz, cell_value * 2);
        board_set_cell_val(b, y, x, 0);
        return 1;
    }
    return 0;
}

bool board_move_dir(Board *b, int vert, int horz)
{
    bool moved = 0;
    int major_size = b->height;
    int minor_size = b->width;
    if (vert != 0)
    {
        major_size = b->width;
        minor_size = b->height;
    }
    if (minor_size < 2)
    {
        return 0;
    }
    int minor_index = 0;
    int major_index = 0;
    bool *added_indices = malloc(minor_size * major_size * sizeof(*added_indices));
    for (int i = 0; i < major_size; ++i)
    {
        memset(added_indices, 0, minor_size * major_size * sizeof(*added_indices));
        for (int j = 0; j < minor_size; ++j)
        {
            minor_index = j;
            major_index = i;
            if (vert == 1 || horz == 1)
            {
                minor_index = minor_size - 1 - j;
            }
            if (horz != 0)
            {
                int temp = major_index;
                major_index = minor_index;
                minor_index = temp;
            }
            moved |= board_cell_dir(b, minor_index, major_index, vert, horz, added_indices);
        }
    }
    free(added_indices);
    if (!b->simulation)
        b->scores += moved;
    return moved;
}

bool board_move_right(Board *b)
{
    return board_move_dir(b, 0, 1);
}
bool board_move_left(Board *b)
{
    return board_move_dir(b, 0, -1);
}
bool board_move_up(Board *b)
{
    return board_move_dir(b, -1, 0);
}
bool board_move_down(Board *b)
{
    return board_move_dir(b, 1, 0);
}

// returns false if failed to generate
bool board_generate(Board *b)
{
    int cells_count = b->width * b->height;
    int available_indices[cells_count]; // VLA https://en.wikipedia.org/wiki/Variable-length_array
    int available_count = 0;
    for (int i = 0; i < cells_count; ++i)
    {
        if (b->cells[i].value == 0)
        {
            available_indices[available_count++] = i;
        }
    }
    if (available_count == 0)
    {
        return false;
    }

    int rand_idx = rand() % available_count;
    bool is_2 = ((double)rand() / RAND_MAX) < CHANCE_FOR_2;
    b->last_generated_y = available_indices[rand_idx] / b->width;
    b->last_generated_x = available_indices[rand_idx] % b->width;

    assert(board_get_cell_val(b, b->last_generated_y, b->last_generated_x) == 0 && "tried to generate in an occupied cell");

    int value = 2 * (2 - is_2); // 2 or 4
    board_set_cell_val(b, b->last_generated_y, b->last_generated_x, value);
    return true;
}

void board_render(Board *b, WINDOW *win)
{
    werase(win);
    box(win, 0, 0);

    for (int r = 0; r < b->height; ++r)
    {
        for (int c = 0; c < b->width; ++c)
        {
            Cell cell = b->cells[r * b->width + c];
            char str_val[MAX_VAL_DIGITS];
            sprintf(str_val, "%d", cell.value);

            wattron(win, COLOR_PAIR(cell.color_idx));
            bool new_generated = r == b->last_generated_y && c == b->last_generated_x;
            if (new_generated)
                wattron(win, A_BOLD | A_BLINK);

            int cell_offset_x = c * MAX_VAL_DIGITS + 1;
            int center_pad_x = (MAX_VAL_DIGITS - strlen(str_val)) / 2;
            mvwaddstr(win, r + 1, cell_offset_x + center_pad_x, str_val);

            if (new_generated)
                wattroff(win, A_BOLD | A_BLINK);
            wattroff(win, COLOR_PAIR(cell.color_idx));
        }
    }
    mvwprintw(win, b->height + 1, 1, "Scores: %d", b->scores);
    wrefresh(win);
}

void render_game_over(WINDOW *win)
{
    static const char *game_over_massage = "GAME OVER";
    mvwprintw(win, 0, (getmaxx(win) - strlen(game_over_massage)) / 2, "%s", game_over_massage);
    wrefresh(win);
}

WINDOW *board_create_window(Board *b)
{
    int board_height = b->height + 3;
    int board_width = b->width * MAX_VAL_DIGITS + 2;
    WINDOW *win = newwin(board_height, board_width, 0, 0);
    return win;
}

bool board_is_game_over(Board *b)
{
    bool result = 0;
    bool prev_simulation_mode = b->simulation;
    b->simulation = 1;

    result = !(board_move_down(b) || board_move_up(b) || board_move_left(b) || board_move_right(b));
    b->simulation = prev_simulation_mode;
    return result;
}

void game()
{
    int input;
    Board b;
    board_init(&b, 4, 4);
    WINDOW *win = board_create_window(&b);

    board_generate(&b);

    board_render(&b, win);

    bool moved = 0;
    bool game_over = 0;
    while ((input = getch()) != 'q' && !(game_over = board_is_game_over(&b)))
    {
        switch (input)
        {
        case KEY_LEFT:
            moved = board_move_left(&b);
            break;
        case KEY_RIGHT:
            moved = board_move_right(&b);
            break;
        case KEY_UP:
            moved = board_move_up(&b);
            break;
        case KEY_DOWN:
            moved = board_move_down(&b);
            break;
        }
        if (moved)
        {
            board_generate(&b);
        }
        board_render(&b, win);
    }
    if (game_over)
    {
        while ((input = getch()) != 'q')
            render_game_over(win);
    }
    board_free(&b);
    delwin(win);
}

int main(int argc, char *argv[])
{
    init();
    refresh();
    game();
    endwin();
}