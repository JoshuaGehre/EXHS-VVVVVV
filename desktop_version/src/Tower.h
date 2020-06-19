#ifndef TOWER_H
#define TOWER_H

#include <string>
#include <vector>
#include "Game.h"

class towerclass
{
public:
    towerclass();

    int backat(int xp, int yp, int yoff);

    int at(int xp, int yp, int yoff);

    int miniat(int xp, int yp, int yoff);

    void loadminitower1();

    void loadminitower2();

    void loadbackground();

    void loadmap();

    //public var back:Array = new Array();
    //public var contents:Array = new Array();
    //public var minitower:Array = new Array();
    //public var vmult:Array = new Array();

    growing_vector<int> back;
    growing_vector<int> contents;
    growing_vector<int> minitower;
    growing_vector<int> vmult;

    bool minitowermode = false;
};





#endif /* TOWER_H */
