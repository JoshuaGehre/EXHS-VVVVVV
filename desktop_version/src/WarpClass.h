#ifndef WARPCLASS_H
#define WARPCLASS_H

#include "Entity.h"
#include "Game.h"

#include <string>

class warpclass {
  public:
	const int* loadlevel(int rx, int ry);
	std::string roomname;
	int coin, rcol = 0;
	bool warpx, warpy = 0;
};

#endif /* WARPCLASS_H */
