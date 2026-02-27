#include <chrono> //chosing to use chrono so I can have fixed time ticks
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdio>

//I'm avoiding using namespace std for now
using std::cout;
using std::cin;
using std::deque;
using std::string;
using std::vector;

using GameClock = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;


struct TermGuard { //this is to change the keyboard input, makign it feel more like a game
    termios old{}; //old terminal settings
    bool ok = false; //successfully captures the settings

    TermGuard() {
        if (tcgetattr(STDIN_FILENO, &old) == 0) { //changing the keyboard input stream
            termios raw = old;
            raw.c_lflag &= ~(ICANON | ECHO); //Removing Icanon makes input instant, echo stops the input from repeating in the term again
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0; //no timeout for reading input
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            ok = true;
        }

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    ~TermGuard() { //runs to restore old settings when program is over (IMPORTANT)
        if (ok)
            tcsetattr(STDIN_FILENO, TCSANOW, &old);

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
};

char readKey() { //char by char input reading
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

void hideCursor() { cout << "\x1b[?25l"; } //\x1b is to use ANSI commands,
void showCursor() { cout << "\x1b[?25h"; }
void clearScreen() { cout << "\x1b[2J"; } //this clears the entire screen, super useful
void cursorTopLeft() { cout << "\x1b[H"; }




struct Pos { //POSITION STRUCT
    int x, y;
    bool operator==(const Pos& o) const { return x == o.x && y == o.y; } 
};

enum class Dir { Up, Down, Left, Right }; 

bool opposite(Dir a, Dir b) { //checks if the directions are cardinally opposite(so snake doesn't go backwards/inwards)
    return (a==Dir::Up&&b==Dir::Down) ||
           (a==Dir::Down&&b==Dir::Up) ||
           (a==Dir::Left&&b==Dir::Right) ||
           (a==Dir::Right&&b==Dir::Left);
}

struct Game {
    int w, h;
    deque<Pos> snake; //double ended queue, for easy access to "head" and "tail", which makes it easier to grow.
    Dir dir = Dir::Right;
    Dir pending = Dir::Right;
    Pos apple; //current apple position
    bool over = false; //wether current game is over
    int score = 0;

    std::mt19937 rng{std::random_device{}()}; //this serves to randomize, it's better than the usual call to random() I would make

    Game(int W,int H):w(W),h(H){}

    bool onSnake(Pos p){
        for(auto&s:snake) if(s==p) return true;// if any of the snake positions is the pos P, return true
        return false;
    }

    void spawnApple(){
        std::uniform_int_distribution<int> dx(0,w-1); 
        std::uniform_int_distribution<int> dy(0,h-1);

         while(onSnake(apple)) {//picks randoms spots while checking its not on the snake
            apple={dx(rng),dy(rng)};
        }
    }

    void reset(){ //resets snake
        snake.clear();
        snake.push_front({w/2,h/2});
        snake.push_back({w/2-1,h/2});
        snake.push_back({w/2-2,h/2});
        spawnApple();
        score = 0;
    }

    void tick(){ //this is the main change, otherwhise it felt like it jittery
        if(over) return;

        if(!opposite(dir,pending))
            dir=pending;

        Pos head=snake.front();

        if(dir==Dir::Up) head.y--;
        if(dir==Dir::Down) head.y++; //more rows down
        if(dir==Dir::Left) head.x--;
        if(dir==Dir::Right) head.x++; //more colums right

        if(head.x<0||head.y<0||head.x>=w||head.y>=h){ //out of bounds
            over=true; return;
        }

        if(onSnake(head)){ //since head is just the calculated value of the next position, it shouldn't be in the body unless crashing into itself
            over=true; return;
        }

        snake.push_front(head);

        if(head==apple) {
            score++;
            spawnApple();
        } 
        else {
            snake.pop_back(); //this is how it "grows" in the rear, by not shrinking
        }
    }
};

//PAINT THE GAME
string render(Game& g){
    vector<string> grid(g.h,string(g.w,' '));

    grid[g.apple.y][g.apple.x]='$'; //apple sprite, can't make "♥" work with normal compile

    for(size_t i=0;i<g.snake.size();i++){
        auto p=g.snake[i];
        grid[p.y][p.x]=(i==0?'O':'o'); //head is slightly bigger
    }

    string out;
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
            "Score: %d   |   WASD move | Q quit\n",
            g.score);

    out += buffer;

    out += '#';
    for(int i=0;i<g.w;i++) out+='#';
    out += "#\n";

    for(auto& row:grid){
        out += '#';
        out += row;
        out += "#\n";
    }

    out += '#';
    for(int i=0;i<g.w;i++) out+='#';
    out += "#\n";

    if(g.over) out += "GAME OVER\n";

    return out;
}

//MAIN LOOP
int main() {

    int size;
    cout<<"Board size (8-32): ";
    cin>>size;

    if(size<8) size=8;
    if(size>32) size=32;

    cin.ignore();

    TermGuard guard;

    Game game(size,size);
    game.reset();

    hideCursor();
    clearScreen();

    const ms step(300); // old version was 1 second tick
    auto last = GameClock::now();
    ms accumulator(0);

    bool quit=false;

    while(!quit){

        // input loop
        char c = readKey();
        while(c != 0) {

            if(c >= 'A' && c <= 'Z')
                c += 32;

            if(c == 'q') quit = true;
            if(c == 'w') game.pending = Dir::Up;
            if(c == 's') game.pending = Dir::Down;
            if(c == 'a') game.pending = Dir::Left;
            if(c == 'd') game.pending = Dir::Right;

            c = readKey();   //next key
        }

        auto now=GameClock::now();
        accumulator += std::chrono::duration_cast<ms>(now-last); //this is what maintains the tick length
        last=now;

        while(accumulator>=step){
            game.tick();
            accumulator-=step;
        }

        cursorTopLeft(); //move cursor top left
        cout << render(game) << std::flush; //paint the scene, redraw over

        std::this_thread::sleep_for(ms(5)); //this is so it doesn't monopolize over the cpu
    }

    showCursor();
    clearScreen();
    cursorTopLeft();
}