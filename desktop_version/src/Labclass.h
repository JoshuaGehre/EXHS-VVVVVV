#ifndef LABCLASS_H
#define LABCLASS_H

#include "Entity.h"
#include "Game.h"

class labclass {
  public:
	const int* loadlevel(int rx, int ry);

	std::string roomname;
	int coin, rcol = 0;
};
#endif /* LABCLASS_H */
