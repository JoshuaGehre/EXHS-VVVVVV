#ifndef OTHERLEVEL_H
#define OTHERLEVEL_H

#include "Entity.h"
#include "Game.h"

#include <string>
#include <vector>

struct Roomtext {
	int x, y, subx, suby;
	std::string text;
};

class otherlevelclass {
  public:
	enum {
		BLOCK = 0,
		TRIGGER,
		DAMAGE,
		DIRECTIONAL,
		SAFE,
		ACTIVITY
	};

	void addline(std::string t);
	const int* loadlevel(int rx, int ry);

	std::string roomname;

	int roomtileset = 0;

	// roomtext thing in other level
	bool roomtexton = false;
	std::vector<Roomtext> roomtext;
};

#endif /* OTHERLEVEL_H */
