#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <cstdlib>
#include <ctime>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define FPS 60
#define ANIM_SPEED 6
#define WALK_SPEED 4
#define JUMP_STRENGTH 20
#define GRAVITY 1

#define INPUT_LEFT (1<<0)
#define INPUT_RIGHT (1<<1)
#define INPUT_JUMP (1<<2)
#define INPUT_SHOOT (1<<3)

#define MAX_ZOMBIES 200

#define MAX_SCORE 150

enum Direction {
    LEFT,
    RIGHT
};

static SDL_Renderer *renderer = 0;

static SDL_Texture *gfxPico[2];
static SDL_Texture *gfxZombie1[2];
static SDL_Texture *gfxZombie2[2];
static SDL_Texture *gfxZombie3[2];
static SDL_Texture *gfxGibs[5];
static SDL_Texture *gfxEnd[2];
static SDL_Texture *gfxBomb[4];

static int scroll = 0;
static int score = 0;
static int bombs = 3;

class Gib
{
public:
    int x, y;
    int vx, vy;
    int tex;

    Gib(int x, int y, int tex)
        : x(x+2*4), y(y+2*4), tex(tex)
    {
        vx = rand() % 32 - 16;
        vy = -(rand() % 16) - 8;
    }

    void update()
    {
        x += vx;
        y += vy;
        vy += GRAVITY;
    }

    void draw()
    {
        SDL_Rect rect = {x, y, 4*4, 4*4};
        SDL_RenderCopy(renderer, gfxGibs[tex], NULL, &rect);
    }
};

static std::vector<Gib> gibs;

class Actor
{
public:
    int frame;
    int frameTimer;
    int x, y;
    int vy;
    Direction dir;
    int speed;
    bool moving;
    bool onGround;
    bool dead;
    SDL_Texture **textures;

    Actor(SDL_Texture **textures)
        : frame(0), frameTimer(0),
          x(512/2-2*8), y(512-12-8*4),
          vy(0), dir(RIGHT),
          speed(WALK_SPEED),
          moving(false),
          onGround(true),
          dead(false),
          textures(textures)
    {
    }

    virtual void hitWall() {}

    void update()
    {
        if (!moving) {
            frame = 0;
            frameTimer = 0;
        } else {
            if (++frameTimer > ANIM_SPEED) {
                frameTimer = 0;
                if (++frame >= 2) {
                    frame = 0;
                }
            }

            if (dir == LEFT) {
                if (x - speed < 0) {
                    x = 0;
                    hitWall();
                } else
                    x -= speed;
            } else {
                if (x + speed > 512 - 8*4) {
                    x = 512 - 8*4;
                    hitWall();
                } else
                    x += speed;
            }
        }

        if (!onGround) {
            if (vy > 0 && (y + 512/6 - scroll - 12 - 8*4) % (512/6) > (y + 512/6 + vy - scroll - 12 - 8*4) % (512/6)) {
                onGround = true;
                y = (y - scroll) - (y - scroll) % (512/6) + scroll + 12 + 8*4 - 1;
                vy = 0;
            } else {
                y += vy;
                vy += 2;
            }
        }
    }

    void draw()
    {
        SDL_Rect rect = {x, y, 8*4, 8*4};
        int tex = (!onGround || frame == 1) ? 1 : 0;
        SDL_RendererFlip flip = (dir == RIGHT) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        SDL_RenderCopyEx(renderer, textures[tex], NULL, &rect, 0.0, NULL, flip);
    }

    void kill(int min, int max)
    {
        dead = true;
        int nGibs = rand() % (max - min + 1) + min;
        gibs.push_back(Gib(x, y, 0));
        for (int i = 0; i < nGibs; ++i)
            gibs.push_back(Gib(x, y, rand() % 4 + 1));
    }
};

class Zombie : public Actor
{
public:
    int stage;
    Zombie(int stage)
        : Actor(0),
          stage(stage)
    {
        switch (stage) {
        case 1:
            textures = gfxZombie1;
            break;
        case 2:
            textures = gfxZombie2;
            break;
        case 3:
            textures = gfxZombie3;
            break;
        }
        moving = true;

        if (rand() % 2 == 0) {
            dir = RIGHT;
            x = -4*8;
        } else {
            dir = LEFT;
            x = 512 + 8*8;
        }
        int level = rand() % 5;
        y = (level) * 512/6 - 12 - 4*8 + scroll;

        speed = stage + 1;
    }

    void update()
    {
        if (onGround && rand() % 128 == 0) {
            vy = -JUMP_STRENGTH;
            onGround = false;
        }
        Actor::update();
    }

    void hitWall()
    {
        if (dir == LEFT)
            dir = RIGHT;
        else
            dir = LEFT;
    }
};

void loadTextures(SDL_Texture **gfx, const char *str, int size)
{
    for (int i = 0; i < size; ++i) {
        std::stringstream s;
        s << "data/" << str << i << ".png";
        SDL_Surface *surface = IMG_Load(s.str().c_str());
        gfx[i] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
        std::cerr << "Unable to initialize SDL_Image: " << IMG_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Pico's Birthday Zombie Attack ULTIMATE GAME OF THE YEAR EDITION",
                                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512, 0);
    if (!window) {
        std::cerr << "Could not create SDL window: " << SDL_GetError() << std::endl;
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    //Load graphics
    loadTextures(gfxPico, "pico", 2);
    loadTextures(gfxZombie1, "green", 2);
    loadTextures(gfxZombie2, "blue", 2);
    loadTextures(gfxZombie3, "red", 2);
    loadTextures(gfxGibs, "gib", 5);
    loadTextures(gfxEnd, "end", 2);
    loadTextures(gfxBomb, "bomb", 4);

    SDL_Texture *digits;
    {
        SDL_Surface *surf = IMG_Load("data/digits.png");
        digits = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
    }

    unsigned int input = 0;

    uint32_t ticks = SDL_GetTicks();

    Actor pico(gfxPico);

    std::vector<Zombie> zombies;

    srand(time(0));

    int frame = 0;
    int endDelay = 0;

    bool bombframe = false;

    for (;;) {
        input &= ~(INPUT_SHOOT);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                goto end;
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    input |= INPUT_LEFT;
                    break;
                case SDLK_RIGHT:
                    input |= INPUT_RIGHT;
                    break;
                case SDLK_z:
                    input |= INPUT_JUMP;
                    break;
                case SDLK_x:
                    input |= INPUT_SHOOT;
                    break;
                }
                break;
            case SDL_KEYUP:
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    input &= ~INPUT_LEFT;
                    break;
                case SDLK_RIGHT:
                    input &= ~INPUT_RIGHT;
                    break;
                case SDLK_z:
                    input &= ~INPUT_JUMP;
                    break;
                }
                break;
            }
        }
        SDL_SetRenderDrawColor(renderer, 40, 40, 30+score, 255);
        SDL_RenderClear(renderer);

        /* game logic */

        //spawn zombies
        if (score < MAX_SCORE) {
            if (zombies.size() < MAX_ZOMBIES && frame % 4 == 0) {
                int stage = 1;
                if (score > 60 && rand() % 3 == 0)
                    stage = 3;
                else if (score > 20 && rand() % 3 == 0)
                    stage = 2;
                zombies.push_back(Zombie(stage));
            }
        }

        //gibs
        for (unsigned int i = 0; i < gibs.size();) {
            gibs[i].update();
            if (gibs[i].y > 512)
                gibs.erase(gibs.begin() + i);
            else
                ++i;
        }

        if (!pico.dead) {
            if (score < MAX_SCORE) {
                if (input & INPUT_SHOOT && bombs > 0) {
                    --bombs;
                    for (unsigned int i = 0; i < zombies.size(); ++i) {
                        zombies[i].kill(5, 10);
                    }
                    zombies.clear();
                    bombframe = true;
                }

                bool onGround = pico.onGround;
                if (input & INPUT_LEFT) {
                    pico.dir = LEFT;
                    pico.moving = true;
                } else if (input & INPUT_RIGHT) {
                    pico.dir = RIGHT;
                    pico.moving = true;
                } else {
                    pico.moving = false;
                }

                if (pico.onGround && (input & INPUT_JUMP)) {
                    pico.onGround = false;
                    pico.vy = -JUMP_STRENGTH;
                }
                pico.update();

                if (!onGround && pico.onGround) {
                    if (++score >= MAX_SCORE) {
                        for (unsigned int i = 0; i < zombies.size(); ++i) {
                            zombies[i].kill(5, 10);
                        }
                        zombies.clear();
                        bombframe = true;
                    }
                }
                if (pico.y < 512/2) {
                    int dy = 512/2 - pico.y;
                    scroll += dy;
                    scroll %= 512/6;
                    pico.y = 512/2;
                    for (unsigned int i = 0; i < zombies.size(); ++i) {
                        zombies[i].y += dy;
                    }
                }
            } else {
                pico.moving = false;
                pico.update();
            }
        }

        for (unsigned int i = 0; i < zombies.size();) {
            zombies[i].update();
            if (zombies[i].y > 512) {
                zombies[i].kill(5, 10);
                zombies.erase(zombies.begin() + i);
            } else {
                if (!pico.dead) {
                    if (zombies[i].x + 8*4 > pico.x && zombies[i].x < pico.x + 8*4
                            && zombies[i].y + 8*4 > pico.y && zombies[i].y < pico.y + 8*4) {
                        pico.kill(40, 50);
                    }
                }
                ++i;
            }
        }

        /* game render */

        if (bombframe) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);
            bombframe = false;
        } else {
            for (int i = 0; i < 7; ++i) {
                SDL_Rect rectTop = {0, 512 - 512/6 * i - 12 + scroll, 512, 4};
                SDL_Rect rectMid = {0, 512 - 512/6 * i - 8 + scroll, 512, 4};
                SDL_Rect rectBtm = {0, 512 - 512/6 * i - 4 + scroll, 512, 4};

                SDL_SetRenderDrawColor(renderer, 82*3/2, 52*3/2, 41*3/2, 255);
                SDL_RenderFillRect(renderer, &rectTop);
                SDL_SetRenderDrawColor(renderer, 82, 52, 41, 255);
                SDL_RenderFillRect(renderer, &rectMid);
                SDL_SetRenderDrawColor(renderer, 82/2, 52/2, 41/2, 255);
                SDL_RenderFillRect(renderer, &rectBtm);
            }

            for (unsigned int i = 0; i < zombies.size(); ++i) {
                zombies[i].draw();
            }

            for (unsigned int i = 0; i < gibs.size(); ++i) {
                gibs[i].draw();
            }

            if (pico.dead) {
                if (endDelay > 30)
                    SDL_RenderCopy(renderer, gfxEnd[0], NULL, NULL);
                else
                    endDelay++;
            } else {
                pico.draw();

                if (score >= MAX_SCORE) {
                    if (endDelay > 30)
                        SDL_RenderCopy(renderer, gfxEnd[1], NULL, NULL);
                    else
                        endDelay++;
                }
            }

            SDL_Rect rect = {8, 8, 8*4, 8*4};
            SDL_RenderCopy(renderer, gfxBomb[bombs], NULL, &rect);

            //Render score
            SDL_Rect src = {0, 0, 8, 8};
            rect.x = 512 - 8;
            int nDigits = 3;
            if (score < 10)
                nDigits = 1;
            else if (score < 100)
                nDigits = 2;
            int place = 1;
            for (int i = 0; i < nDigits; ++i) {
                rect.x -= 8*4;
                src.x = (score / place % 10) * 8;
                SDL_RenderCopy(renderer, digits, &src, &rect);
                place *= 10;
            }
        }

        /* fps stuff */
        SDL_RenderPresent(renderer);
        uint32_t delta = SDL_GetTicks() - ticks;
        if (delta < 1000 / FPS)
            SDL_Delay(1000 / FPS - delta);
        ticks = SDL_GetTicks();
        ++frame;
    }
end:

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

