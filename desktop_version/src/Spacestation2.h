#ifndef SPACESTATION2_H
#define SPACESTATION2_H

#include "Entity.h"
#include "Game.h"

#include <string>

class spacestation2class {
  public:
	const int* loadlevel(int rx, int ry);
	std::string roomname;
};

#endif /* SPACESTATION2_H */
