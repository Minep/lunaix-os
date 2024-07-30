#include <errno.h>
#include <fcntl.h>
#include <lunaix/lunaix.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define check(statement)                                                       \
    ({                                                                         \
        int err = 0;                                                           \
        if ((err = (statement)) < 0) {                                         \
            syslog(2, #statement " failed: %d", err);                          \
            _exit(1);                                                          \
        }                                                                      \
        err;                                                                   \
    })


#define TTY_WIDTH 80
#define TTY_HEIGHT 25

#define MAZE_WIDTH TTY_WIDTH-1
#define MAZE_HEIGHT TTY_HEIGHT

#define BUFFER_SIZE 1024

#define WALL '#'
#define ROAD ' '
#define PLAYER 'P'
#define FLAG 'F'
#define CUE '.'

#define VISITED 1

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

int rand_fd;
int fd;

void get_rand(unsigned int *r, unsigned int max) {
    if (max==0)
    {
        *r = 0;
        return;
    }
    read(rand_fd, r, 4);
    *r = (*r)%max;
}

typedef struct player_t {
    int x;
    int y;
}player_t;

typedef struct write_cmd {
    int x;
    int y;
    char data[MAZE_WIDTH+1];
}write_cmd;

typedef struct node_t {
    int x;
    int y;
}node_t;

typedef struct stack_t {
    node_t node[1000];
    unsigned int length;
}road_list_t, cue_list_t;

road_list_t road_list;
cue_list_t cue_list;

void init_list() {
    for (size_t i = 0; i < 1000; i++)
    {
        road_list.node[i].x = 0;
        road_list.node[i].y = 0;
    }
    road_list.length = 0;

    for (size_t i = 0; i < 1000; i++)
    {
        cue_list.node[i].x = 0;
        cue_list.node[i].y = 0;
    }
    cue_list.length = 0;
}

int insert_road_list(node_t n) {
    if (road_list.length>=1000)
    {
        return 0;
    }
    
    unsigned int r = 0;
    get_rand(&r, road_list.length);
    for (size_t i = road_list.length; i > r; i--)
    {
        road_list.node[i] = road_list.node[i-1];
    }
    road_list.node[r] = n;
    road_list.length+=1;
    return 1;
}

int push_road_list(node_t node) {
    if (road_list.length>=1000)
    {
        return 0;
    }
    road_list.node[road_list.length] = node;
    road_list.length+=1;
    return 1;
}

node_t pop_road_list() {
    node_t n={.x = 0, .y = 0};
    if (road_list.length==0)
    {
        return n;
    }
    n = road_list.node[road_list.length-1];
    road_list.node[road_list.length-1].x = 0;
    road_list.node[road_list.length-1].y = 0;
    road_list.length-=1;
    return n;
}

int push_cue_list(node_t node) {
    if (cue_list.length>=1000)
    {
        return 0;
    }
    cue_list.node[cue_list.length] = node;
    cue_list.length+=1;
    return 1;
}

node_t pop_cue_list() {
    node_t n={.x = 0, .y = 0};
    if (cue_list.length==0)
    {
        return n;
    }
    n = cue_list.node[cue_list.length-1];
    cue_list.node[cue_list.length-1].x = 0;
    cue_list.node[cue_list.length-1].y = 0;
    cue_list.length-=1;
    return n;
}

int iswall(node_t n) {
    if (n.x<0 || n.y<0)
    {
        return 0;
    }
    
    return n.x%2==0 || n.y%2==0;
}

// is road position
// no matter it is marked as '-'
int isroad(node_t n) {
    if (n.x<=0 || n.y<=0 || n.x>=MAZE_WIDTH-1 || n.y>=MAZE_HEIGHT-1)
    {
        return 0;
    }
    
    return !iswall(n);
}


void generate_rand_road_node(node_t *n)
{
    get_rand((unsigned int *)&n->x, MAZE_WIDTH);
    get_rand((unsigned int *)&n->y, MAZE_HEIGHT);
    if (isroad(*n))
    {
        return;
    }

    if (n->x%2==0)
    {
        if (n->x<=(MAZE_WIDTH/2))
        {
            n->x+=1;
        }
        else
        {
            n->x-=1;
        }
    }

    if (n->y%2==0)
    {
        if (n->y<=(MAZE_HEIGHT/2))
        {
            n->y+=1;
        }
        else
        {
            n->y-=1;
        }
    }
}

char maze[MAZE_HEIGHT][MAZE_WIDTH];
char visited[MAZE_HEIGHT][MAZE_WIDTH];

void pick_near_road_to_list(node_t current_node) {
    node_t picked_node;

    //UP
    picked_node.x = current_node.x;
    picked_node.y = current_node.y-2;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]!=ROAD)
    {
        insert_road_list(picked_node);
    }
    
    //RIGHT
    picked_node.x = current_node.x+2;
    picked_node.y = current_node.y;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]!=ROAD)
    {
        insert_road_list(picked_node);
    }

    //DOWN
    picked_node.x = current_node.x;
    picked_node.y = current_node.y+2;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]!=ROAD)
    {
        insert_road_list(picked_node);
    }

    //LEFT
    picked_node.x = current_node.x-2;
    picked_node.y = current_node.y;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]!=ROAD)
    {
        insert_road_list(picked_node);
    }
}

node_t pick_near_road(node_t current_node) {
    node_t picked_node;

    //UP
    picked_node.x = current_node.x;
    picked_node.y = current_node.y-2;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]==ROAD)
    {
        return picked_node;
    }
    
    //LEFT
    picked_node.x = current_node.x-2;
    picked_node.y = current_node.y;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]==ROAD)
    {
        return picked_node;
    }

    //DOWN
    picked_node.x = current_node.x;
    picked_node.y = current_node.y+2;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]==ROAD)
    {
        return picked_node;
    }

    //RIGHT
    picked_node.x = current_node.x+2;
    picked_node.y = current_node.y;
    if (isroad(picked_node) && maze[picked_node.y][picked_node.x]==ROAD)
    {
        return picked_node;
    }

    return picked_node;
}


void break_wall(node_t n1, node_t n2)
{
    if (n1.x==n2.x)
    {
        maze[(n1.y+n2.y+1)/2][n1.x]=ROAD;
    }
    else if(n1.y==n2.y)
    {
        maze[n1.y][(n1.x+n2.x+1)/2]=ROAD;
    }
}

void print_maze(){
    write_cmd wcmd;
    wcmd.x = 0;
    wcmd.data[MAZE_WIDTH] = '\0';
    
    for (size_t j = 0; j < MAZE_HEIGHT; j++)
    {
        for (size_t i = 0; i < MAZE_WIDTH; i++)
        {
            wcmd.data[i] = maze[j][i];
        }
        wcmd.y = j;
        write(fd, &wcmd, sizeof(write_cmd));
    }
}

void refresh_line(int y){
    write_cmd wcmd;
    wcmd.x = 0;
    wcmd.y = y;
    wcmd.data[MAZE_WIDTH] = '\0';

    for (size_t i = 0; i < MAZE_WIDTH; i++)
    {
        wcmd.data[i] = maze[y][i];
    }

    write(fd, &wcmd, sizeof(write_cmd));
}

// Prim algorithm
void generate_maze()
{
    node_t start_node;

    for (size_t i = 0; i < MAZE_HEIGHT; i++)
    {
        for (size_t j = 0; j < MAZE_WIDTH; j++)
        {
            maze[i][j] = WALL;
        }
    }

    rand_fd = open("/dev/rand", O_RDONLY);

    generate_rand_road_node(&start_node);

    maze[start_node.y][start_node.x] = ROAD;

    pick_near_road_to_list(start_node);

    while (road_list.length>0)
    {
        node_t n = pop_road_list();
        maze[n.y][n.x] = ROAD;
        node_t n2 = pick_near_road(n);
        maze[n2.y][n2.x] = ROAD;
        break_wall(n, n2);
        pick_near_road_to_list(n);
        print_maze();
    }

    maze[1][1] = PLAYER;
    maze[MAZE_HEIGHT-2][MAZE_WIDTH-2] = FLAG;
    refresh_line(1);
    refresh_line(MAZE_HEIGHT-2);
}


int
set_termios(int fd) {
    struct termios term;

    check(tcgetattr(fd, &term));

    term.c_lflag = IEXTEN | ISIG | ECHO | ECHOE | ECHONL;
    term.c_cc[VERASE] = 0x7f;

    check(tcsetattr(fd, 0, &term));

    return 0;
}

int
set_termios_end(int fd) {
    struct termios term;

    check(tcgetattr(fd, &term));

    term.c_lflag = ICANON | IEXTEN | ISIG | ECHO | ECHOE | ECHONL;
    term.c_cc[VERASE] = 0x7f;

    check(tcsetattr(fd, 0, &term));

    return 0;
}

node_t player;

void up(){
    if (maze[player.y-1][player.x]!='#')
    {
        player.y-=1;
        maze[player.y][player.x] = 'P';
        maze[player.y+1][player.x] = ' ';
    }
    refresh_line(player.y);
    refresh_line(player.y+1);
}

void right(){
    if (maze[player.y][player.x+1]!='#')
    {
        player.x+=1;
        maze[player.y][player.x] = 'P';
        maze[player.y][player.x-1] = ' ';
    }
    refresh_line(player.y);
}

void down(){
    if (maze[player.y+1][player.x]!='#')
    {
        player.y+=1;
        maze[player.y][player.x] = 'P';
        maze[player.y-1][player.x] = ' ';
    }
    refresh_line(player.y);
    refresh_line(player.y-1);
}

void left(){
    if (maze[player.y][player.x-1]!='#')
    {
        player.x-=1;
        maze[player.y][player.x] = 'P';
        maze[player.y][player.x+1] = ' ';
    }
    refresh_line(player.y);
}

void visit_node(node_t current_node){
    node_t picked_node;

    //UP
    picked_node.x = current_node.x;
    picked_node.y = current_node.y-1;
    if (maze[picked_node.y][picked_node.x]!=PLAYER && maze[picked_node.y][picked_node.x]!=WALL && visited[picked_node.y][picked_node.x]!=VISITED)
    {
        push_road_list(picked_node);
    }
    
    //RIGHT
    picked_node.x = current_node.x+1;
    picked_node.y = current_node.y;
    if (maze[picked_node.y][picked_node.x]!=PLAYER && maze[picked_node.y][picked_node.x]!=WALL && visited[picked_node.y][picked_node.x]!=VISITED)
    {
        push_road_list(picked_node);
    }

    //DOWN
    picked_node.x = current_node.x;
    picked_node.y = current_node.y+1;
    if (maze[picked_node.y][picked_node.x]!=PLAYER && maze[picked_node.y][picked_node.x]!=WALL && visited[picked_node.y][picked_node.x]!=VISITED)
    {
        push_road_list(picked_node);
    }

    //LEFT
    picked_node.x = current_node.x-1;
    picked_node.y = current_node.y;
    if (maze[picked_node.y][picked_node.x]!=PLAYER && maze[picked_node.y][picked_node.x]!=WALL && visited[picked_node.y][picked_node.x]!=VISITED)
    {
        push_road_list(picked_node);
    }    
}

int is_near_node(node_t n1, node_t n2){
    if (n1.x==n2.x && (n1.y-n2.y==1 || n1.y-n2.y==-1))
    {
        return 1;
    }
    else if (n1.y==n2.y && (n1.x-n2.x==1 || n1.x-n2.x==-1))
    {
        return 1;
    }
    else if (n1.x==n2.x && n1.y==n2.y)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void dfs(){
    for (size_t i = 0; i < MAZE_HEIGHT; i++)
    {
        for (size_t j = 0; j < MAZE_WIDTH; j++)
        {
            visited[i][j] = 0;
        }
    }
    
    node_t node = player;
    node_t cue_node = player;
    push_road_list(node);
    push_cue_list(cue_node);
    while (road_list.length>0 && maze[node.y][node.x]!=FLAG)
    {
        node = pop_road_list();
        cue_node = pop_cue_list();
        while (!is_near_node(node, cue_node) && cue_node.x!=0)
        {
            if (maze[cue_node.y][cue_node.x] != PLAYER)
            {
                maze[cue_node.y][cue_node.x] = ROAD;
            }            
            refresh_line(cue_node.y);
            cue_node = pop_cue_list();
        }
        if (cue_node.x!=0)
        {
            push_cue_list(cue_node);
        }
        
        if (maze[node.y][node.x]!=FLAG && visited[node.y][node.x]!=VISITED)
        {
            if (maze[node.y][node.x]!=PLAYER)
            {
                maze[node.y][node.x] = CUE;
            }
            push_cue_list(node);
            visited[node.y][node.x] = VISITED;
            visit_node(node);
        }

        refresh_line(node.y);
    }
}

int
main(int argc, char* argv[])
{
    fd = open("/dev/ttyG0", FO_RDWR);
    check(set_termios(fd));
    init_list();
    generate_maze();
    
    char action = '\0';
    player.x=1;
    player.y=1;
    while (action!='Q')
    {
        read(fd, &action, 1);
        switch (action)
        {
        case 'W':
            up();
            break;
        case 'D':
            right();
            break;
        case 'S':
            down();
            break;
        case 'A':
            left();
            break;
        case 'H':
            dfs();
            break;
        case 'N':
            generate_maze();
            break;
        default:
            break;
        }
    }

    check(set_termios_end(fd));
    return 0;
}