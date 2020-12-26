#ifndef FINALCLASS_H
#define FINALCLASS_H

#include "Entity.h"
#include "Game.h"

#include <string>

class finalclass {
  public:
	const int* loadlevel(int rx, int ry);

	std::string roomname;
	int coin, rcol = 0;
	bool warpx, warpy = false;
};

#endif /* FINALCLASS_H */
