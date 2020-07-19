#if !defined(NO_CUSTOM_LEVELS)

#ifndef EDITOR_H
#define EDITOR_H

#include <vector>
#include "Game.h"
#include <string>
#include <string_view>
#include "Script.h"
#include "Graphics.h"

#define VCEVERSION 1
#define IS_VCE_LEVEL (map.custommode && ed.vceversion > 0)

enum tiletyp {
    TILE_NONE,
    TILE_BACKGROUND,
    TILE_SPIKE,
    TILE_FOREGROUND,
};

// Text entry field type
enum textmode {
    TEXT_NONE,

    // In-editor text fields
    TEXT_LOAD,
    TEXT_SAVE,
    TEXT_ROOMNAME,
    TEXT_SCRIPT,
    TEXT_ROOMTEXT,
    TEXT_ACTIVITYZONE,
    TEXT_GOTOROOM,
    LAST_EDTEXT = TEXT_GOTOROOM,

    // Settings-mode text fields
    TEXT_TITLE,
    TEXT_DESC,
    TEXT_WEBSITE,
    TEXT_CREATOR,
    NUM_TEXTMODES,

    // Text modes with an entity
    FIRST_ENTTEXT = TEXT_SCRIPT,
    LAST_ENTTEXT = TEXT_ACTIVITYZONE,
};

std::string find_title(std::string_view buf);
std::string find_desc1(std::string_view buf);
std::string find_desc2(std::string_view buf);
std::string find_desc3(std::string_view buf);
std::string find_creator(std::string_view buf);
std::string find_website(std::string_view buf);

class edentities{
public:
    int x = 0;
    int y = 0;
    int t = 0;
    int subx = 0;
    int suby = 0;
    //parameters
    int p1 = 0;
    int p2 = 0;
    int p3 = 0;
    int p4 = 0;
    int p5 = 0;
    int p6 = 0;
    int state = 0;
    int intower = 0;
    std::string scriptname;
    std::string activityname;
    std::string activitycolor;
    bool onetime = false;
};


class edlevelclass{
public:
    edlevelclass();
    int tileset = 0;
    int tilecol = 0;
    int customtileset = 0;
    int customspritesheet = 0;
    std::string roomname;
    int warpdir = 0;
    int platx1 = 0;
    int platy1 = 0;
    int platx2 = 0;
    int platy2 = 0;
    int platv = 0;
    int enemyv = 0;
    int enemyx1 = 0;
    int enemyy1 = 0;
    int enemyx2 = 0;
    int enemyy2 = 0;
    int enemytype = 0;
    int directmode = 0;
    int tower = 0;
    int tower_row = 0;
};

class edaltstate {
public:
    edaltstate();
    int x = -1;
    int y = -1; // -1 means not set
    int state = -1;
    std::vector<int> tiles;

    void reset();
};

class edtower {
public:
    edtower();
    int size = 40; // minimum size
    int scroll = 0; // scroll direction (0=The Tower, 1=Panic Room)
    std::vector<int> tiles;

    void reset(void);
};

struct LevelMetaData
{
    std::string title;
    std::string creator;
    std::string Desc1;
    std::string Desc2;
    std::string Desc3;
    std::string website;
    std::string filename;

    std::string modifier;
    std::string timeCreated;
    std::string timeModified;

    int version = 0;
};

struct GhostInfo {
    int rx; // game.roomx-100
    int ry; // game.roomy-100
    int x; // .xp
    int y; // .yp
    int col; // .colour
    Uint32 realcol;
    int frame; // .drawframe
};

struct Dimension {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

extern std::vector<edentities> edentity;

class EditorData
{
    public:

    static EditorData& GetInstance()
    {
        static EditorData  instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }


    std::string title;
    std::string creator;

    std::string modifier;
    std::string timeCreated;
    std::string timeModified;

private:


    EditorData()
    {
    }

};


class editorclass {
    //Special class to handle ALL editor variables locally
public:
    editorclass();

    std::string Desc1;
    std::string Desc2;
    std::string Desc3;
    std::string website;

    std::vector<std::string> directoryList;
    std::vector<LevelMetaData> ListOfMetaData;

    void loadZips();
    void getDirectoryData();
    bool getLevelMetaData(std::string& filename, LevelMetaData& _data );

    void reset();
    void getlin(const enum textmode mode, const std::string& prompt, std::string *ptr);
  std::vector<int> loadlevel(int rxi, int ryi, int altstate);

    void placetile(int x, int y, int t);

    void placetilelocal(int x, int y, int t);

    int gettilelocal(int x, int y);
    void settilelocal(int x, int y, int tile);

    int getenemyframe(int t, int dir);
    int base(int x, int y);

    int backbase(int x, int y);

    enum tiletyp gettiletyp(int room, int tile);
    enum tiletyp gettiletyplocal(int x, int y);
    enum tiletyp getabstiletyp(int x, int y);

    int absat(int *x, int *y);
    int at(int x, int y);

    int freewrap(int x, int y);

    int backonlyfree(int x, int y);

    int backfree(int x, int y);

    int towerspikefree(int x, int y);
    int spikefree(int x, int y);
    int towerfree(int x, int y);
    int free(int x, int y);
    int getfree(enum tiletyp tile);
    int absfree(int x, int y);

    int match(int x, int y);
    int warpzonematch(int x, int y);
    int outsidematch(int x, int y);

    int backmatch(int x, int y);

    void switch_tileset(const bool reversed = false);
    void switch_tileset_tiles(int from, int to);
    void switch_tilecol(const bool reversed = false);
    void clamp_tilecol(const int rx, const int ry, const bool wrap = false);
    void switch_enemy(const bool reversed = false);

    void enable_tower(void);
    void snap_tower_entry(int rx, int ry);
    void upsize_tower(int tower, int y);
    void downsize_tower(int tower);
    void resize_tower_tiles(int tower);
    void shift_tower(int tower, int y);
    int get_tower(int rx, int ry);
    bool find_tower(int tower, int &rx, int &ry);
    int tower_size(int tower);
    int tower_scroll(int tower);
    bool intower(void);
    int tower_row(int rx, int ry);

    bool load(std::string& _path);
    bool save(std::string& _path);
    void generatecustomminimap();
    int toweredgetile(int x, int y);
    int edgetile(int x, int y);
    int warpzoneedgetile(int x, int y);
    int outsideedgetile(int x, int y);

    int backedgetile(int x, int y);

    int labspikedir(int x, int y, int t);
    int spikebase(int x, int y);
    int spikedir(int x, int y);
    int towerspikedir(int x, int y);
    int findtrinket(int t);
    int findcoin(int t);
    int findcrewmate(int t);
    int findwarptoken(int t);
    std::string warptokendest(int t);
    void findstartpoint();
    int getlevelcol(int t);
    int getenemycol(int t);
    int entcol = 0;
    Uint32 entcolreal = 0;

    //Colouring stuff
    int getwarpbackground(int rx, int ry);

  std::vector<std::string> getLevelDirFileNames( );
  static const int maxwidth = 100, maxheight = 100; //Special; the physical max the engine allows
  static const int numrooms = maxwidth * maxheight;
  int contents[40 * 30 * numrooms];
  int vmult[30 * maxheight];
    int numtrinkets();
    int numcrewmates();
    int numcoins();
  edlevelclass level[numrooms]; //Maxwidth*maxheight
  int kludgewarpdir[numrooms]; //Also maxwidth*maxheight

    int notedelay = 0;
    int oldnotedelay = 0;
    std::string note;
    std::string keybuffer;
    std::string filename;

    int drawmode = 0;
    int tilex = 0;
    int tiley = 0;
    int keydelay = 0;
    int lclickdelay = 0;
    bool savekey, loadkey = false;
    int levx = 0;
    int levy = 0;
    int levaltstate = 0;
    int entframe = 0;
    int entframedelay = 0;

    enum textmode textmod; // In text entry
    std::string *textptr; // Pointer to text we're changing
    std::string textdesc; // Description (for editor mode text fields)
    std::string oldenttext; // Old text content
    int textcount; // Level description row, or activity zone parameter
    int textent; // Entity ID for text prompt

    int lastentcycle;
    int entcycle;

    bool xmod = false;
    bool zmod = false;
    bool cmod = false;
    bool vmod = false;
    bool bmod = false;
    bool hmod = false;
    bool spacemod = false;
    bool warpmod = false;
    bool textentry = false;
    bool titlemod = false;
    bool trialnamemod = false;
    bool creatormod = false;
    bool desc1mod = false;
    bool desc2mod = false;
    bool desc3mod = false;
    bool websitemod = false;

    bool trialstartpoint = false;

    int edtrial = 0;

    int roomnamehide = 0;
    bool saveandquit = false;
    bool shiftmenu = false;
    bool shiftkey = false;
    int spacemenu = 0;
    bool settingsmod = false;
    bool settingskey = false;
    bool trialmod = false;
    int warpent = 0;
    bool updatetiles = false;
    bool changeroom = false;
    int deletekeyheld = 0;

    int boundarymod = 0;
    int boundarytype = 0;
    int boundx1 = 0;
    int boundx2 = 0;
    int boundy1 = 0;
    int boundy2 = 0;

    int levmusic = 0;
    int mapwidth = 0;
    int mapheight = 0; //Actual width and height of stage

    int version = 0;
    int vceversion = 0;

    //Script editor stuff
    void removeline(int t);
    void insertline(int t);

    bool scripteditmod = false;
    int scripthelppage = 0;
    int scripthelppagedelay = 0;
    std::vector<std::string> sb;
    std::string sbscript;
    int sbx = 0;
    int sby = 0;
    int pagey = 0;

    std::string author;
    std::string description;
    std::string title;

    //Functions for interfacing with the script:
    void addhook(std::string t);
    void removehook(std::string t);
    void addhooktoscript(std::string t);
    void removehookfromscript(std::string t);
    void loadhookineditor(std::string t);
    void clearscriptbuffer();
    void gethooks();
    bool checkhook(std::string t);
    std::vector<std::string> hooklist;

    int hookmenupage = 0;
    int hookmenu = 0;

    //Direct Mode variables
    int dmtile = 0;
    int dmtileeditor = 0;

    std::vector<edaltstate> altstates;
    std::vector<edtower> towers;
    std::vector<customtrial> customtrials;

    int ypos; // tower mode y position

    int getedaltstatenum(int rxi, int ryi, int state);
    void addaltstate(int rxi, int ryi, int state);
    void removealtstate(int rxi, int ryi, int state);
    int getnumaltstates(int rxi, int ryi);

    int entspeed = 0;

    int gettowerplattile(int col);

    std::vector<GhostInfo> ghosts;
    std::vector<Dimension> dimensions;

    int currentghosts = 0;

    void switchroomsinput();

    Uint32 getonewaycol(const int rx, const int ry);
    Uint32 getonewaycol();
    bool onewaycol_override = false;

    int getcustomtiles(int rx, int ry);
    int getcustomtiles();
    int getcustomsprites(int rx, int ry);
    int getcustomsprites();

    int returneditoralpha = 0;
    int oldreturneditoralpha = 0;

    int cursor_x = 0;
    int cursor_y = 0;
};

void addedentity(int xp, int yp, int tp, int p1=0, int p2=0, int p3=0, int p4=0, int p5=320, int p6=240);

void removeedentity(int t);

int edentat(int x, int y, int state = 0, int tower = 0);


bool edentclear(int x, int y, int state = 0, int tower = 0);

void fillbox(int x, int y, int x2, int y2, int c);

void fillboxabs(int x, int y, int x2, int y2, int c);

int dmcap(void);
int dmwidth(void);

#if !defined(NO_EDITOR)
void editorrender();

void editorlogic();

void editorinput();
#endif

extern editorclass ed;

#endif /* EDITOR_H */

#endif /* NO_CUSTOM_LEVELS */
