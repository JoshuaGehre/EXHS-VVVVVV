#if !defined(NO_CUSTOM_LEVELS)

#ifdef __APPLE__
#include "Game.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#else
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "Game.h"
#endif

#include "editor.h"

#include "Graphics.h"
#include "Entity.h"
#include "Music.h"
#include "KeyPoll.h"
#include "Map.h"
#include "Script.h"
#include "UtilityClass.h"
#include "time.h"
#include "Utilities.h"

#include "tinyxml2.h"

#include "Enums.h"

#include "FileSystemUtils.h"

#include <string>
#include <string_view>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utf8/checked.h>
#include <physfs.h>
#include <iterator>
#include <iostream>

#include <inttypes.h>
#include <cstdio>

edlevelclass::edlevelclass()
{
    tileset=0;
    tilecol=0;
    roomname="";
    warpdir=0;
    platx1=0;
    platy1=0;
    platx2=320;
    platy2=240;
    platv=4;
    enemyv=4;
    enemyx1=0;
    enemyy1=0;
    enemyx2=320;
    enemyy2=240;
    enemytype=0;
    directmode=0;
    tower=0;
    tower_row=0;
}

edaltstate::edaltstate()
{
    reset();
}

void edaltstate::reset()
{
    x = -1;
    y = -1;
    state = -1;
    tiles.resize(40 * 30);
}

edtower::edtower() {
    reset();
}

void edtower::reset(void) {
    size = 40;
    scroll = 0;
    tiles.resize(40 * size);
    int x, y;
    for (x = 0; x < 40; x++)
        for (y = 0; y < size; y++)
            tiles[x + y*40] = 0;
}

editorclass::editorclass()
{
    //We create a blank map
    for (int j = 0; j < 30 * maxwidth; j++)
    {
        for (int i = 0; i < 40 * maxheight; i++)
        {
            contents.push_back(0);
        }
    }

    for (int i = 0; i < 30 * maxheight; i++)
    {
        vmult.push_back(int(i * 40 * maxwidth));
    }

    altstates.resize(500);
    towers.resize(400);
    level.resize(maxwidth * maxheight);
    kludgewarpdir.resize(maxwidth * maxheight);

    entspeed = 0;

    reset();
}

// comparison, not case sensitive.
bool compare_nocase (std::string first, std::string second)
{
    unsigned int i=0;
    while ( (i<first.length()) && (i<second.length()) )
    {
        if (tolower(first[i])<tolower(second[i]))
            return true;
        else if (tolower(first[i])>tolower(second[i]))
            return false;
        ++i;
    }
    if (first.length()<second.length())
        return true;
    else
        return false;
}

void editorclass::loadZips()
{
    directoryList = FILESYSTEM_getLevelDirFileNames();
    bool needsReload = false;

    for(size_t i = 0; i < directoryList.size(); i++)
    {
        if (endsWith(directoryList[i], ".zip")) {
            PHYSFS_File* zip = PHYSFS_openRead(directoryList[i].c_str());
            if (!PHYSFS_mountHandle(zip, directoryList[i].c_str(), "levels", 1)) {
                printf("%s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            } else {
                needsReload = true;
            }
        }
    }

    if (needsReload) directoryList = FILESYSTEM_getLevelDirFileNames();
}

void replace_all(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t start_pos = 0;

    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); //In case `to` contains `from`, like replacing 'x' with 'yx'
    }
}

std::string find_tag(const std::string& buf, const std::string& start, const std::string& end)
{
    size_t tag = buf.find(start);

    if (tag == std::string::npos)
    {
        //No start tag
        return "";
    }

    size_t tag_start = tag + start.size();
    size_t tag_close = buf.find(end, tag_start);

    if (tag_close == std::string::npos)
    {
        //No close tag
        return "";
    }

    size_t tag_len = tag_close - tag_start;
    std::string value(buf.substr(tag_start, tag_len));

    //Encode special XML entities
    replace_all(value, "&quot;", "\"");
    replace_all(value, "&amp;", "&");
    replace_all(value, "&apos;", "'");
    replace_all(value, "&lt;", "<");
    replace_all(value, "&gt;", ">");

    //Encode general XML entities
    size_t start_pos = 0;
    while ((start_pos = value.find("&#", start_pos)) != std::string::npos)
    {
        bool hex = value[start_pos + 2] == 'x';
        size_t end = value.find(';', start_pos);
        size_t real_start = start_pos + 2 + ((int) hex);
        std::string number(value.substr(real_start, end - real_start));

        if (!is_positive_num(number, hex))
        {
            return "";
        }

        uint32_t character = 0;
        if (hex)
        {
            sscanf(number.c_str(), "%" SCNx32, &character);
        }
        else
        {
            sscanf(number.c_str(), "%" SCNu32, &character);
        }
        uint32_t utf32[] = {character, 0};
        std::string utf8;
        utf8::utf32to8(utf32, utf32 + 1, std::back_inserter(utf8));
        value.replace(start_pos, end - start_pos + 1, utf8);
    }

    return value;
}

#define TAG_FINDER(NAME, TAG) \
std::string NAME(const std::string& buf) \
{ \
    return find_tag(buf, "<" TAG ">", "</" TAG ">"); \
}

TAG_FINDER(find_metadata, "MetaData"); //only for checking that it exists

TAG_FINDER(find_creator, "Creator");
TAG_FINDER(find_title, "Title");
TAG_FINDER(find_desc1, "Desc1");
TAG_FINDER(find_desc2, "Desc2");
TAG_FINDER(find_desc3, "Desc3");
TAG_FINDER(find_website, "website");

#undef TAG_FINDER

void editorclass::getDirectoryData()
{

    ListOfMetaData.clear();
    directoryList.clear();

    loadZips();

    for(size_t i = 0; i < directoryList.size(); i++)
    {
        if (!endsWith(directoryList[i], ".zip")) {
            LevelMetaData temp;
            if (getLevelMetaData( directoryList[i], temp))
            {
                ListOfMetaData.push_back(temp);
            }
        }
    }

    for(size_t i = 0; i < ListOfMetaData.size(); i++)
    {
        for(size_t k = 0; k < ListOfMetaData.size(); k++)
        {
            if(compare_nocase(ListOfMetaData[i].title, ListOfMetaData[k].title ))
            {
                std::swap(ListOfMetaData[i] , ListOfMetaData[k]);
                std::swap(directoryList[i], directoryList[k]);
            }
        }
    }

}
bool editorclass::getLevelMetaData(std::string& _path, LevelMetaData& _data )
{
    unsigned char *uMem = NULL;
    FILESYSTEM_loadFileToMemory(_path.c_str(), &uMem, NULL, true);

    if (uMem == NULL)
    {
        printf("Level %s not found :(\n", _path.c_str());
        return false;
    }

    std::unique_ptr<char[], free_delete> mem((char*) uMem);

    std::string buf((char*) uMem);

    if (find_metadata(buf) == "")
    {
        printf("Couldn't load metadata for %s\n", _path.c_str());
        return false;
    }

    _data.creator = find_creator(buf);
    _data.title = find_title(buf);
    _data.Desc1 = find_desc1(buf);
    _data.Desc2 = find_desc2(buf);
    _data.Desc3 = find_desc3(buf);
    _data.website = find_website(buf);

    _data.filename = _path;
    return true;
}

void editorclass::reset()
{
    version=2; //New smaller format change is 2
    vceversion=VCEVERSION;

    mapwidth=5;
    mapheight=5;

    EditorData::GetInstance().title="Untitled Level";
    EditorData::GetInstance().creator="Unknown";
    Desc1="";
    Desc2="";
    Desc3="";
    website="";

    roomnamehide=0;
    zmod=false;
    xmod=false;
    cmod=false;
    vmod=false;
    hmod=false;
    bmod=false;
    spacemod=false;
    spacemenu=0;
    shiftmenu=false;
    shiftkey=false;
    saveandquit=false;
    note="";
    notedelay=0;
    oldnotedelay=0;
    textentry=false;
    deletekeyheld=false;
    textmod = TEXT_NONE;

    entcycle = 0;
    lastentcycle = 0;

    trialnamemod=false;
    titlemod=false;
    creatormod=false;
    desc1mod=false;
    desc2mod=false;
    desc3mod=false;
    websitemod=false;
    settingsmod=false;
    trialmod=false;
    warpmod=false; //Two step process
    warpent=-1;

    boundarymod=0;
    boundarytype=0;
    boundx1=0;
    boundx2=0;
    boundy1=0;
    boundy2=0;

    drawmode=0;
    dmtile=0;
    dmtileeditor=0;
    entcol=0;

    tilex=0;
    tiley=0;
    levx=0;
    levy=0;
    levaltstate=0;
    keydelay=0;
    lclickdelay=0;
    savekey=false;
    loadkey=false;
    updatetiles=true;
    changeroom=true;
    levmusic=0;

    trialstartpoint = false;
    edtrial = 0;

    entframe=0;
    entframedelay=0;

    edentity.clear();
    levmusic=0;

    for (int j = 0; j < maxheight; j++)
    {
        for (int i = 0; i < maxwidth; i++)
        {
            level[i+(j*maxwidth)].tileset=0;
            level[i+(j*maxwidth)].tilecol=(i+j)%32;
            level[i+(j*maxwidth)].roomname="";
            level[i+(j*maxwidth)].warpdir=0;
            level[i+(j*maxwidth)].platx1=0;
            level[i+(j*maxwidth)].platy1=0;
            level[i+(j*maxwidth)].platx2=320;
            level[i+(j*maxwidth)].platy2=240;
            level[i+(j*maxwidth)].platv=4;
            level[i+(j*maxwidth)].enemyv=4;
            level[i+(j*maxwidth)].enemyx1=0;
            level[i+(j*maxwidth)].enemyy1=0;
            level[i+(j*maxwidth)].enemyx2=320;
            level[i+(j*maxwidth)].enemyy2=240;
            level[i+(j*maxwidth)].enemytype=0;
            level[i+(j*maxwidth)].directmode=0;
            level[i+(j*maxwidth)].tower=0;
            level[i+(j*maxwidth)].tower_row=0;
            kludgewarpdir[i+(j*maxwidth)]=0;
        }
    }

    for (int j = 0; j < 30 * maxheight; j++)
    {
        for (int i = 0; i < 40 * maxwidth; i++)
        {
            contents[i+(j*40*maxwidth)]=0;
        }
    }

    hooklist.clear();

    sb.clear();

    clearscriptbuffer();
    sbx=0;
    sby=0;
    pagey=0;
    scripteditmod=false;
    sbscript="null";
    scripthelppage=0;
    scripthelppagedelay=0;

    hookmenupage=0;
    hookmenu=0;
    script.customscripts.clear();

    grayenemieskludge = false;

    for (size_t i = 0; i < altstates.size(); i++)
        altstates[i].reset();
    for (size_t i = 0; i < towers.size(); i++)
        towers[i].reset();

    edentity.clear();

    returneditoralpha = 0;
    oldreturneditoralpha = 0;

    customtrials.clear();
    dimensions.clear();
    ghosts.clear();
}

void editorclass::gethooks()
{
    //Scan through the script and create a hooks list based on it
    hooklist.clear();
    for (auto& script_ : script.customscripts)
        hooklist.push_back(script_.name);
}

void editorclass::loadhookineditor(std::string t)
{
    //Find hook t in the scriptclass, then load it into the editor
    clearscriptbuffer();

    for (auto& script_ : script.customscripts)
        if (script_.name == t) {
            sb = script_.contents;
            break;
        }
    if(sb.empty())
    {
        //Always have one line or we'll have problems
        sb.resize(1);
    }
}

void editorclass::addhooktoscript(std::string t)
{
    //Adds hook+the scriptbuffer to the end of the scriptclass
    removehookfromscript(t);
    script.customscripts.push_back(Script{t, sb});
}

void editorclass::removehookfromscript(std::string t)
{
    //Find hook t in the scriptclass, then removes it (and any other code with it)
    script.customscripts.erase(std::remove_if(script.customscripts.begin(), script.customscripts.end(), [&](auto& x){return x.name == t;}), script.customscripts.end());
}

void editorclass::removehook(std::string t)
{
    //Check the hooklist for the hook t. If it's there, remove it from here and the script
    removehookfromscript(t);
    hooklist.erase(std::remove(hooklist.begin(), hooklist.end(), t), hooklist.end());
}

void editorclass::addhook(std::string t)
{
    //Add an empty function to the list in both editor and script
    removehook(t);
    hooklist.push_back(t);
    addhooktoscript(t);
}

bool editorclass::checkhook(std::string t)
{
    //returns true if hook t already is in the list
    for(size_t i=0; i<hooklist.size(); i++)
    {
        if(hooklist[i]==t) return true;
    }
    return false;
}


void editorclass::clearscriptbuffer()
{
    sb.clear();
}

void editorclass::removeline(int t)
{
    //Remove line t from the script
    if((int)sb.size()>1)
    {
        sb.erase(sb.begin() + t);
    }
}

void editorclass::insertline(int t)
{
    //insert a blank line into script at line t
    sb.insert(sb.begin() + t, "");
}

void editorclass::getlin(enum textmode mode, std::string prompt, std::string *ptr) {
    ed.textmod = mode;
    ed.textptr = ptr;
    ed.textdesc = prompt;
    key.enabletextentry();
    if (ptr)
        key.keybuffer = *ptr;
    else {
        key.keybuffer = "";
        ed.textptr = &(key.keybuffer);
    }

    ed.oldenttext = key.keybuffer;
}

std::vector<int> editorclass::loadlevel( int rxi, int ryi, int altstate )
{
    //Set up our buffer array to be picked up by mapclass
    rxi -= 100;
    ryi -= 100;
    if (rxi < 0) rxi += mapwidth;
    if (ryi < 0) ryi += mapheight;
    if (rxi >= mapwidth) rxi -= mapwidth;
    if (ryi >= mapheight) ryi -= mapheight;
    std::vector<int> result;

    int tower = get_tower(rxi, ryi);

    if (tower) {
        result = towers[tower-1].tiles;

        return result;
    }

    int thisstate = -1;
    if (altstate != 0)
        thisstate = getedaltstatenum(rxi, ryi, altstate);

    if (thisstate == -1) { // Didn't find the alt state, or not using one
        for (int j = 0; j < 30; j++)
            for (int i = 0; i < 40; i++)
                result.push_back(contents[i+(rxi*40)+vmult[j+(ryi*30)]]);
    } else {
        result = altstates[thisstate].tiles;
    }

    return result;
}

int editorclass::getlevelcol(int t)
{
    if(level[t].tileset==0)  //Station
    {
        if (level[t].tilecol == -1)
            // Fix gray enemies
            grayenemieskludge = true;
        return level[t].tilecol;
    }
    else if(level[t].tileset==1)   //Outside
    {
        return 32+level[t].tilecol;
    }
    else if(level[t].tileset==2)   //Lab
    {
        return 40+level[t].tilecol;
    }
    else if(level[t].tileset==3)   //Warp Zone
    {
        if (level[t].tilecol == 6)
            // Fix gray enemies
            grayenemieskludge = true;
        return 46+level[t].tilecol;
    }
    else if(level[t].tileset==4)   //Ship
    {
        return 52+level[t].tilecol;
    }
    else if (level[t].tileset==5)   //Tower
    {
        // WARNING: This is duplicated in mapclass::updatetowerentcol()!
        return 58 + level[t].tilecol/5;
    }
    return 0;
}

int editorclass::getenemycol(int t)
{
    switch(t)
    {
        //RED
    case 3:
    case 7:
    case 12:
    case 23:
    case 28:
    case 34:
    case 42:
    case 48:
    case 58:
    case 59:
        return 6;
        break;
        //GREEN
    case 5:
    case 9:
    case 22:
    case 25:
    case 29:
    case 31:
    case 38:
    case 46:
    case 52:
    case 53:
    case 61:
        return 7;
        break;
        //BLUE
    case 1:
    case 6:
    case 14:
    case 27:
    case 33:
    case 44:
    case 50:
    case 57:
        return 12;
        break;
        //YELLOW
    case 4:
    case 17:
    case 24:
    case 30:
    case 37:
    case 45:
    case 51:
    case 55:
    case 60:
        return 9;
        break;
        //PURPLE
    case 2:
    case 11:
    case 15:
    case 19:
    case 32:
    case 36:
    case 49:
    case 63:
        return 20;
        break;
        //CYAN
    case 8:
    case 10:
    case 13:
    case 18:
    case 26:
    case 35:
    case 41:
    case 47:
    case 54:
    case 62:
        return 11;
        break;
        //PINK
    case 16:
    case 20:
    case 39:
    case 43:
    case 56:
    case 64:
        return 8;
        break;
        //ORANGE
    case 21:
    case 40:
        return 17;
        break;
    default:
        return 6;
        break;
    }
    return 0;
}

int editorclass::getwarpbackground(int rx, int ry)
{
    int tmp=rx+(maxwidth*ry);
    switch(level[tmp].tileset)
    {
    case 0: //Space Station
        switch(level[tmp].tilecol)
        {
        case 0:
            return 3;
            break;
        case 1:
            return 2;
            break;
        case 2:
            return 1;
            break;
        case 3:
            return 4;
            break;
        case 4:
            return 5;
            break;
        case 5:
            return 3;
            break;
        case 6:
            return 1;
            break;
        case 7:
            return 0;
            break;
        case 8:
            return 5;
            break;
        case 9:
            return 0;
            break;
        case 10:
            return 2;
            break;
        case 11:
            return 1;
            break;
        case 12:
            return 5;
            break;
        case 13:
            return 0;
            break;
        case 14:
            return 3;
            break;
        case 15:
            return 2;
            break;
        case 16:
            return 4;
            break;
        case 17:
            return 0;
            break;
        case 18:
            return 3;
            break;
        case 19:
            return 1;
            break;
        case 20:
            return 4;
            break;
        case 21:
            return 5;
            break;
        case 22:
            return 1;
            break;
        case 23:
            return 4;
            break;
        case 24:
            return 5;
            break;
        case 25:
            return 0;
            break;
        case 26:
            return 3;
            break;
        case 27:
            return 1;
            break;
        case 28:
            return 5;
            break;
        case 29:
            return 4;
            break;
        case 30:
            return 5;
            break;
        case 31:
            return 2;
            break;
        default:
            return 6;
            break;
        }
        break;
    case 1: //Outside
        switch(level[tmp].tilecol)
        {
        case 0:
            return 3;
            break;
        case 1:
            return 1;
            break;
        case 2:
            return 0;
            break;
        case 3:
            return 2;
            break;
        case 4:
            return 4;
            break;
        case 5:
            return 5;
            break;
        case 6:
            return 2;
            break;
        case 7:
            return 4;
            break;
        default:
            return 6;
            break;
        }
        break;
    case 2: //Lab
        switch(level[tmp].tilecol)
        {
        case 0:
            return 0;
            break;
        case 1:
            return 1;
            break;
        case 2:
            return 2;
            break;
        case 3:
            return 3;
            break;
        case 4:
            return 4;
            break;
        case 5:
            return 5;
            break;
        case 6:
            return 6;
            break;
        default:
            return 6;
            break;
        }
        break;
    case 3: //Warp Zone
        switch(level[tmp].tilecol)
        {
        case 0:
            return 0;
            break;
        case 1:
            return 1;
            break;
        case 2:
            return 2;
            break;
        case 3:
            return 3;
            break;
        case 4:
            return 4;
            break;
        case 5:
            return 5;
            break;
        case 6:
            return 6;
            break;
        default:
            return 6;
            break;
        }
        break;
    case 4: //Ship
        switch(level[tmp].tilecol)
        {
        case 0:
            return 5;
            break;
        case 1:
            return 0;
            break;
        case 2:
            return 4;
            break;
        case 3:
            return 2;
            break;
        case 4:
            return 3;
            break;
        case 5:
            return 1;
            break;
        case 6:
            return 6;
            break;
        default:
            return 6;
            break;
        }
        break;
    case 5: //Tower
        temp = (level[tmp].tilecol) / 5;
        switch(temp)
        {
        case 0:
            return 1;
            break;
        case 1:
            return 4;
            break;
        case 2:
            return 5;
            break;
        case 3:
            return 0;
            break;
        case 4:
            return 3;
            break;
        case 5:
            return 2;
            break;
        default:
            return 6;
            break;
        }
        break;
    default:
        return 6;
        break;
    }
}

int editorclass::getenemyframe(int t, int dir)
{
    switch(t)
    {
    case 0:
        return 78;
        break;
    case 1:
        return 88;
        break;
    case 2:
        return 36;
        break;
    case 3:
        return 164;
        break;
    case 4:
        return 68;
        break;
    case 5:
        return 48;
        break;
    case 6:
        return 176;
        break;
    case 7:
        return 168;
        break;
    case 8:
        return 112;
        break;
    case 9:
        return 114;
        break;
    case 10:
        return 92;
        break;
    case 11:
        return 40;
        break;
    case 12:
        return 28;
        break;
    case 13:
        return 32;
        break;
    case 14:
        return 100;
        break;
    case 15:
        return 52;
        break;
    case 16:
        return dir == 2 ? 66 : 54;
        break;
    case 17:
        return 51;
        break;
    case 18:
        return dir == 2 ? 160 : 156;
        break;
    case 19:
        return 44;
        break;
    case 20:
        return 106;
        break;
    case 21:
        return 82;
        break;
    case 22:
        return 116;
        break;
    case 23:
        return 64;
        break;
    case 24:
        return 56;
        break;
    case 25:
        return 172;
        break;
    case 26:
        return 24;
        break;
    case 27:
        return 120;
        break;
    default:
        return 78;
        break;
    }
    return 78;
}


void editorclass::placetile( int x, int y, int t )
{
    // Unused, no need to add altstates support to this function
    if(x>=0 && y>=0 && x<mapwidth*40 && y<mapheight*30)
    {
        contents[x+(levx*40)+vmult[y+(levy*30)]]=t;
    }
}

void editorclass::placetilelocal( int x, int y, int t )
{
    if(x>=0 && y>=0 && x<40 && y<30)
        settilelocal(x, y, t);
    updatetiles=true;
}

int editorclass::gettilelocal(int x, int y)
{
    int tower = get_tower(levx, levy);
    if (tower) {
        y += ypos;

        // Show spikes beyond the tower boundaries
        if (y < 0)
            return 159;
        if (y >= tower_size(tower))
            return 158;

        // Mark tower entry point for current screen with green
        int tile = towers[tower-1].tiles[x + y*40];
        int entrypos = level[levx + levy*maxwidth].tower_row;
        if (y >= entrypos && y <= (entrypos + 29) && tile)
            tile += 300;

        return tile;
    }

    if (levaltstate == 0)
        return contents[x + levx*40 + vmult[y + levy*30]];
    else
        return altstates[getedaltstatenum(levx, levy, levaltstate)].tiles[x + y*40];
}

void editorclass::settilelocal(int x, int y, int tile)
{
    int tower = get_tower(levx, levy);
    if (tower) {
        y += ypos;

        upsize_tower(tower, y);
        if (y < 0)
            y = 0;

        towers[tower-1].tiles[x + y*40] = tile % 30;
        downsize_tower(tower);
    } else if (levaltstate == 0)
        contents[x + levx*40 + vmult[y + levy*30]] = tile;
    else
        altstates[getedaltstatenum(levx, levy, levaltstate)].tiles[x + y*40] = tile;
}

int editorclass::base( int x, int y )
{
    //Return the base tile for the given tileset and colour
    int temp=x+(y*maxwidth);
    if(level[temp].tileset==0)  //Space Station
    {
        if(level[temp].tilecol>=22)
        {
            return 483 + ((level[temp].tilecol-22)*3);
        }
        else if(level[temp].tilecol>=11)
        {
            return 283 + ((level[temp].tilecol-11)*3);
        }
        else
        {
            return 83 + (level[temp].tilecol*3);
        }
    }
    else if(level[temp].tileset==1)   //Outside
    {
        return 480 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==2)   //Lab
    {
        return 280 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==3)   //Warp Zone/Intermission
    {
        return 80 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==4)   //SHIP
    {
        return 101 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==5)   //Tower
    {
        return 12 + (level[temp].tilecol*30);
    }
    return 0;
}

int editorclass::backbase( int x, int y )
{
    //Return the base tile for the background of the given tileset and colour
    int temp=x+(y*maxwidth);
    if(level[temp].tileset==0)  //Space Station
    {
        //Pick depending on tilecol
        switch(level[temp].tilecol)
        {
        case 0:
        case 5:
        case 26:
            return 680; //Blue
            break;
        case 3:
        case 16:
        case 23:
            return 683; //Yellow
            break;
        case 9:
        case 12:
        case 21:
            return 686; //Greeny Cyan
            break;
        case 4:
        case 8:
        case 24:
        case 28:
        case 30:
            return 689; //Green
            break;
        case 20:
        case 29:
            return 692; //Orange
            break;
        case 2:
        case 6:
        case 11:
        case 22:
        case 27:
            return 695; //Red
            break;
        case 1:
        case 10:
        case 15:
        case 19:
        case 31:
            return 698; //Pink
            break;
        case 14:
        case 18:
            return 701; //Dark Blue
            break;
        case 7:
        case 13:
        case 17:
        case 25:
            return 704; //Cyan
            break;
        default:
            return 680;
            break;
        }

    }
    else if(level[temp].tileset==1)   //outside
    {
        return 680 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==2)   //Lab
    {
        return 0;
    }
    else if(level[temp].tileset==3)   //Warp Zone/Intermission
    {
        return 120 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==4)   //SHIP
    {
        return 741 + (level[temp].tilecol*3);
    }
    else if(level[temp].tileset==5)   //Tower
    {
        return 28 + (level[temp].tilecol*30);
    }
    return 0;
}

enum tiletyp
editorclass::gettiletyplocal(int x, int y)
{
    return gettiletyp(level[levx + levy*maxwidth].tileset, at(x, y));
}

enum tiletyp
editorclass::getabstiletyp(int x, int y)
{
    int tile = absat(&x, &y);
    int room = x / 40 + ((y / 30)*maxwidth);

    return gettiletyp(level[room].tileset, tile);
}

enum tiletyp
editorclass::gettiletyp(int tileset, int tile)
{
    if (tile == 0)
        return TILE_NONE;

    if (tileset == 5) {
        tile = tile % 30;
        if (tile >= 6 && tile <= 11)
            return TILE_SPIKE;
        if (tile >= 12 && tile <= 27)
            return TILE_FOREGROUND;
        return TILE_BACKGROUND;
    }

    // non-space station has more spikes
    int lastspike = 50;
    if (tileset != 0)
        lastspike = 74;

    if ((tile >= 6 && tile <= 9) || (tile >= 49 && tile <= lastspike))
        return TILE_SPIKE;
    if (tile == 1 || (tile >= 80 && tile <= 679))
        return TILE_FOREGROUND;
    return TILE_BACKGROUND;
}

int editorclass::at( int x, int y )
{
    if(x<0) return at(0,y);
    if(y<0) return at(x,0);
    if(x>=40) return at(39,y);
    if(y>=30) return at(x,29);

    return gettilelocal(x, y);
}

int
editorclass::absat(int *x, int *y)
{
    if (*x < 0) *x = (*x) +mapwidth*40;
    if (*y < 0) *y = (*y) +mapheight*30;
    if (*x >= (mapwidth*40)) *x = (*x) - mapwidth*40;
    if (*y >= (mapheight*30)) *y = (*y) - mapheight*30;
    return contents[(*x) + vmult[*y]];
}
int editorclass::freewrap( int x, int y )
{
    temp = getabstiletyp(x, y);
    if (temp != TILE_FOREGROUND) return 0;
    return 1;
}

int editorclass::backonlyfree( int x, int y )
{
    //Returns 1 if tile is a background tile, 0 otherwise
    temp = gettiletyplocal(x, y);
    if (temp == TILE_BACKGROUND)
        return 1;
    return 0;
}

int editorclass::backfree( int x, int y )
{
    //Returns 1 if tile is nonzero
    if (gettiletyplocal(x, y) == TILE_NONE)
        return 0;
    return 1;
}

int editorclass::towerspikefree(int x, int y) {
    // Uses absolute y in tower mode
    int tower = get_tower(levx, levy);
    int size = tower_size(tower);
    if (!intower())
        return spikefree(x, y);

    if (x == -1) x = 0;
    if (x == 40) x = 39;
    if (y == -1) y = 0;
    if (y >= size) y = size - 1;

    int tile = towers[tower-1].tiles[x + y*40];
    temp = gettiletyp(level[levx + levy * maxwidth].tileset, tile);
    if (temp == TILE_FOREGROUND || temp == TILE_BACKGROUND || temp == TILE_SPIKE)
        return 1;
    return 0;
}

int editorclass::spikefree(int x, int y) {
    //Returns 0 if tile is not a block or spike, 1 otherwise
    if (x == -1) x = 0;
    if (x == 40) x = 39;
    if (y == -1) y = 0;
    if (y == 30) y = 29;

    temp = gettiletyplocal(x, y);
    if (temp == TILE_FOREGROUND || temp == TILE_BACKGROUND || temp == TILE_SPIKE)
        return 1;
    return 0;
}

int editorclass::getfree(enum tiletyp thistiletyp)
{
    //Returns 0 if tile is not a block, 1 otherwise
    if (thistiletyp != TILE_FOREGROUND)
        return 0;
    return 1;
}

int editorclass::towerfree(int x, int y) {
    // Uses absolute y in tower mode
    int tower = get_tower(levx, levy);
    int size = tower_size(tower);
    if (!intower())
        return free(x, y);

    if (x == -1) x = 0;
    if (x == 40) x = 39;
    if (y == -1) y = 0;
    if (y >= size) y = size - 1;

    int tile = towers[tower-1].tiles[x + y*40];
    return getfree(gettiletyp(level[levx + levy * maxwidth].tileset,
                              tile));
}

int editorclass::free(int x, int y) {
    //Returns 0 if tile is not a block, 1 otherwise
    if (x == -1) x = 0;
    if (x == 40) x = 39;
    if (y == -1) y = 0;
    if (y == 30) y = 29;

    return getfree(gettiletyplocal(x, y));
}

int editorclass::absfree( int x, int y )
{
    //Returns 0 if tile is not a block, 1 otherwise, abs on grid
    if(x>=0 && y>=0 && x<mapwidth*40 && y<mapheight*30)
        return getfree(getabstiletyp(x, y));
    return 1;
}

int editorclass::match( int x, int y )
{
    if (intower())
        y += ypos;

    if(towerfree(x-1,y)==0 && towerfree(x,y-1)==0 &&
       towerfree(x+1,y)==0 && towerfree(x,y+1)==0) return 0;

    if(towerfree(x-1,y)==0 && towerfree(x,y-1)==0) return 10;
    if(towerfree(x+1,y)==0 && towerfree(x,y-1)==0) return 11;
    if(towerfree(x-1,y)==0 && towerfree(x,y+1)==0) return 12;
    if(towerfree(x+1,y)==0 && towerfree(x,y+1)==0) return 13;

    if(towerfree(x,y-1)==0) return 1;
    if(towerfree(x-1,y)==0) return 2;
    if(towerfree(x,y+1)==0) return 3;
    if(towerfree(x+1,y)==0) return 4;
    if(towerfree(x-1,y-1)==0) return 5;
    if(towerfree(x+1,y-1)==0) return 6;
    if(towerfree(x-1,y+1)==0) return 7;
    if(towerfree(x+1,y+1)==0) return 8;

    return 0;
}

int editorclass::warpzonematch( int x, int y )
{
    if(free(x-1,y)==0 && free(x,y-1)==0 && free(x+1,y)==0 && free(x,y+1)==0) return 0;

    if(free(x-1,y)==0 && free(x,y-1)==0) return 10;
    if(free(x+1,y)==0 && free(x,y-1)==0) return 11;
    if(free(x-1,y)==0 && free(x,y+1)==0) return 12;
    if(free(x+1,y)==0 && free(x,y+1)==0) return 13;

    if(free(x,y-1)==0) return 1;
    if(free(x-1,y)==0) return 2;
    if(free(x,y+1)==0) return 3;
    if(free(x+1,y)==0) return 4;
    if(free(x-1,y-1)==0) return 5;
    if(free(x+1,y-1)==0) return 6;
    if(free(x-1,y+1)==0) return 7;
    if(free(x+1,y+1)==0) return 8;

    return 0;
}

int editorclass::outsidematch( int x, int y )
{

    if(backonlyfree(x-1,y)==0 && backonlyfree(x+1,y)==0) return 2;
    if(backonlyfree(x,y-1)==0 && backonlyfree(x,y+1)==0) return 1;

    return 0;
}

int editorclass::backmatch( int x, int y )
{
    //Returns the first position match for a border
    // 5 1 6
    // 2 X 4
    // 7 3 8
    if(backfree(x-1,y)==0 && backfree(x,y-1)==0 && backfree(x+1,y)==0 && backfree(x,y+1)==0) return 0;

    if(backfree(x-1,y)==0 && backfree(x,y-1)==0) return 10;
    if(backfree(x+1,y)==0 && backfree(x,y-1)==0) return 11;
    if(backfree(x-1,y)==0 && backfree(x,y+1)==0) return 12;
    if(backfree(x+1,y)==0 && backfree(x,y+1)==0) return 13;

    if(backfree(x,y-1)==0) return 1;
    if(backfree(x-1,y)==0) return 2;
    if(backfree(x,y+1)==0) return 3;
    if(backfree(x+1,y)==0) return 4;
    if(backfree(x-1,y-1)==0) return 5;
    if(backfree(x+1,y-1)==0) return 6;
    if(backfree(x-1,y+1)==0) return 7;
    if(backfree(x+1,y+1)==0) return 8;

    return 0;
}

int editorclass::toweredgetile(int x, int y)
{
    switch(match(x,y))
    {
    case 14: // true center
        return 0;
        break;
    case 10: // top left
        return 5;
        break;
    case 11: // top right
        return 7;
        break;
    case 12: // bottom left
        return 10;
        break;
    case 13: // bottom right
        return 12;
        break;
    case 1: // top center
        return 6;
        break;
    case 2: // center left
        return 8;
        break;
    case 3: // bottom center
        return 11;
        break;
    case 4: // center right
        return 9;
        break;
    case 5: // reversed bottom right edge
        return 4;
        break;
    case 6: // reversed bottom left edge
        return 3;
        break;
    case 7: // reversed top right edge
        return 2;
        break;
    case 8: // reversed top left edge
        return 1;
        break;
    case 0:
    default:
        return 0;
        break;
    }
    return 0;
}
int editorclass::edgetile( int x, int y )
{
    switch(match(x,y))
    {
    case 14: // true center
        return 0;
        break;
    case 10: // top left
        return 80;
        break;
    case 11: // top right
        return 82;
        break;
    case 12: // bottom left
        return 160;
        break;
    case 13: // bottom right
        return 162;
        break;
    case 1: // top center
        return 81;
        break;
    case 2: // center left
        return 120;
        break;
    case 3: // bottom center
        return 161;
        break;
    case 4: // center right
        return 122;
        break;
    case 5: // reversed bottom right edge
        return 42;
        break;
    case 6: // reversed bottom left edge
        return 41;
        break;
    case 7: // reversed top right edge
        return 2;
        break;
    case 8: // reversed top left edge
        return 1;
        break;
    case 0:
    default:
        return 0;
        break;
    }
    return 0;
}

int editorclass::spikebase(int x, int y)
{
    temp=x+(y*maxwidth);
    if (level[temp].tileset==5) {
        return level[temp].tilecol * 30;
    }
    return 0;
}

int editorclass::warpzoneedgetile( int x, int y )
{
    switch(backmatch(x,y))
    {
    case 14:
        return 0;
        break;
    case 10:
        return 80;
        break;
    case 11:
        return 82;
        break;
    case 12:
        return 160;
        break;
    case 13:
        return 162;
        break;
    case 1:
        return 81;
        break;
    case 2:
        return 120;
        break;
    case 3:
        return 161;
        break;
    case 4:
        return 122;
        break;
    case 5:
        return 42;
        break;
    case 6:
        return 41;
        break;
    case 7:
        return 2;
        break;
    case 8:
        return 1;
        break;
    case 0:
    default:
        return 0;
        break;
    }
    return 0;
}

int editorclass::outsideedgetile( int x, int y )
{
    switch(outsidematch(x,y))
    {
    case 2:
        return 0;
        break;
    case 1:
        return 1;
        break;
    case 0:
    default:
        return 2;
        break;
    }
    return 2;
}


int editorclass::backedgetile( int x, int y )
{
    switch(backmatch(x,y))
    {
    case 14:
        return 0;
        break;
    case 10:
        return 80;
        break;
    case 11:
        return 82;
        break;
    case 12:
        return 160;
        break;
    case 13:
        return 162;
        break;
    case 1:
        return 81;
        break;
    case 2:
        return 120;
        break;
    case 3:
        return 161;
        break;
    case 4:
        return 122;
        break;
    case 5:
        return 42;
        break;
    case 6:
        return 41;
        break;
    case 7:
        return 2;
        break;
    case 8:
        return 1;
        break;
    case 0:
    default:
        return 0;
        break;
    }
    return 0;
}

int editorclass::labspikedir( int x, int y, int t )
{
    // a slightly more tricky case
    if(free(x,y+1)==1) return 63 + (t*2);
    if(free(x,y-1)==1) return 64 + (t*2);
    if(free(x-1,y)==1) return 51 + (t*2);
    if(free(x+1,y)==1) return 52 + (t*2);
    return 63 + (t*2);
}

int editorclass::spikedir( int x, int y )
{
    if(free(x,y+1)==1) return 8;
    if(free(x,y-1)==1) return 9;
    if(free(x-1,y)==1) return 49;
    if(free(x+1,y)==1) return 50;
    return 8;
}

int editorclass::towerspikedir(int x, int y) {
    if (intower())
        y += ypos;

    if(towerfree(x,y+1) == 1) return 8;
    if(towerfree(x,y-1) == 1) return 9;
    if(towerfree(x-1,y) == 1) return 10;
    if(towerfree(x+1,y) == 1) return 11;
    return 8;
}

void editorclass::findstartpoint()
{
    //Ok! Scan the room for the closest checkpoint
    int testeditor=-1;
    //First up; is there a start point on this screen?
    for(size_t i=0; i<edentity.size(); i++)
    {
        //if() on screen
        if(edentity[i].t==16 && testeditor==-1)
        {
            testeditor=i;
        }
    }

    if(testeditor==-1)
    {
        game.edsavex = 160;
        game.edsavey = 120;
        game.edsaverx = 100;
        game.edsavery = 100;
        game.edsavegc = 0;
        game.edsavey--;
        game.edsavedir=1;
    }
    else
    {
        //Start point spawn
        int tx=(edentity[testeditor].x-(edentity[testeditor].x%40))/40;
        int ty=(edentity[testeditor].y-(edentity[testeditor].y%30))/30;
        game.edsavex = ((edentity[testeditor].x%40)*8)-4;
        game.edsavey = (edentity[testeditor].y%30)*8;
        game.edsavex += edentity[testeditor].subx;
        game.edsavey += edentity[testeditor].suby;
        game.edsaverx = 100+tx;
        game.edsavery = 100+ty;
        game.edsavegc = 0;
        game.edsavey--;
        game.edsavedir=1-edentity[testeditor].p1;
    }
}

void editorclass::saveconvertor()
{
    // Unused, no need to add altstates support to this function

    //In the case of resizing breaking a level, this function can fix it
    maxwidth=20;
    maxheight=20;
    int oldwidth=10, oldheight=10;

    std::vector <int> tempcontents;
    for (int j = 0; j < 30 * oldwidth; j++)
    {
        for (int i = 0; i < 40 * oldheight; i++)
        {
            tempcontents.push_back(contents[i+(j*40*oldwidth)]);
        }
    }

    contents.clear();
    for (int j = 0; j < 30 * maxheight; j++)
    {
        for (int i = 0; i < 40 * maxwidth; i++)
        {
            contents.push_back(0);
        }
    }

    for (int j = 0; j < 30 * oldheight; j++)
    {
        for (int i = 0; i < 40 * oldwidth; i++)
        {
            contents[i+(j*40*oldwidth)]=tempcontents[i+(j*40*oldwidth)];
        }
    }

    tempcontents.clear();

    for (int i = 0; i < 30 * maxheight; i++)
    {
        vmult.push_back(int(i * 40 * maxwidth));
    }

    for (int j = 0; j < maxheight; j++)
    {
        for (int i = 0; i < maxwidth; i++)
        {
            level[i+(j*maxwidth)].tilecol=(i+j)%6;
        }
    }
    contents.clear();

}

int editorclass::findtrinket(int t)
{
    int ttrinket=0;
    for(int i=0; i<(int)edentity.size(); i++)
    {
        if(i==t) return ttrinket;
        if(edentity[i].t==9) ttrinket++;
    }
    return 0;
}

int editorclass::findcoin(int t)
{
    int tcoin=0;
    for(int i=0; i<(int)edentity.size(); i++)
    {
        if(i==t) return tcoin;
        if(edentity[i].t==8) tcoin++;
    }
    return 0;
}

int editorclass::findcrewmate(int t)
{
    int ttrinket=0;
    for(int i=0; i<(int)edentity.size(); i++)
    {
        if(i==t) return ttrinket;
        if(edentity[i].t==15) ttrinket++;
    }
    return 0;
}

int editorclass::findwarptoken(int t)
{
    int ttrinket=0;
    for(int i=0; i<(int)edentity.size(); i++)
    {
        if(i==t) return ttrinket;
        if(edentity[i].t==13) ttrinket++;
    }
    return 0;
}

// Returns a warp token destination room string
std::string editorclass::warptokendest(int t) {
    int ex = edentity[t].p1;
    int ey = edentity[t].p2;
    int tower = edentity[t].p3;
    std::string towerstr = "";
    int rx, ry;
    if (!tower || !find_tower(tower, rx, ry)) {
        rx = ex / 40;
        ry = ey / 30;
    } else {
        if (ey < 20)
            ey = 20;
        towerstr = "T"+help.String(tower)+":"+help.String(ey - 20);
    }

    // Rooms are 1-indexed in the editor
    rx++;
    ry++;

    return "("+help.String(rx)+","+help.String(ry)+")"+towerstr;
}

// Switches tileset
void editorclass::switch_tileset(bool reversed) {
    std::string tilesets[6] =
        {"Space Station", "Outside", "Lab", "Warp Zone", "Ship", "Tower"};
    int tiles = level[levx + levy*maxwidth].tileset;
    int oldtiles = tiles;
    if (reversed)
        tiles--;
    else
        tiles++;

    tiles = mod(tiles, 6);
    level[levx + levy*maxwidth].tileset = tiles;
    int newtiles = tiles;

    clamp_tilecol(levx, levy, false);

    switch_tileset_tiles(oldtiles, newtiles);
    notedelay = 45;
    ed.note = "Now using "+tilesets[tiles]+" Tileset";
    updatetiles = true;
}

// Gracefully switches to and from Tower Tileset if autotilig is on
void editorclass::switch_tileset_tiles(int from, int to) {
    // Do nothing in Direct Mode
    if (level[levx + levy*maxwidth].directmode)
        return;

    // Otherwise, set tiles naively to one of the correct type.
    // Autotiling will fix them automatically later.
    int tile;
    enum tiletyp typ;
    int newfg = 80;
    int newbg = 680;
    int newspike = 6;

    if (to == 5) {
        newfg = 12;
        newbg = 28;
    }

    for (int x = 0; x < 40; x++) {
        for (int y = 0; y < 30; y++) {
            tile = gettilelocal(x, y);
            typ = gettiletyp(from, tile);
            if (typ == TILE_FOREGROUND)
                settilelocal(x, y, newfg);
            else if (typ == TILE_BACKGROUND)
                settilelocal(x, y, newbg);
            else if (typ == TILE_SPIKE)
                settilelocal(x, y, newspike);
        }
    }
}

// Switches tileset color
void editorclass::switch_tilecol(bool reversed) {
    if (reversed)
        level[levx + levy*maxwidth].tilecol--;
    else
        level[levx + levy*maxwidth].tilecol++;

    clamp_tilecol(levx, levy, true);

    notedelay = 45;
    ed.note = "Tileset Colour Changed";
    updatetiles = true;
}

void editorclass::clamp_tilecol(int levx, int levy, bool wrap) {
    int tileset = level[levx + levy*maxwidth].tileset;
    int tilecol = level[levx + levy*maxwidth].tilecol;

    int mincol = -1;
    int maxcol = 5;

    // Only Space Station allows tileset -1
    if (tileset != 0)
        mincol = 0;

    if (tileset == 0)
        maxcol = 31;
    else if (tileset == 1)
        maxcol = 7;
    else if (tileset == 3)
        maxcol = 6;
    else if (tileset == 5)
        maxcol = 29;

    // If wrap is true, wrap-around, otherwise just cap
    if (tilecol > maxcol)
        tilecol = (wrap ? mincol : maxcol);
    if (tilecol < mincol)
        tilecol = (wrap ? maxcol : mincol);
    level[levx + levy*maxwidth].tilecol = tilecol;
}

// Performs tasks needed when enabling Tower Mode
void editorclass::enable_tower(void) {
    int room = levx + levy*maxwidth;

    // Set Tower Tileset and color 0
    switch_tileset_tiles(level[room].tileset, 5);
    level[room].tileset = 5;
    level[room].tilecol = 0;

    /* Place the player at the level's tower destination.
       Defaults to zero, but might be something else if we've
       had tower mode enabled in this room previously. */
    ypos = level[room].tower_row;

    // If we have an adjacant tower room, reuse its tower
    int rx = levx;
    int ry = levy;
    int tower = 0;
    if (get_tower(rx, ry - 1))
        tower = get_tower(rx, ry - 1);
    else if (get_tower(rx, ry + 1))
        tower = get_tower(rx, ry + 1);

    if (!tower) {
        // Find an unused tower ID
        int i;
        bool unused = false;
        for (i = 1; i <= maxwidth * maxheight; i++) {
            unused = true;

            for (rx = 0; rx < maxwidth && unused; rx++)
                for (ry = 0; ry < maxheight && unused; ry++)
                    if (get_tower(rx, ry) == i)
                        unused = false;

            if (unused)
                break;
        }

        tower = i;
    }

    level[room].tower = tower;
    snap_tower_entry(levx, levy);
}

// Move tower entry and editor position within tower boundaries
void editorclass::snap_tower_entry(int rx, int ry) {
    int tower = get_tower(rx, ry);
    int size = tower_size(tower);

    // Snap editor position to the whole tower bottom or top with a 10 offset
    if (ypos > size - 20)
        ypos = size - 20;

    if (ypos < -10)
        ypos = -10;

    // Snap entry row to the bottom row.
    // Useful to avoid using the room as exit point.
    if (level[rx + ry*maxwidth].tower_row >= size)
        level[rx + ry*maxwidth].tower_row = size - 1;
}

// Enlarge a tower, downwards to y or shifting down if y is negative
void editorclass::upsize_tower(int tower, int y)
{
    if (!y || !tower)
        return;

    // Check if we actually need to upsize it
    if (y > 0 && towers[tower-1].size > y)
        return;

    if (y > 0) {
        towers[tower-1].size = y + 1;
        resize_tower_tiles(tower);
        return;
    }

    towers[tower-1].size = towers[tower-1].size - y;
    resize_tower_tiles(tower);
    shift_tower(tower, -y);
}

// Remove vertical edges lacking tiles (down to a minimum of 40)
void editorclass::downsize_tower(int tower) {
    if (!tower)
        return;

    int ty, by, size;
    size = tower_size(tower);

    // Check unused topmost edges
    for (ty = 0; ty < size * 40; ty++)
        if (towers[tower-1].tiles[ty] != 0)
            break;
    ty /= 40;

    // Don't resize below 40
    if (ty > (size - 40))
        ty = size - 40;
    if (ty > 0) {
        shift_tower(tower, -ty);
        towers[tower-1].size -= ty;
        resize_tower_tiles(tower);
    }

    // Check unused bottom edges
    size = tower_size(tower);
    for (by = size * 40 - 1; by; by--)
        if (towers[tower-1].tiles[by] != 0)
            break;
    by = size * 40 - 1 - by;
    by /= 40;

    if (by > (size - 40))
        by = size - 40;
    if (by > 0) {
        towers[tower-1].size -= by;
        resize_tower_tiles(tower);
    }
}

// Resizes the tower tile size. If enlarged, zerofill the new tiles
void editorclass::resize_tower_tiles(int tower) {
    if (!tower)
        return;

    int oldsize = towers[tower-1].tiles.size() / 40 + 1;
    int newsize = tower_size(tower);
    towers[tower-1].tiles.resize(40 * newsize);

    // Zerofill new rows
    for (; oldsize < newsize; oldsize++)
        for (int x = 0; x < 40; x++)
            towers[tower-1].tiles[x + oldsize*40] = 0;
}

// Shift tower downwards (positive y) or upwards (negative y).
// Also shifts tower entry position
void editorclass::shift_tower(int tower, int y) {
    if (!tower || !y)
        return;

    int x, ny, size;
    size = tower_size(tower);

    // Shift entry points
    for (int rx = 0; rx < maxwidth; rx++) {
        for (int ry = 0; ry < maxheight; ry++) {
            if (tower == get_tower(rx, ry)) {
                level[rx + ry*maxwidth].tower_row += y;
                if (level[rx + ry*maxwidth].tower_row < 0)
                    level[rx + ry*maxwidth].tower_row = 0;
                if (level[rx + ry*maxwidth].tower_row >= size)
                    level[rx + ry*maxwidth].tower_row = size - 1;
            }
        }
    }

    // Shift entities
    for (size_t i = 0; i < edentity.size(); i++) {
        if (tower == edentity[i].intower)
            edentity[i].y += y;
        if (edentity[i].t == 13 && tower == edentity[i].p3)
            edentity[i].p2 += y;
    }

    // Shift editor scroll position
    ypos += y;

    // Shift tower downwards
    if (y > 0) {
        for (ny = size - 1; ny >= 0; ny--) {
            for (x = 0; x < 40; x++) {
                if (ny >= y)
                    towers[tower-1].tiles[x + ny*40] =
                        towers[tower-1].tiles[x + (ny - y)*40];
                else
                    towers[tower-1].tiles[x + ny*40] = 0;
            }
        }

        return;
    }

    // Shift tower upwards
    for (ny = 0; ny < size + y; ny++)
        for (x = 0; x < 40; x++)
            towers[tower-1].tiles[x + ny*40] =
                towers[tower-1].tiles[x + (ny - y)*40];
}

// Returns the tower of the given room
int editorclass::get_tower(int rx, int ry) {
    int room = rx + ry * maxwidth;
    if (ry < 0 || rx < 0 || rx >= maxwidth || ry >= maxheight)
        return 0;

    return level[room].tower;
}

// Finds the tower in the level and sets rx/ry to it
bool editorclass::find_tower(int tower, int &rx, int &ry) {
    for (rx = 0; rx < maxwidth; rx++)
        for (ry = 0; ry < maxheight; ry++)
            if (tower == get_tower(rx, ry))
                return true;
    return false;
}

int editorclass::tower_size(int tower) {
    if (!tower)
        return 0;

    return towers[tower-1].size;
}
int editorclass::tower_scroll(int tower) {
    return towers[tower-1].scroll;
}

bool editorclass::intower(void) {
    return !!get_tower(levx, levy);
}

int editorclass::tower_row(int rx, int ry) {
    int tower = get_tower(rx, ry);
    if (!tower)
        return -1;

    int room = rx + ry * maxwidth;
    return level[room].tower_row;
}

bool editorclass::load(std::string& _path)
{
    reset();
    map.teleporters.clear();
    ed.customtrials.clear();
    dimensions.clear();

    static const char *levelDir = "levels/";
    if (_path.compare(0, strlen(levelDir), levelDir) != 0)
    {
        _path = levelDir + _path;
    }

    char** path = PHYSFS_getSearchPath();
    char** i = path;
    int len = 0;
    while (*i != nullptr) {
        i++;
        len++;
    }

    //printf("Unmounting %s\n", graphics.assetdir.c_str());
    //PHYSFS_unmount(graphics.assetdir.c_str());
    //graphics.assetdir = "";
    //graphics.reloadresources();

    FILESYSTEM_unmountassets();
    if (game.playassets != "")
    {
        FILESYSTEM_mountassets(game.playassets.c_str());
    }
    else
    {
        FILESYSTEM_mountassets(_path.c_str());
    }

    tinyxml2::XMLDocument doc;
    if (!FILESYSTEM_loadTiXml2Document(_path.c_str(), doc))
    {
        printf("No level %s to load :(\n", _path.c_str());
        return false;
    }


    tinyxml2::XMLHandle hDoc(&doc);
    tinyxml2::XMLElement* pElem;
    tinyxml2::XMLHandle hRoot(NULL);
    version = 0;

    {
        pElem=hDoc.FirstChildElement().ToElement();
        // should always have a valid root but handle gracefully if it does
        if (!pElem)
        {
            printf("No valid root! Corrupt level file?\n");
        }

        pElem->QueryIntAttribute("version", &version);
        if (pElem->Attribute("vceversion"))
            pElem->QueryIntAttribute("vceversion", &vceversion);
        else
            vceversion = 0;
        // save this for later
        hRoot=tinyxml2::XMLHandle(pElem);
    }

    for( pElem = hRoot.FirstChildElement( "Data" ).FirstChild().ToElement(); pElem; pElem=pElem->NextSiblingElement())
    {
        std::string pKey(pElem->Value());
        const char* pText = pElem->GetText() ;
        if(pText == NULL)
        {
            pText = "";
        }

        if (pKey == "MetaData")
        {

            for( tinyxml2::XMLElement* subElem = pElem->FirstChildElement(); subElem; subElem= subElem->NextSiblingElement())
            {
                std::string pKey(subElem->Value());
                const char* pText = subElem->GetText() ;
                if(pText == NULL)
                {
                    pText = "";
                }

                if(pKey == "Creator")
                {
                    EditorData::GetInstance().creator = pText;
                }

                if(pKey == "Title")
                {
                    EditorData::GetInstance().title = pText;
                }

                if(pKey == "Desc1")
                {
                    Desc1 = pText;
                }

                if(pKey == "Desc2")
                {
                    Desc2 = pText;
                }

                if(pKey == "Desc3")
                {
                    Desc3 = pText;
                }

                if(pKey == "website")
                {
                    website = pText;
                }
            }
        }

        if (pKey == "mapwidth")
        {
            mapwidth = atoi(pText);
        }
        if (pKey == "mapheight")
        {
            mapheight = atoi(pText);
        }
        if (pKey == "levmusic")
        {
            levmusic = atoi(pText);
        }

        if (pKey == "dimensions")
        {
            for (tinyxml2::XMLElement* dimensionEl = pElem->FirstChildElement(); dimensionEl; dimensionEl = dimensionEl->NextSiblingElement()) {
                Dimension dim;
                dimensionEl->QueryIntAttribute("x", &dim.x);
                dimensionEl->QueryIntAttribute("y", &dim.y);
                dimensionEl->QueryIntAttribute("w", &dim.w);
                dimensionEl->QueryIntAttribute("h", &dim.h);

                if (dim.w <= 0 || dim.h <= 0)
                    continue;

                const char* pText = dimensionEl->GetText();
                if (pText == NULL)
                    pText = "";
                std::string TextString = pText;
                if (TextString.length())
                    dim.name = TextString;

                dimensions.push_back(dim);
            }
        }

        if (pKey == "timetrials")
        {
            for( tinyxml2::XMLElement* trialEl = pElem->FirstChildElement(); trialEl; trialEl=trialEl->NextSiblingElement())
            {
                customtrial temp;
                trialEl->QueryIntAttribute("roomx",    &temp.roomx    );
                trialEl->QueryIntAttribute("roomy",    &temp.roomy    );
                trialEl->QueryIntAttribute("startx",   &temp.startx   );
                trialEl->QueryIntAttribute("starty",   &temp.starty   );
                trialEl->QueryIntAttribute("startf",   &temp.startf   );
                trialEl->QueryIntAttribute("par",      &temp.par      );
                trialEl->QueryIntAttribute("trinkets", &temp.trinkets );
                trialEl->QueryIntAttribute("music",    &temp.music );
                if(trialEl->GetText() != NULL)
                {
                    temp.name = std::string(trialEl->GetText()) ;
                } else {
                    temp.name = "???";
                }
                ed.customtrials.push_back(temp);

            }

        }


        if (pKey == "contents")
        {
            std::string TextString = (pText);
            if(TextString.length())
            {
                std::vector<std::string> values = split(TextString,',');
                //contents.clear();
                for(size_t i = 0; i < contents.size(); i++)
                {
                    contents[i] =0;
                }
                int x =0;
                int y =0;
                for(size_t i = 0; i < values.size(); i++)
                {
                    contents[x + (maxwidth*40*y)] = atoi(values[i].c_str());
                    x++;
                    if(x == mapwidth*40)
                    {
                        x=0;
                        y++;
                    }

                }
            }
        }

        if (pKey == "altstates") {
            int i = 0;
            for (tinyxml2::XMLElement* edAltstateEl = pElem->FirstChildElement(); edAltstateEl; edAltstateEl = edAltstateEl->NextSiblingElement()) {
                std::string pKey(edAltstateEl->Value());
                const char* pText = edAltstateEl->GetText();

                if (pText == NULL)
                    pText = "";

                std::string TextString = pText;

                if (TextString.length()) {
                    edAltstateEl->QueryIntAttribute("x", &altstates[i].x);
                    edAltstateEl->QueryIntAttribute("y", &altstates[i].y);
                    edAltstateEl->QueryIntAttribute("state", &altstates[i].state);

                    std::vector<std::string> values = split(TextString, ',');

                    for (size_t t = 0; t < values.size(); t++)
                        altstates[i].tiles[t] = atoi(values[t].c_str());

                    i++;
                }
            }
        }

        if (pKey == "towers") {
            int i = 0;
            for (tinyxml2::XMLElement *edTowerEl = pElem->FirstChildElement();
                 edTowerEl; edTowerEl = edTowerEl->NextSiblingElement()) {
                std::string pKey(edTowerEl->Value());
                const char* pText = edTowerEl->GetText();

                if (pText == NULL)
                    pText = "";

                std::string TextString = pText;

                if (TextString.length()) {
                    edTowerEl->QueryIntAttribute("size", &towers[i].size);
                    edTowerEl->QueryIntAttribute("scroll", &towers[i].scroll);

                    std::vector<std::string> values = split(TextString, ',');

                    for (size_t t = 0; t < values.size(); t++)
                        towers[i].tiles[t] = atoi(values[t].c_str());

                    i++;
                }
            }
        }

        /*else if(version==1){
          if (pKey == "contents")
          {
            std::string TextString = (pText);
            if(TextString.length())
            {
              std::vector<std::string> values = split(TextString,',');
              contents.clear();
              for(int i = 0; i < values.size(); i++)
              {
                contents.push_back(atoi(values[i].c_str()));
              }
            }
          }
        //}
        */

        if (pKey == "teleporters")
        {
            for( tinyxml2::XMLElement* teleporterEl = pElem->FirstChildElement(); teleporterEl; teleporterEl=teleporterEl->NextSiblingElement())
            {
                point temp;
                teleporterEl->QueryIntAttribute("x", &temp.x);
                teleporterEl->QueryIntAttribute("y", &temp.y);

                map.setteleporter(temp.x,temp.y);

            }

        }

        if (pKey == "edEntities")
        {
            for( tinyxml2::XMLElement* edEntityEl = pElem->FirstChildElement(); edEntityEl; edEntityEl=edEntityEl->NextSiblingElement())
            {
                edentities entity;

                std::string pKey(edEntityEl->Value());
                if (edEntityEl->GetText() != NULL)
                {
                    std::string text(edEntityEl->GetText());

                    // And now we come to the part where we have to deal with
                    // the terrible decisions of the past.
                    //
                    // For some reason, the closing tag of edentities generated
                    // by 2.2 and below has not only been put on a separate
                    // line, but also indented to match with the opening tag as
                    // well. Like this:
                    //
                    //    <edentity ...>contents
                    //    </edentity>
                    //
                    // Instead of doing <edentity ...>contents</edentity>.
                    //
                    // This is COMPLETELY terrible. This requires the XML to be
                    // parsed in an extremely specific and quirky way, which
                    // TinyXML-1 just happened to do.
                    //
                    // TinyXML-2 by default interprets the newline and the next
                    // indentation of whitespace literally, so you end up with
                    // tag contents that has a linefeed plus a bunch of extra
                    // spaces. You can't fix this by setting the whitespace
                    // mode to COLLAPSE_WHITESPACE, that does way more than
                    // TinyXML-1 ever did - it removes the leading whitespace
                    // from things like <edentity ...> this</edentity>, and
                    // collapses XML-encoded whitespace like <edentity ...>
                    // &#32; &#32;this</edentity>, which TinyXML-1 never did.
                    //
                    // Best solution here is to specifically hardcode removing
                    // the linefeed + the extremely specific amount of
                    // whitespace at the end of the contents.

                    if (endsWith(text, "\n            ")) // linefeed + exactly 12 spaces
                    {
                        // 12 spaces + 1 linefeed = 13 chars
                        text = text.substr(0, text.length()-13);
                    }

                    entity.scriptname = text;
                }

                if (edEntityEl->Attribute("activityname")) {
                    entity.activityname = edEntityEl->Attribute("activityname");
                } else {
                    entity.activityname = "";
                }

                if (edEntityEl->Attribute("activitycolor")) {
                    entity.activitycolor = edEntityEl->Attribute("activitycolor");
                } else {
                    entity.activitycolor = "";
                }

                edEntityEl->QueryIntAttribute("x", &entity.x);
                edEntityEl->QueryIntAttribute("y", &entity.y);
                edEntityEl->QueryIntAttribute("subx", &entity.subx);
                edEntityEl->QueryIntAttribute("suby", &entity.suby);
                edEntityEl->QueryIntAttribute("t", &entity.t);

                edEntityEl->QueryIntAttribute("p1", &entity.p1);
                edEntityEl->QueryIntAttribute("p2", &entity.p2);
                edEntityEl->QueryIntAttribute("p3", &entity.p3);
                edEntityEl->QueryIntAttribute("p4", &entity.p4);
                edEntityEl->QueryIntAttribute("p5", &entity.p5);
                edEntityEl->QueryIntAttribute("p6", &entity.p6);

                edEntityEl->QueryIntAttribute("state", &entity.state);
                edEntityEl->QueryIntAttribute("intower", &entity.intower);

                edEntityEl->QueryIntAttribute("onetime", (int*) &entity.onetime);

                edentity.push_back(entity);
            }
        }

        if (pKey == "levelMetaData")
        {
            int i = 0;
            int rowwidth = 0;
            int maxrowwidth = std::max(mapwidth, 20);
            for( tinyxml2::XMLElement* edLevelClassElement = pElem->FirstChildElement(); edLevelClassElement; edLevelClassElement=edLevelClassElement->NextSiblingElement())
            {
                std::string pKey(edLevelClassElement->Value());
                if(edLevelClassElement->GetText() != NULL)
                {
                    level[i].roomname = std::string(edLevelClassElement->GetText()) ;
                }

                edLevelClassElement->QueryIntAttribute("tileset", &level[i].tileset);
                edLevelClassElement->QueryIntAttribute("tilecol", &level[i].tilecol);
                edLevelClassElement->QueryIntAttribute("customtileset", &level[i].customtileset);
                edLevelClassElement->QueryIntAttribute("customspritesheet", &level[i].customspritesheet);
                edLevelClassElement->QueryIntAttribute("platx1", &level[i].platx1);
                edLevelClassElement->QueryIntAttribute("platy1", &level[i].platy1);
                edLevelClassElement->QueryIntAttribute("platx2", &level[i].platx2);
                edLevelClassElement->QueryIntAttribute("platy2", &level[i].platy2);
                edLevelClassElement->QueryIntAttribute("platv", &level[i].platv);
                if (edLevelClassElement->Attribute("enemyv")) {
                    edLevelClassElement->QueryIntAttribute("enemyv", &level[i].enemyv);
                } else {
                    level[i].enemyv = 4;
                }
                edLevelClassElement->QueryIntAttribute("enemyx1", &level[i].enemyx1);
                edLevelClassElement->QueryIntAttribute("enemyy1", &level[i].enemyy1);
                edLevelClassElement->QueryIntAttribute("enemyx2", &level[i].enemyx2);
                edLevelClassElement->QueryIntAttribute("enemyy2", &level[i].enemyy2);
                edLevelClassElement->QueryIntAttribute("enemytype", &level[i].enemytype);
                edLevelClassElement->QueryIntAttribute("directmode", &level[i].directmode);
                edLevelClassElement->QueryIntAttribute("tower", &level[i].tower);
                edLevelClassElement->QueryIntAttribute("tower_row", &level[i].tower_row);
                edLevelClassElement->QueryIntAttribute("warpdir", &level[i].warpdir);

                i++;

                rowwidth++;
                if (rowwidth == maxrowwidth) {
                    rowwidth = 0;
                    i += maxwidth - maxrowwidth;
                }
            }
        }

        if (pKey == "script")
        {
            std::string TextString = (pText);
            if(TextString.length())
            {
                std::vector<std::string> values = split(TextString,'|');
                script.clearcustom();
                Script script_ = {};
                bool headerfound = false;
                for (auto& line : values)
                {
                    if (endsWith(line, ":")) {
                        if (headerfound)
                            // Add the script if we have a preceding header
                            script.customscripts.push_back(script_);

                        script_.name = line.substr(0, line.length()-1);
                        script_.contents.clear();
                        headerfound = true;
                        continue;
                    }

                    if (headerfound)
                        script_.contents.push_back(line);
                }
                // Add the last script
                if (headerfound)
                    // Add the script if we have a preceding header
                    script.customscripts.push_back(script_);

            }
        }

    }

    gethooks();
    version=2;

    return true;
}

bool editorclass::save(std::string& _path)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* msg;
    tinyxml2::XMLDeclaration* decl = doc.NewDeclaration();
    doc.LinkEndChild( decl );

    tinyxml2::XMLElement * root = doc.NewElement( "MapData" );
    root->SetAttribute("version",version);
    root->SetAttribute("vceversion",VCEVERSION);
    doc.LinkEndChild( root );

    tinyxml2::XMLComment * comment = doc.NewComment(" Save file " );
    root->LinkEndChild( comment );

    tinyxml2::XMLElement * data = doc.NewElement( "Data" );
    root->LinkEndChild( data );

    msg = doc.NewElement( "MetaData" );

    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    std::string timeAndDate = asctime (timeinfo);

    EditorData::GetInstance().timeModified =  timeAndDate;
    if(EditorData::GetInstance().timeModified == "")
    {
        EditorData::GetInstance().timeCreated =  timeAndDate;
    }

    //getUser
    tinyxml2::XMLElement* meta = doc.NewElement( "Creator" );
    meta->LinkEndChild( doc.NewText( EditorData::GetInstance().creator.c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Title" );
    meta->LinkEndChild( doc.NewText( EditorData::GetInstance().title.c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Created" );
    meta->LinkEndChild( doc.NewText( help.String(version).c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Modified" );
    meta->LinkEndChild( doc.NewText( EditorData::GetInstance().modifier.c_str() ) );
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Modifiers" );
    meta->LinkEndChild( doc.NewText( help.String(version).c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Desc1" );
    meta->LinkEndChild( doc.NewText( Desc1.c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Desc2" );
    meta->LinkEndChild( doc.NewText( Desc2.c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "Desc3" );
    meta->LinkEndChild( doc.NewText( Desc3.c_str() ));
    msg->LinkEndChild( meta );

    meta = doc.NewElement( "website" );
    meta->LinkEndChild( doc.NewText( website.c_str() ));
    msg->LinkEndChild( meta );

    data->LinkEndChild( msg );

    msg = doc.NewElement( "mapwidth" );
    msg->LinkEndChild( doc.NewText( help.String(mapwidth).c_str() ));
    data->LinkEndChild( msg );

    msg = doc.NewElement( "mapheight" );
    msg->LinkEndChild( doc.NewText( help.String(mapheight).c_str() ));
    data->LinkEndChild( msg );

    msg = doc.NewElement( "levmusic" );
    msg->LinkEndChild( doc.NewText( help.String(levmusic).c_str() ));
    data->LinkEndChild( msg );

    //New save format
    std::string contentsString="";
    for(int y = 0; y < mapheight*30; y++ )
    {
        for(int x = 0; x < mapwidth*40; x++ )
        {
            contentsString += help.String(contents[x + (maxwidth*40*y)]) + ",";
        }
    }
    msg = doc.NewElement( "contents" );
    msg->LinkEndChild( doc.NewText( contentsString.c_str() ));
    data->LinkEndChild( msg );

    msg = doc.NewElement("altstates");

    // Iterate through all the altstates. Nonexistent altstates are ones at -1,-1
    tinyxml2::XMLElement* alt;
    for (size_t a = 0; a < altstates.size(); a++) {
        if (altstates[a].x == -1 || altstates[a].y == -1)
            continue;

        std::string tiles = "";
        for (int y = 0; y < 30; y++)
            for (int x = 0; x < 40; x++)
                tiles += help.String(altstates[a].tiles[x + y*40]) + ",";

        alt = doc.NewElement("altstate");
        alt->SetAttribute("x", altstates[a].x);
        alt->SetAttribute("y", altstates[a].y);
        alt->SetAttribute("state", altstates[a].state);
        alt->LinkEndChild(doc.NewText(tiles.c_str()));
        msg->LinkEndChild(alt);
    }
    data->LinkEndChild(msg);

    msg = doc.NewElement("towers");

    // Figure out amount of towers used
    int twx, twy;
    int max_tower = 0;
    for (twx = 0; twx < maxwidth; twx++)
        for (twy = 0; twy < maxheight; twy++)
            if (max_tower < get_tower(twx, twy))
                max_tower = get_tower(twx, twy);

    tinyxml2::XMLElement* tw;
    for (int t = 0; t < max_tower; t++) {
        // Don't save unused towers
        bool found = false;
        for (twx = 0; twx < maxwidth && !found; twx++)
            for (twy = 0; twy < maxheight && !found; twy++)
                if ((t + 1) == get_tower(twx, twy))
                    found = true;

        if (!found) {
            for (int u = (t + 1); u < max_tower; u++) {
                towers[u - 1].size = towers[u].size;
                towers[u - 1].scroll = towers[u].scroll;
                towers[u - 1].tiles.resize(40 * towers[u - 1].size);
                for (int i = 0; i < 40 * towers[u - 1].size; i++)
                    towers[u - 1].tiles[i] = towers[u].tiles[i];
            }

            // Shift tower ID in rooms
            for (twx = 0; twx < maxwidth; twx++)
                for (twy = 0; twy < maxheight; twy++)
                    if (level[twx + twy * maxwidth].tower > t)
                        level[twx + twy * maxwidth].tower--;

            // Shift tower ID in entities
            for (size_t i = 0; i < edentity.size(); i++) {
                if (edentity[i].intower == t ||
                    (edentity[i].t == 13 && edentity[i].p3 == t)) {
                    removeedentity(i);
                    i--;
                    continue;
                }

                if (edentity[i].intower > t)
                    edentity[i].intower--;
                if (edentity[i].t == 13 && edentity[i].p3 > t)
                    edentity[i].p3--;
            }

            t--;
            max_tower--;
            continue;
        }

        std::string tiles = "";
        for (int y = 0; y < towers[t].size; y++)
            for (int x = 0; x < 40; x++)
                tiles += help.String(towers[t].tiles[x + y*40]) + ",";

        tw = doc.NewElement("tower");
        tw->SetAttribute("size", towers[t].size);
        tw->SetAttribute("scroll", towers[t].scroll);
        tw->LinkEndChild(doc.NewText(tiles.c_str()));
        msg->LinkEndChild(tw);
    }
    data->LinkEndChild(msg);

    msg = doc.NewElement( "teleporters" );
    for(size_t i = 0; i < map.teleporters.size(); i++)
    {
        tinyxml2::XMLElement *teleporterElement = doc.NewElement( "teleporter" );
        teleporterElement->SetAttribute( "x", map.teleporters[i].x);
        teleporterElement->SetAttribute( "y", map.teleporters[i].y);
        msg->LinkEndChild( teleporterElement );
    }

    data->LinkEndChild( msg );

    msg = doc.NewElement( "timetrials" );
    for(int i = 0; i < (int)ed.customtrials.size(); i++) {
        tinyxml2::XMLElement *trialElement = doc.NewElement( "trial" );
        trialElement->SetAttribute( "roomx",    ed.customtrials[i].roomx   );
        trialElement->SetAttribute( "roomy",    ed.customtrials[i].roomy   );
        trialElement->SetAttribute( "startx",   ed.customtrials[i].startx  );
        trialElement->SetAttribute( "starty",   ed.customtrials[i].starty  );
        trialElement->SetAttribute( "startf",   ed.customtrials[i].startf  );
        trialElement->SetAttribute( "par",      ed.customtrials[i].par     );
        trialElement->SetAttribute( "trinkets", ed.customtrials[i].trinkets);
        trialElement->SetAttribute( "music",    ed.customtrials[i].music   );
        trialElement->LinkEndChild( doc.NewText( ed.customtrials[i].name.c_str() )) ;
        msg->LinkEndChild( trialElement );
    }

    data->LinkEndChild( msg );

    msg = doc.NewElement("dimensions");
    for (size_t i = 0; i < dimensions.size(); i++) {
        Dimension* dim = &dimensions[i];

        tinyxml2::XMLElement* dimensionEl = doc.NewElement("dimension");
        dimensionEl->SetAttribute("x", dim->x);
        dimensionEl->SetAttribute("y", dim->y);
        dimensionEl->SetAttribute("w", dim->w);
        dimensionEl->SetAttribute("h", dim->h);
        dimensionEl->LinkEndChild(doc.NewText(dim->name.c_str()));
        msg->LinkEndChild(dimensionEl);
    }
    data->LinkEndChild(msg);

    msg = doc.NewElement( "edEntities" );
    for(size_t i = 0; i < edentity.size(); i++)
    {
        tinyxml2::XMLElement *edentityElement = doc.NewElement( "edentity" );
        edentityElement->SetAttribute( "x", edentity[i].x);
        edentityElement->SetAttribute(  "y", edentity[i].y);
        edentityElement->SetAttribute( "subx", edentity[i].subx);
        edentityElement->SetAttribute(  "suby", edentity[i].suby);
        edentityElement->SetAttribute(  "t", edentity[i].t);
        edentityElement->SetAttribute(  "p1", edentity[i].p1);
        edentityElement->SetAttribute(  "p2", edentity[i].p2);
        edentityElement->SetAttribute(  "p3", edentity[i].p3);
        edentityElement->SetAttribute( "p4", edentity[i].p4);
        edentityElement->SetAttribute( "p5", edentity[i].p5);
        edentityElement->SetAttribute(  "p6", edentity[i].p6);
        if (edentity[i].state != 0)
                edentityElement->SetAttribute("state", edentity[i].state);
        edentityElement->SetAttribute("intower", edentity[i].intower);
        if (edentity[i].activityname != "") {
            edentityElement->SetAttribute(  "activityname", edentity[i].activityname.c_str());
        }
        if (edentity[i].activitycolor != "") {
            edentityElement->SetAttribute(  "activitycolor", edentity[i].activitycolor.c_str());
        }
        if (edentity[i].onetime)
            edentityElement->SetAttribute("onetime", help.String((int) edentity[i].onetime).c_str());
        edentityElement->LinkEndChild( doc.NewText( edentity[i].scriptname.c_str() )) ;
        msg->LinkEndChild( edentityElement );
    }

    data->LinkEndChild( msg );

    msg = doc.NewElement( "levelMetaData" );
    int rowwidth = 0;
    int maxrowwidth = std::max(mapwidth, 20);
    int rows = 0;
    int maxrows = mapwidth <= 20 && mapheight <= 20 ? 20 : mapheight;
    for (int i = 0; i < maxwidth * maxheight; i++) {
        tinyxml2::XMLElement *edlevelclassElement = doc.NewElement( "edLevelClass" );
        edlevelclassElement->SetAttribute( "tileset", level[i].tileset);
        edlevelclassElement->SetAttribute(  "tilecol", level[i].tilecol);
        edlevelclassElement->SetAttribute(  "customtileset", level[i].customtileset);
        edlevelclassElement->SetAttribute(  "customspritesheet", level[i].customspritesheet);
        edlevelclassElement->SetAttribute(  "platx1", level[i].platx1);
        edlevelclassElement->SetAttribute(  "platy1", level[i].platy1);
        edlevelclassElement->SetAttribute(  "platx2", level[i].platx2);
        edlevelclassElement->SetAttribute( "platy2", level[i].platy2);
        edlevelclassElement->SetAttribute( "platv", level[i].platv);
        edlevelclassElement->SetAttribute( "enemyv", level[i].enemyv);
        edlevelclassElement->SetAttribute(  "enemyx1", level[i].enemyx1);
        edlevelclassElement->SetAttribute(  "enemyy1", level[i].enemyy1);
        edlevelclassElement->SetAttribute(  "enemyx2", level[i].enemyx2);
        edlevelclassElement->SetAttribute(  "enemyy2", level[i].enemyy2);
        edlevelclassElement->SetAttribute(  "enemytype", level[i].enemytype);
        edlevelclassElement->SetAttribute(  "directmode", level[i].directmode);
        edlevelclassElement->SetAttribute(  "tower", level[i].tower);
        edlevelclassElement->SetAttribute(  "tower_row", level[i].tower_row);
        edlevelclassElement->SetAttribute(  "warpdir", level[i].warpdir);

        edlevelclassElement->LinkEndChild( doc.NewText( level[i].roomname.c_str() )) ;
        msg->LinkEndChild( edlevelclassElement );

        rowwidth++;
        if (rowwidth == maxrowwidth) {
            rowwidth = 0;
            i += maxwidth - maxrowwidth;
            rows++;
            if (rows == maxrows)
                break;
        }
    }
    data->LinkEndChild( msg );

    std::string scriptString;
    for(auto& script_ : script.customscripts)
    {
        scriptString += script_.name + ":|";
        for (auto& line : script_.contents)
            scriptString += line + "|";
    }
    msg = doc.NewElement( "script" );
    msg->LinkEndChild( doc.NewText( scriptString.c_str() ));
    data->LinkEndChild( msg );

    return FILESYSTEM_saveTiXml2Document(("levels/" + _path).c_str(), doc);
}


void addedentity( int xp, int yp, int tp, int p1/*=0*/, int p2/*=0*/, int p3/*=0*/, int p4/*=0*/, int p5/*=320*/, int p6/*=240*/)
{
    edentities entity;

    int tower = ed.get_tower(ed.levx, ed.levy);
    entity.x=xp;
    entity.y=yp;
    entity.subx=0;
    entity.suby=0;
    entity.t=tp;
    entity.p1=p1;
    entity.p2=p2;
    entity.p3=p3;
    entity.p4=p4;
    entity.p5=p5;
    entity.p6=p6;
    entity.state=ed.levaltstate;
    entity.intower=tower;
    entity.scriptname="";
    entity.activityname="";
    entity.activitycolor="";
    entity.onetime = false;

    edentity.push_back(entity);
}

void removeedentity( int t )
{
    edentity.erase(edentity.begin() + t);
}

int edentat(int x, int y, int state, int tower) {
    for(size_t i=0; i<edentity.size(); i++)
        if (edentity[i].x==x && edentity[i].y==y &&
            edentity[i].state==state && edentity[i].intower==tower)
            return i;
    return -1;
}

bool edentclear(int x, int y, int state, int tower) {
    if (edentat(x, y, state, tower) >= 0)
        return false;
    return true;
}

void fillbox( int x, int y, int x2, int y2, int c )
{
    FillRect(graphics.backBuffer, x, y, x2-x, 1, c);
    FillRect(graphics.backBuffer, x, y2-1, x2-x, 1, c);
    FillRect(graphics.backBuffer, x, y, 1, y2-y, c);
    FillRect(graphics.backBuffer, x2-1, y, 1, y2-y, c);
}

void fillboxabs( int x, int y, int x2, int y2, int c )
{
    FillRect(graphics.backBuffer, x, y, x2, 1, c);
    FillRect(graphics.backBuffer, x, y+y2-1, x2, 1, c);
    FillRect(graphics.backBuffer, x, y, 1, y2, c);
    FillRect(graphics.backBuffer, x+x2-1, y, 1, y2, c);
}


void editorclass::generatecustomminimap()
{
    map.customwidth=mapwidth;
    map.customheight=mapheight;
    map.custommmstartx = 0;
    map.custommmstarty = 0;
    if (map.custommode && map.dimension >= 0) {
        Dimension* dim = map.getdimension();
        if (dim != NULL) {
            map.customwidth = dim->w;
            map.customheight = dim->h;
            map.custommmstartx = dim->x;
            map.custommmstarty = dim->y;
        }
    }

    map.customzoom=1;
    if(map.customwidth<=10 && map.customheight<=10) map.customzoom=2;
    if(map.customwidth<=5 && map.customheight<=5) map.customzoom=4;

    //Set minimap offsets
    if(map.customzoom==4)
    {
        map.custommmxoff=24*(5-map.customwidth);
        map.custommmxsize=240-(map.custommmxoff*2);

        map.custommmyoff=18*(5-map.customheight);
        map.custommmysize=180-(map.custommmyoff*2);
    }
    else if(map.customzoom==2)
    {
        map.custommmxoff=12*(10-map.customwidth);
        map.custommmxsize=240-(map.custommmxoff*2);

        map.custommmyoff=9*(10-map.customheight);
        map.custommmysize=180-(map.custommmyoff*2);
    }
    else
    {
        map.custommmxoff=6*(20-map.customwidth);
        map.custommmxsize=240-(map.custommmxoff*2);

        map.custommmyoff=int(4.5*(20-map.customheight));
        map.custommmysize=180-(map.custommmyoff*2);
    }

    if (auto mapimage = graphics.mapimage) {
        SDL_FreeSurface(graphics.images[12]);
        graphics.images[12] = LoadImage(mapimage->c_str());
        return;
    }


    FillRect(graphics.images[12], graphics.getRGB(0,0,0));

    int tm=0;
    int temp=0;
    //Scan over the map size
    if(map.customheight<=5 && map.customwidth<=5)
    {
        //4x map
        for(int j2=0; j2<map.customheight; j2++)
        {
            for(int i2=0; i2<map.customwidth; i2++)
            {
                int i3 = i2 + map.custommmstartx;
                int j3 = j2 + map.custommmstarty;
                //Ok, now scan over each square
                tm=196;
                if(level[i3 + (j3*maxwidth)].tileset==1) tm=96;

                for(int j=0; j<36; j++)
                {
                    for(int i=0; i<48; i++)
                    {
                        temp=absfree(int(i*0.83) + (i3*40),int(j*0.83)+(j3*30));
                        if(temp>=1)
                        {
                            //Fill in this pixel
                            FillRect(graphics.images[12], (i2*48)+i, (j2*36)+j, 1, 1, graphics.getRGB(tm, tm, tm));
                        }
                    }
                }
            }
        }
    }
    else if(map.customheight<=10 && map.customwidth<=10)
    {
        //2x map
        for(int j2=map.custommmstarty; j2<map.customheight+map.custommmstarty; j2++)
        {
            for(int i2=map.custommmstartx; i2<map.customwidth+map.custommmstartx; i2++)
            {
                int i3 = i2 + map.custommmstartx;
                int j3 = j2 + map.custommmstarty;
                //Ok, now scan over each square
                tm=196;
                if(level[i3 + (j3*maxwidth)].tileset==1) tm=96;

                for(int j=0; j<18; j++)
                {
                    for(int i=0; i<24; i++)
                    {
                        temp=absfree(int(i*1.6) + (i3*40),int(j*1.6)+(j3*30));
                        if(temp>=1)
                        {
                            //Fill in this pixel
                            FillRect(graphics.images[12], (i2*24)+i, (j2*18)+j, 1, 1, graphics.getRGB(tm, tm, tm));
                        }
                    }
                }
            }
        }
    }
    else
    {
        for(int j2=map.custommmstarty; j2<map.customheight+map.custommmstarty; j2++)
        {
            for(int i2=map.custommmstartx; i2<map.customwidth+map.custommmstartx; i2++)
            {
                int i3 = i2 + map.custommmstartx;
                int j3 = j2 + map.custommmstarty;
                //Ok, now scan over each square
                tm=196;
                if(level[i3 + (j3*maxwidth)].tileset==1) tm=96;

                for(int j=0; j<9; j++)
                {
                    for(int i=0; i<12; i++)
                    {
                        temp=absfree(3+(i*3) + (i3*40),(j*3)+(j3*30));
                        if(temp>=1)
                        {
                            //Fill in this pixel
                            FillRect(graphics.images[12], (i2*12)+i, (j2*9)+j, 1, 1, graphics.getRGB(tm, tm, tm));
                        }
                    }
                }
            }
        }
    }
}

int
dmcap(void)
{
    if (ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset == 5)
        return 900;
    return 1200;
}

int
dmwidth(void)
{
    if (ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset == 5)
        return 30;
    return 40;
}

int cycle_through_custom_resources(int current, std::map <int, std::vector<SDL_Surface*>>& Map, bool forward)
{
    // The map is empty only the default value is valid
    if(Map.size() == 0)
        return 0;
    // We have selected the default use the first (or last) custom
    if(current == 0)
        return forward ? Map.begin() -> first : (--Map.end())->first;
    // Once we found the current on return the next
    bool returnNext = 0;
    if(forward){
        for(auto it = Map.begin(); it != Map.end(); ++it){
            if(returnNext)
                return it->first;
            if(current == it->first)
              returnNext = true;
        }
    }else{
        for(auto it = Map.end(); it != Map.begin(); ){
            --it;
            if(returnNext)
                return it->first;
            if(current == it->first)
              returnNext = true;
        }
    }
    // We have reached the end because the last one was the current one or that spritesheet
    // no longer exist just go back to the default one
    return 0;
}

#if !defined(NO_EDITOR)
void editormenurender(int tr, int tg, int tb)
{
    switch (game.currentmenuname)
    {
    case Menu::ed_settings:
        if (game.currentmenuoption == 4) {
            if (!game.ghostsenabled)
                graphics.Print(2, 230, "Editor ghost trail is OFF", tr/2, tg/2, tb/2);
            else
                graphics.Print(2, 230, "Editor ghost trail is ON", tr, tg, tb);
        }
        [[fallthrough]];
    case Menu::ed_settings2:
    case Menu::ed_settings3:
        graphics.bigprint( -1, 75, "Map Settings", tr, tg, tb, true);
        break;
    case Menu::ed_dimensions: {
        int colors[6][3] = {
            {255, 0,   0  },
            {0,   255, 0  },
            {0,   0,   255},
            {255, 255, 0  },
            {255, 0,   255},
            {0,   255, 255}
        };

        ed.generatecustomminimap();
        graphics.drawcustompixeltextbox(35+map.custommmxoff, 16+map.custommmyoff, map.custommmxsize+10, map.custommmysize+10, (map.custommmxsize+10)/8, (map.custommmysize+10)/8, tr, tg, tb,4,0);
        graphics.drawpartimage(12, 40+map.custommmxoff, 21+map.custommmyoff, map.custommmxsize,map.custommmysize);
        int xmult = 12;
        int ymult = 9;
        if (map.customzoom == 4) {
            xmult = 48;
            ymult = 36;
        } else if (map.customzoom == 2) {
            xmult = 24;
            ymult = 18;
        }
        int lastcolor = -1;
        for (int i = 0; i < (int)ed.dimensions.size(); i++) {
            // game.mx
            // game.my
            Dimension dim = ed.dimensions[i];
            int color = i % 6;
            lastcolor = color;
            int x_ = 40 + (dim.x * xmult) + map.custommmxoff;
            int y_ = 21 + (dim.y * ymult) + map.custommmyoff;
            int w_ = (dim.w * xmult);
            int h_ = (dim.h * ymult);
            fillboxabs(x_, y_, w_, h_,
                       graphics.getRGB(colors[color][2],colors[color][1],colors[color][0]));
        }
        for (int y = 0; y < map.customheight; y++) {
            for (int x = 0; x < map.customwidth; x++) {
                int x_ = 40 + (x * xmult) + map.custommmxoff;
                int y_ = 21 + (y * ymult) + map.custommmyoff;
                if ((game.mx > x_) && ((game.mx - 1) < (x_ + xmult))) {
                    if ((game.my > y_) && ((game.my - 1) < (y_ + ymult))) {
                        ed.cursor_x = x;
                        ed.cursor_y = y;
                        goto dimensions_break;
                    }
                }
            }
        }
dimensions_break:
        int color = (lastcolor + 1) % 6;
        int display_x = 40 + (ed.cursor_x * xmult) + map.custommmxoff;
        int display_y = 21 + (ed.cursor_y * ymult) + map.custommmyoff;
        fillboxabs(display_x, display_y, xmult, ymult,
            graphics.getRGB(colors[color][2],colors[color][1],colors[color][0]));
        break;
    }
    case Menu::ed_edit_trial: {
        customtrial ctrial = ed.customtrials[ed.edtrial];

        if(ed.trialnamemod)
        {
            if(ed.entframe<2)
            {
                graphics.bigprint( -1, 35, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.bigprint( -1, 35, key.keybuffer+" ", tr, tg, tb, true);
            }
        } else {
            graphics.bigprint( -1, 35, ctrial.name, tr, tg, tb, true);
        }
        game.timetrialpar = ctrial.par;
        graphics.Print( 16, 65,  "MUSIC      " + help.getmusicname(ctrial.music), tr, tg, tb);
        if (ed.trialmod && (game.currentmenuoption == 3))
            graphics.Print( 16, 75,  "TRINKETS   < " + help.number(ctrial.trinkets) + " >", tr, tg, tb);
        else
            graphics.Print( 16, 75,  "TRINKETS   " + help.number(ctrial.trinkets), tr, tg, tb);
        if (ed.trialmod && (game.currentmenuoption == 4))
            graphics.Print( 16, 85,  "TIME       < " + game.partimestring() + " >", tr, tg, tb);
        else
            graphics.Print( 16, 85,  "TIME       " + game.partimestring(), tr, tg, tb);
        break;
    }
    case Menu::ed_remove_trial:
        graphics.bigprint( -1, 35, "Are you sure?", tr, tg, tb, true);
        break;
    case Menu::ed_trials:
        graphics.bigprint( -1, 35, "Time Trials", tr, tg, tb, true);
        for (int i = 0; i < (int)ed.customtrials.size(); i++) {
            std::string sl = ed.customtrials[i].name;
            if (game.currentmenuoption == i) {
                std::transform(sl.begin(), sl.end(), sl.begin(), ::toupper);
                sl = "[ " + sl + " ]";
            } else {
                std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
                sl = "  " + sl + "  ";
            }
            graphics.Print(-1, 75 + (i * 16), sl, tr,tg,tb,true);
        }
        if (game.currentmenuoption == (int)ed.customtrials.size()) {
            graphics.Print(-1, 75 + ((int)ed.customtrials.size() * 16), "[ ADD NEW TRIAL ]", tr,tg,tb,true);
        } else {
            graphics.Print(-1, 75 + ((int)ed.customtrials.size() * 16), "  add new trial  ", tr,tg,tb,true);
        }
        if (game.currentmenuoption == (int)ed.customtrials.size() + 1) {
            graphics.Print(-1, 75 + (((int)ed.customtrials.size() + 1) * 16), "[ BACK TO MENU ]", tr,tg,tb,true);
        } else {
            graphics.Print(-1, 75 + (((int)ed.customtrials.size() + 1) * 16), "  back to menu  ", tr,tg,tb,true);
        }
        break;
    case Menu::ed_desc:
        if(ed.titlemod)
        {
            if(ed.entframe<2)
            {
                graphics.bigprint( -1, 35, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.bigprint( -1, 35, key.keybuffer+" ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.bigprint( -1, 35, EditorData::GetInstance().title, tr, tg, tb, true);
        }
        if(ed.creatormod)
        {
            if(ed.entframe<2)
            {
                graphics.Print( -1, 60, "by " + key.keybuffer+ "_", tr, tg, tb, true);
            }
            else
            {
                graphics.Print( -1, 60, "by " + key.keybuffer+ " ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.Print( -1, 60, "by " + EditorData::GetInstance().creator, tr, tg, tb, true);
        }
        if(ed.websitemod)
        {
            if(ed.entframe<2)
            {
                graphics.Print( -1, 70, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.Print( -1, 70, key.keybuffer+" ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.Print( -1, 70, ed.website, tr, tg, tb, true);
        }
        if(ed.desc1mod)
        {
            if(ed.entframe<2)
            {
                graphics.Print( -1, 90, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.Print( -1, 90, key.keybuffer+" ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.Print( -1, 90, ed.Desc1, tr, tg, tb, true);
        }
        if(ed.desc2mod)
        {
            if(ed.entframe<2)
            {
                graphics.Print( -1, 100, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.Print( -1, 100, key.keybuffer+" ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.Print( -1, 100, ed.Desc2, tr, tg, tb, true);
        }
        if(ed.desc3mod)
        {
            if(ed.entframe<2)
            {
                graphics.Print( -1, 110, key.keybuffer+"_", tr, tg, tb, true);
            }
            else
            {
                graphics.Print( -1, 110, key.keybuffer+" ", tr, tg, tb, true);
            }
        }
        else
        {
            graphics.Print( -1, 110, ed.Desc3, tr, tg, tb, true);
        }
        break;
    case Menu::ed_music:
        graphics.bigprint( -1, 65, "Map Music", tr, tg, tb, true);

        graphics.Print( -1, 85, "Current map music:", tr, tg, tb, true);
        switch(ed.levmusic)
        {
        case 0:
            graphics.Print( -1, 120, "No background music", tr, tg, tb, true);
            break;
        case 1:
            graphics.Print( -1, 120, "1: Pushing Onwards", tr, tg, tb, true);
            break;
        case 2:
            graphics.Print( -1, 120, "2: Positive Force", tr, tg, tb, true);
            break;
        case 3:
            graphics.Print( -1, 120, "3: Potential for Anything", tr, tg, tb, true);
            break;
        case 4:
            graphics.Print( -1, 120, "4: Passion for Exploring", tr, tg, tb, true);
            break;
        case 5:
            graphics.Print( -1, 120, "N/A: Pause", tr, tg, tb, true);
            break;
        case 6:
            graphics.Print( -1, 120, "5: Presenting VVVVVV", tr, tg, tb, true);
            break;
        case 7:
            graphics.Print( -1, 120, "N/A: Plenary", tr, tg, tb, true);
            break;
        case 8:
            graphics.Print( -1, 120, "6: Predestined Fate", tr, tg, tb, true);
            break;
        case 9:
            graphics.Print( -1, 120, "N/A: Positive Force Reversed", tr, tg, tb, true);
            break;
        case 10:
            graphics.Print( -1, 120, "7: Popular Potpourri", tr, tg, tb, true);
            break;
        case 11:
            graphics.Print( -1, 120, "8: Pipe Dream", tr, tg, tb, true);
            break;
        case 12:
            graphics.Print( -1, 120, "9: Pressure Cooker", tr, tg, tb, true);
            break;
        case 13:
            graphics.Print( -1, 120, "10: Paced Energy", tr, tg, tb, true);
            break;
        case 14:
            graphics.Print( -1, 120, "11: Piercing the Sky", tr, tg, tb, true);
            break;
        case 15:
            graphics.Print( -1, 120, "N/A: Predestined Fate Remix", tr, tg, tb, true);
            break;
        default:
            graphics.Print( -1, 120, "?: something else", tr, tg, tb, true);
            break;
        }
        break;
    case Menu::ed_quit:
        graphics.bigprint( -1, 90, "Save before", tr, tg, tb, true);
        graphics.bigprint( -1, 110, "quitting?", tr, tg, tb, true);
        break;
    default:
        break;
    }
}

void editorrender()
{
    if (game.shouldreturntoeditor)
    {
        graphics.backgrounddrawn = false;
    }

    //Draw grid

    FillRect(graphics.backBuffer, 0, 0, 320,240, graphics.getRGB(0,0,0));
    for(int j=0; j<30; j++)
    {
        for(int i=0; i<40; i++)
        {
            fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(8,8,8)); //a simple grid
            if(i%4==0) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(16,16,16));
            if(j%4==0) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(16,16,16));

            //Minor guides
            if(i==9) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(24,24,24));
            if(i==30) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(24,24,24));
            if(j==6 || j==7) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(24,24,24));
            if(j==21 || j==22) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(24,24,24));

            //Major guides
            if(i==20 || i==19) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(32,32,32));
            if(j==14) fillbox(i*8, j*8, (i*8)+7, (j*8)+7, graphics.getRGB(32,32,32));
        }
    }

    //Or draw background
    if(!ed.settingsmod)
    {
        switch(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir)
        {
        case 1:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.drawbackground(3);
            break;
        case 2:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.drawbackground(4);
            break;
        case 3:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.drawbackground(5);
            break;
        default:
            break;
        }

        if (ed.level[ed.levx+(ed.levy*ed.maxwidth)].tower)
            graphics.drawbackground(9);
    }

    //Draw map, in function
    int temp;
    if(ed.level[ed.levx+(ed.maxwidth*ed.levy)].tileset==0 || ed.level[ed.levx+(ed.maxwidth*ed.levy)].tileset==10)
    {
        for (int j = 0; j < 30; j++)
        {
            for (int i = 0; i < 40; i++)
            {
                temp = ed.gettilelocal(i, j);
                if(temp>0) graphics.drawtile(i*8,j*8,temp);
            }
        }
    }
    else if(ed.level[ed.levx+(ed.maxwidth*ed.levy)].tileset==5)
    {
        for (int j = 0; j < 30; j++)
        {
            for (int i = 0; i < 40; i++)
            {
                temp = ed.gettilelocal(i, j);
                if(temp>0) graphics.drawtile3(i*8,j*8,temp);
            }
        }
    }
    else
    {
        for (int j = 0; j < 30; j++)
        {
            for (int i = 0; i < 40; i++)
            {
                temp = ed.gettilelocal(i, j);
                if(temp>0) graphics.drawtile2(i*8,j*8,temp);
            }
        }
    }

    //Edge tile fix

    //Buffer the sides of the new room with tiles from other rooms, to ensure no gap problems.
    for(int j=0; j<30; j++)
    {
        //left edge
        if(ed.freewrap((ed.levx*40)-1,j+(ed.levy*30))==1)
        {
            FillRect(graphics.backBuffer, 0,j*8, 2,8, graphics.getRGB(255,255,255-help.glow));
        }
        //right edge
        if(ed.freewrap((ed.levx*40)+40,j+(ed.levy*30))==1)
        {
            FillRect(graphics.backBuffer, 318,j*8, 2,8, graphics.getRGB(255,255,255-help.glow));
        }
    }

    for(int i=0; i<40; i++)
    {
        if(ed.freewrap((ed.levx*40)+i,(ed.levy*30)-1)==1)
        {
            FillRect(graphics.backBuffer, i*8,0, 8,2, graphics.getRGB(255,255,255-help.glow));
        }

        if(ed.freewrap((ed.levx*40)+i,30+(ed.levy*30))==1)
        {
            FillRect(graphics.backBuffer, i*8,238, 8,2, graphics.getRGB(255,255,255-help.glow));
        }
    }

    std::string rmstr;
    rmstr = "("+help.String(ed.levx+1)+","+help.String(ed.levy+1)+")";
    int tower = ed.get_tower(ed.levx, ed.levy);
    if (tower)
        rmstr += "T" + help.String(tower) + ":" + help.String(ed.ypos);
    else if (ed.levaltstate != 0)
        rmstr += "@" + help.String(ed.levaltstate);

    int rmstrx = 318 - rmstr.length() * 8;

    //Draw entities
    obj.customplatformtile=game.customcol*12;

    int tx = ed.tilex;
    int ty = ed.tiley;
    if (!tower) {
        tx += ed.levx * 40;
        ty += ed.levy * 30;
    } else
        ty += ed.ypos;

    ed.temp=edentat(tx, ty, ed.levaltstate, tower);

    // Iterate backwards to make the editor draw in the same order as ingame
    for(int i=edentity.size() - 1; i >= 0; i--) {
        // Entity locations
        int ex = edentity[i].x;
        int ey = edentity[i].y;
        if (!tower) {
            ex -= ed.levx * 40;
            ey -= ed.levy * 30;
        } else
            ey -= ed.ypos;

        ex *= 8;
        ey *= 8;

        // Warp line/gravity line area
        tx = ex / 8;
        ty = ey / 8;
        int tx2 = ex / 8;
        int ty2 = ey / 8;
        if (tower) {
            ty += ed.ypos;
            ty2 += ed.ypos;
        }
        ex += edentity[i].subx;
        ey += edentity[i].suby;

        int len;
        SDL_Rect drawRect;
        int y_size = 30;
        if (tower)
            y_size = ed.tower_size(tower);

        // WARNING: Don't get any bright ideas about reducing indentation by negating this conditional and using a `continue`
        if (edentity[i].state == ed.levaltstate &&
            edentity[i].intower == tower &&
            (tower || (ex >= 0 && ex < 320 && ey >= 0 && ey < 240))) {

            int flipped = ed.level[ed.levx+(ed.levy*ed.maxwidth)].enemytype == 15 && edentity[i].p1 == 2 ? 24 : 0;
            // FIXME: UNUSED! Fix flipped enemies!
            (void) flipped; // suppress unused warning
            switch(edentity[i].t) {
            case 1: // Enemies
                graphics.drawspritesetcol(ex, ey, ed.getenemyframe(ed.level[ed.levx+(ed.levy*ed.maxwidth)].enemytype, edentity[i].p1),ed.entcolreal);
                if(edentity[i].p1==0)
                    graphics.Print(ex+4,ey+4, "V", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==1)
                    graphics.Print(ex+4,ey+4, "^", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==2)
                    graphics.Print(ex+4,ey+4, "<", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==3)
                    graphics.Print(ex+4,ey+4, ">", 255, 255, 255 - help.glow, false);
                break;
            case 2: // Moving platforms, conveyors
            case 3: { // Disappearing platforms
                int thetile = obj.customplatformtile;
                int theroomnum = ed.levx + ed.maxwidth*ed.levy;
                // Kludge for platforms/conveyors/quicksand in towers and tower hallways...
                if (ed.level[theroomnum].tileset == 5) {
                    thetile = ed.gettowerplattile(ed.level[theroomnum].tilecol);

                    thetile *= 12;
                }

                drawRect = graphics.tiles_rect;
                drawRect.x += ex;
                drawRect.y += ey;

                len = 32;
                if (edentity[i].t == 2 && edentity[i].p1 >= 7)
                    len *= 2;
                while (drawRect.x < (ex + len)) {
                    BlitSurfaceStandard(graphics.entcolours[thetile],
                                        NULL, graphics.backBuffer, &drawRect);
                    drawRect.x += 8;
                }

                fillboxabs(ex, ey, len, 8, graphics.getBGR(255, 255, 255));
                if (edentity[i].t == 3) {
                    graphics.Print(ex, ey, "////", 255, 255, 255 - help.glow, false);
                    break;
                }

                if (edentity[i].p1 == 5) {
                    graphics.Print(ex, ey, ">>>>", 255, 255, 255 - help.glow, false);
                    break;
                }

                if (edentity[i].p1 == 6) {
                    graphics.Print(ex, ey, "<<<<", 255, 255, 255 - help.glow, false);
                    break;
                }

                if (edentity[i].p1 == 7) {
                    graphics.Print(ex, ey, "> > > >", 255, 255, 255 - help.glow,
                                false);
                    break;
                }

                if (edentity[i].p1 == 8) {
                    graphics.Print(ex, ey, "< < < <", 255, 255, 255 - help.glow,
                                false);
                    break;
                }

                if(edentity[i].p1==0)
                    graphics.Print(ex+12,ey, "V", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==1)
                    graphics.Print(ex+12,ey, "^", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==2)
                    graphics.Print(ex+12,ey, "<", 255, 255, 255 - help.glow, false);
                if(edentity[i].p1==3)
                    graphics.Print(ex+12,ey, ">", 255, 255, 255 - help.glow, false);
                break;
            }
            case 5: // Flip Tokens
                graphics.drawspritesetcol(ex, ey, 192, obj.crewcolour(0));
                //graphics.drawsprite(ex, ty, 16 + !edentity[i].p1, 96, 96, 96);
                fillboxabs(ex, ey, 16, 16, graphics.getRGB(164,164,255));
                break;
            case 8: // Coin
                if(edentity[i].p1==0) {
                    graphics.huetilesetcol(8);
                    graphics.drawhuetile(ex, ey, 48);
                    fillboxabs(ex, ey, 8, 8, graphics.getRGB(164,164,164));
                }
                if(edentity[i].p1==1) {
                    graphics.drawspritesetcol(ex, ey, 196, 201);
                    fillboxabs(ex, ey, 16, 16, graphics.getRGB(164,164,164));
                }
                if(edentity[i].p1==2) {
                    graphics.drawspritesetcol(ex, ey, 197, 201);
                    fillboxabs(ex, ey, 16, 16, graphics.getRGB(164,164,164));
                }
                if(edentity[i].p1==3) {
                    graphics.drawspritesetcol(ex, ey, 198, 201);
                    fillboxabs(ex, ey, 24, 24, graphics.getRGB(164,164,164));
                }
                if(edentity[i].p1==4) {
                    graphics.drawspritesetcol(ex, ey, 199, 201);
                    fillboxabs(ex, ey, 24, 24, graphics.getRGB(164,164,164));
                }
                //graphics.drawsprite(ex, ey, 22, 196, 196, 196);
                break;
            case 9: // Shiny Trinket
                graphics.drawsprite(ex, ey, 22, 196, 196, 196);
                fillboxabs(ex, ey, 16, 16, graphics.getRGB(164, 164, 255));
                break;
            case 10: // Checkpoints
                if (edentity[i].p1 == 0 || edentity[i].p1 == 1)
                    graphics.drawsprite(ex, ey, 20 + edentity[i].p1, 196, 196, 196);
                else
                    graphics.drawsprite(ex, ey, 188 + edentity[i].p1, 196, 196, 196);
                fillboxabs(ex, ey, 16, 16, graphics.getRGB(164, 164, 255));
                break;
            case 11: // Gravity lines
                fillboxabs(ex, ey, 8, 8, graphics.getRGB(164,255,164));
                if(edentity[i].p1 == 0) { //Horizontal
                    while (tx >= 0 && !ed.spikefree(tx, ey / 8)) tx--;
                    while (tx2 < 40 && !ed.spikefree(tx2, ey / 8)) tx2++;
                    tx++;
                    FillRect(graphics.backBuffer, (tx*8), ey+4, (tx2-tx)*8, 1,
                             graphics.getRGB(194,194,194));
                    edentity[i].p2 = tx;
                    edentity[i].p3 = (tx2-tx)*8;
                } else { // Vertical
                    while (ty >= 0 && !ed.towerspikefree(tx, ty)) ty--;
                    while (ty2 < y_size && !ed.towerspikefree(tx, ty2)) ty2++;
                    ty++;
                    FillRect(graphics.backBuffer, (tx*8)+3, (ty*8) - (ed.ypos*8), 1,
                             (ty2-ty)*8, graphics.getRGB(194,194,194));
                    edentity[i].p2 = ty;
                    edentity[i].p3 = (ty2-ty) * 8;
                }
                break;
            case 13: // Warp tokens
                graphics.drawsprite(ex, ey, 18+(ed.entframe%2),196,196,196);
                fillboxabs(ex, ey, 16, 16, graphics.getRGB(164,164,255));
                if(ed.temp==i)
                    graphics.Print(ex, ey - 8, ed.warptokendest(i),210,210,255);
                else
                    graphics.Print(ex, ey - 8,
                                help.String(ed.findwarptoken(i)),210,210,255);
                break;
            case 14: // Teleporter
                graphics.drawtele(ex, ey, 1, 100);
                fillboxabs(ex, ey, 8*12, 8*12, graphics.getRGB(164,164,255));
                break;
            case 15: // Crewmates
                graphics.drawsprite(ex - 4, ey, 144, graphics.crewcolourreal(edentity[i].p1));
                fillboxabs(ex, ey, 16, 24, graphics.getRGB(164,164,164));
                break;
            case 16: // Start
                if (edentity[i].p1==0) // Left
                    graphics.drawspritesetcol(ex - 4, ey, 0, graphics.col_crewcyan);
                else if (edentity[i].p1==1)
                    graphics.drawspritesetcol(ex - 4, ey, 3, graphics.col_crewcyan);
                fillboxabs(ex, ey, 16, 24, graphics.getRGB(164,164,164));
                if(ed.entframe<2)
                    graphics.Print(ex - 12, ey - 8, "START", 255, 255, 255);
                else
                    graphics.Print(ex - 12, ey - 8, "START", 196, 196, 196);
                break;
            case 17: // Roomtext
                if(edentity[i].scriptname.length()<1) {
                    fillboxabs(ex, ey, 8, 8, graphics.getRGB(96, 96, 96));
                } else {
                    auto length = utf8::distance(edentity[i].scriptname.begin(),
                                                 edentity[i].scriptname.end());
                    fillboxabs(ex, ey, length*8, 8, graphics.getRGB(96,96,96));
                }
                graphics.Print(ex, ey, edentity[i].scriptname,
                            196, 196, 255 - help.glow);
                break;
            case 18: { // Terminals
                ty = ey;

                int usethistile = edentity[i].p1;

                if (usethistile == 0) {
                    usethistile = 1; // Unflipped
                } else if (usethistile == 1) {
                    usethistile = 0; // Flipped
                    ty -= 8;
                }

                graphics.drawsprite(ex, ty+8, 16 + usethistile, 96, 96, 96);
                fillboxabs(ex, ey, 16, 24, graphics.getRGB(164,164,164));
                if(ed.temp==i)
                    graphics.Print(ex, ey - 8, edentity[i].scriptname,210,210,255);
                break;
            }
            case 19: // Script Triggers
                fillboxabs(ex, ey, edentity[i].p1*8 + edentity[i].p3, edentity[i].p2*8 + edentity[i].p4,
                           edentity[i].onetime ? graphics.getRGB(255,255,164) : graphics.getRGB(255,164,255));
                fillboxabs(ex, ey, 8, 8, graphics.getRGB(255,255,255));
                if(ed.temp==i)
                    graphics.Print(ex, ey - 8, edentity[i].scriptname,210,210,255);
                break;
            case 20: // Activity Zones
                fillboxabs(ex, ey, edentity[i].p1*8 + edentity[i].p3, edentity[i].p2*8 + edentity[i].p4,
                           graphics.getRGB(164,255,164));
                fillboxabs(ex, ey, 8, 8, graphics.getRGB(255,255,255));
                if(ed.temp==i)
                    graphics.Print(ex, ey - 8, edentity[i].scriptname,210,210,255);
                break;
            case 50: // Warp lines
                fillboxabs(ex, ey, 8, 8, graphics.getRGB(164,255,164));
                if (edentity[i].p1>=2) { //Horizontal
                    while (tx >= 0 && !ed.free(tx, ey / 8)) tx--;
                    while (tx2 < 40 && !ed.free(tx2, ey / 8)) tx2++;
                    tx++;
                    fillboxabs((tx*8), ey+1, (tx2-tx)*8, 6,
                               graphics.getRGB(255,255,194));
                    edentity[i].p2=tx;
                    edentity[i].p3=(tx2-tx)*8;
                } else { // Vertical
                    while (ty >= 0 && !ed.towerfree(tx, ty)) ty--;
                    while (ty2 < y_size && !ed.towerfree(tx, ty2)) ty2++;
                    ty++;
                    fillboxabs((tx*8)+1, (ty*8) - (ed.ypos*8), 6,
                               (ty2-ty)*8, graphics.getRGB(255,255,194));
                    edentity[i].p2=ty;
                    edentity[i].p3=(ty2-ty)*8;
                }
                break;
            case 999: // ?
                //graphics.drawspritesetcol(ex, ey, 3, 102);
                graphics.setcol(102);
                graphics.drawimage(3, ex, ey);
                //graphics.drawsprite(ex, ty, 16 + !edentity[i].p1, 96, 96, 96);
                fillboxabs(ex, ey, 464, 320, graphics.getRGB(164,164,255));
                break;
            }
        }

        //Need to also check warp point destinations
        if(edentity[i].t==13 && ed.warpent!=i)
        {
            int ep1 = edentity[i].p1;
            int ep2 = edentity[i].p2;
            int etower = edentity[i].p3;
            if (!etower) {
                ep1 -= ed.levx * 40;
                ep2 -= ed.levy * 30;
            } else
                ep2 -= ed.ypos;
            ep1 *= 8;
            ep2 *= 8;
            if (tower == etower &&
                (tower || (ep1 >= 0 && ep1 < 320 && ep2 >= 0 && ep2 < 240)))
            {
                graphics.drawsprite(ep1, ep2, 18 + ed.entframe%2, 64, 64, 64);
                fillboxabs(ep1, ep2, 16, 16, graphics.getRGB(64, 64, 96));
                if (ed.tilex == ep1/8 && ed.tiley == ep2/8)
                {
                    graphics.bprint(ep1, ep2 - 8, ed.warptokendest(i), 190, 190, 225);
                }
                else
                {
                    graphics.bprint(ep1, ep2 - 8, help.String(ed.findwarptoken(i)), 190, 190, 225);
                }
            }
        }
    }

    if(ed.boundarymod>0)
    {
        if(ed.boundarymod==1)
        {
            fillboxabs(ed.tilex*8, ed.tiley*8, 8,8,graphics.getRGB(255-(help.glow/2),191+(help.glow),210+(help.glow/2)));
            fillboxabs((ed.tilex*8)+2, (ed.tiley*8)+2, 4,4,graphics.getRGB(128-(help.glow/4),100+(help.glow/2),105+(help.glow/4)));
        }
        else if(ed.boundarymod==2)
        {
            if((ed.tilex*8)+8<=ed.boundx1 || (ed.tiley*8)+8<=ed.boundy1)
            {
                fillboxabs(ed.boundx1, ed.boundy1, 8, 8,graphics.getRGB(255-(help.glow/2),191+(help.glow),210+(help.glow/2)));
                fillboxabs(ed.boundx1+2, ed.boundy1+2, 4, 4,graphics.getRGB(128-(help.glow/4),100+(help.glow/2),105+(help.glow/4)));
            }
            else
            {
                fillboxabs(ed.boundx1, ed.boundy1, (ed.tilex*8)+8-ed.boundx1,(ed.tiley*8)+8-ed.boundy1,graphics.getRGB(255-(help.glow/2),191+(help.glow),210+(help.glow/2)));
                fillboxabs(ed.boundx1+2, ed.boundy1+2, (ed.tilex*8)+8-ed.boundx1-4,(ed.tiley*8)+8-ed.boundy1-4,graphics.getRGB(128-(help.glow/4),100+(help.glow/2),105+(help.glow/4)));
            }
        }
    }
    else
    {
        //Draw boundaries
        int tmp=ed.levx+(ed.levy*ed.maxwidth);
        if(ed.level[tmp].enemyx1!=0 && ed.level[tmp].enemyy1!=0
                && ed.level[tmp].enemyx2!=320 && ed.level[tmp].enemyy2!=240)
        {
            fillboxabs( ed.level[tmp].enemyx1, ed.level[tmp].enemyy1,
                       ed.level[tmp].enemyx2-ed.level[tmp].enemyx1,
                       ed.level[tmp].enemyy2-ed.level[tmp].enemyy1,
                       graphics.getBGR(255-(help.glow/2),64,64));
        }

        if(ed.level[tmp].platx1!=0 && ed.level[tmp].platy1!=0
                && ed.level[tmp].platx2!=320 && ed.level[tmp].platy2!=240)
        {
            fillboxabs( ed.level[tmp].platx1, ed.level[tmp].platy1,
                       ed.level[tmp].platx2-ed.level[tmp].platx1,
                       ed.level[tmp].platy2-ed.level[tmp].platy1,
                       graphics.getBGR(64,64,255-(help.glow/2)));
        }
    }

    //Draw ghosts (spooky!)
    SDL_FillRect(graphics.ghostbuffer, NULL, SDL_MapRGBA(graphics.ghostbuffer->format, 0, 0, 0, 0));
    if (game.ghostsenabled) {
        for (int i = 0; i < (int)ed.ghosts.size(); i++) {
            if (i <= ed.currentghosts) { // We don't want all of them to show up at once :)
                if (ed.ghosts[i].rx != ed.levx || ed.ghosts[i].ry != ed.levy
                || !INBOUNDS(ed.ghosts[i].frame, graphics.sprites))
                    continue;

                point tpoint;
                tpoint.x = ed.ghosts[i].x;
                tpoint.y = ed.ghosts[i].y;
                graphics.setcolreal(ed.ghosts[i].realcol);
                Uint32 alpha = graphics.ct.colour & graphics.backBuffer->format->Amask;
                Uint32 therest = graphics.ct.colour & 0x00FFFFFF;
                alpha = (3 * (alpha >> 24) / 4) << 24;
                graphics.ct.colour = therest | alpha;
                SDL_Rect drawRect = graphics.sprites_rect;
                drawRect.x += tpoint.x;
                drawRect.y += tpoint.y;
                BlitSurfaceColoured(graphics.sprites[ed.ghosts[i].frame],NULL, graphics.ghostbuffer, &drawRect, graphics.ct);
            }
        }
        SDL_BlitSurface(graphics.ghostbuffer, NULL, graphics.backBuffer, &graphics.bg_rect);
    }

    //Draw Cursor
    if (!ed.trialstartpoint) {
        switch(ed.drawmode)
        {
        case 0:
        case 1:
        case 2:
        case 9:
        case 10:
        case 12: //Single point
            fillboxabs((ed.tilex*8),(ed.tiley*8),8,8, graphics.getRGB(200,32,32));
            break;
        case 3:
        case 4:
        case 8:
        case 13:
        case 17: //2x2
            fillboxabs((ed.tilex*8),(ed.tiley*8),16,16, graphics.getRGB(200,32,32));
            break;
        case 5:
        case 6:
        case 7://Platform
            fillboxabs((ed.tilex*8),(ed.tiley*8),32,8, graphics.getRGB(200,32,32));
            break;
        case 14: //X if not on edge
            if(ed.tilex==0 || ed.tilex==39 || ed.tiley==0 || ed.tiley==29)
            {
                fillboxabs((ed.tilex*8),(ed.tiley*8),8,8, graphics.getRGB(200,32,32));
            }
            else
            {
                graphics.Print((ed.tilex*8),(ed.tiley*8),"X",255,0,0);
            }
            break;
        case 11:
        case 15:
        case 16: //2x3
            fillboxabs((ed.tilex*8),(ed.tiley*8),16,24, graphics.getRGB(200,32,32));
            break;
        case 19: //12x12 :))))))
            fillboxabs((ed.tilex*8),(ed.tiley*8),8*12,8*12, graphics.getRGB(200,32,32));
            break;
        case 18: //Coins can be multiple sizes
            if (ed.zmod)      fillboxabs((ed.tilex*8),(ed.tiley*8),16,16, graphics.getRGB(200,32,32));
            else if (ed.xmod) fillboxabs((ed.tilex*8),(ed.tiley*8),16,16, graphics.getRGB(200,32,32));
            else if (ed.cmod) fillboxabs((ed.tilex*8),(ed.tiley*8),24,24, graphics.getRGB(200,32,32));
            else if (ed.vmod) fillboxabs((ed.tilex*8),(ed.tiley*8),24,24, graphics.getRGB(200,32,32));
            else fillboxabs((ed.tilex*8),(ed.tiley*8),8,8, graphics.getRGB(200,32,32));
            break;
        case -6: // ...?
            fillboxabs((ed.tilex*8),(ed.tiley*8),464,320, graphics.getRGB(200,32,32));
            break;
        }

        if(ed.drawmode<3)
        {
            if(ed.zmod && ed.drawmode<2)
            {
                fillboxabs((ed.tilex*8)-8,(ed.tiley*8)-8,24,24, graphics.getRGB(200,32,32));
            }
            else if(ed.xmod && ed.drawmode<2)
            {
                fillboxabs((ed.tilex*8)-16,(ed.tiley*8)-16,24+16,24+16, graphics.getRGB(200,32,32));
            }
            else if(ed.cmod && ed.drawmode<2)
            {
                fillboxabs((ed.tilex*8)-24,(ed.tiley*8)-24,24+32,24+32, graphics.getRGB(200,32,32));
            }
            else if(ed.vmod && ed.drawmode<2)
            {
                fillboxabs((ed.tilex*8)-32,(ed.tiley*8)-32,24+48,24+48, graphics.getRGB(200,32,32));
            }
            else if(ed.hmod && ed.drawmode<2)
            {
                fillboxabs(0,(ed.tiley*8),320,8,graphics.getRGB(200,32,32));
            }
            else if(ed.bmod && ed.drawmode<2)
            {
                fillboxabs((ed.tilex*8),0,8,240,graphics.getRGB(200,32,32));
            }

        }
    } else {
        graphics.drawspritesetcol((ed.tilex*8) - 4, (ed.tiley*8), 0, obj.crewcolour(0));
        fillboxabs((ed.tilex*8),(ed.tiley*8),16,24, graphics.getRGB(200,32,32));
    }

    //If in directmode, show current directmode tile
    if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode==1)
    {
        //Tile box for direct mode
        int t2=0;
        if(ed.dmtileeditor>0)
        {
            ed.dmtileeditor--;
            if(ed.dmtileeditor<=4)
            {
                t2=(4-ed.dmtileeditor)*12;
            }

            //Draw five lines of the editor
            temp=ed.dmtile-(ed.dmtile%dmwidth());
            temp-=dmwidth()*2;
            FillRect(graphics.backBuffer, 0,-t2,320,40, graphics.getRGB(0,0,0));
            FillRect(graphics.backBuffer, 0,-t2+40,320,2, graphics.getRGB(255,255,255));
            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==0)
            {
                for(int i=0; i<dmwidth(); i++)
                {
                    graphics.drawtile(i*8,0-t2,(temp+dmcap()+i)%dmcap());
                    graphics.drawtile(i*8,8-t2,(temp+dmcap()+dmwidth()*1+i)%dmcap());
                    graphics.drawtile(i*8,16-t2,(temp+dmcap()+dmwidth()*2+i)%dmcap());
                    graphics.drawtile(i*8,24-t2,(temp+dmcap()+dmwidth()*3+i)%dmcap());
                    graphics.drawtile(i*8,32-t2,(temp+dmcap()+dmwidth()*4+i)%dmcap());
                }
            }
            else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==5)
            {
                for(int i=0; i<dmwidth(); i++)
                {
                    graphics.drawtile3(i*8,0-t2,(temp+dmcap()+i)%dmcap());
                    graphics.drawtile3(i*8,8-t2,(temp+dmcap()+dmwidth()*1+i)%dmcap());
                    graphics.drawtile3(i*8,16-t2,(temp+dmcap()+dmwidth()*2+i)%dmcap());
                    graphics.drawtile3(i*8,24-t2,(temp+dmcap()+dmwidth()*3+i)%dmcap());
                    graphics.drawtile3(i*8,32-t2,(temp+dmcap()+dmwidth()*4+i)%dmcap());
                }
            }
            else
            {
                for(int i=0; i<dmwidth(); i++)
                {
                    graphics.drawtile2(i*8,0-t2,(temp+dmcap()+i)%dmcap());
                    graphics.drawtile2(i*8,8-t2,(temp+dmcap()+dmwidth()*1+i)%dmcap());
                    graphics.drawtile2(i*8,16-t2,(temp+dmcap()+dmwidth()*2+i)%dmcap());
                    graphics.drawtile2(i*8,24-t2,(temp+dmcap()+dmwidth()*3+i)%dmcap());
                    graphics.drawtile2(i*8,32-t2,(temp+dmcap()+dmwidth()*4+i)%dmcap());
                }
            }
            //Highlight our little block
            fillboxabs(((ed.dmtile%dmwidth())*8)-2,16-2,12,12,graphics.getRGB(196, 196, 255 - help.glow));
            fillboxabs(((ed.dmtile%dmwidth())*8)-1,16-1,10,10,graphics.getRGB(0,0,0));
        }

        if(ed.dmtileeditor>0 && t2<=30)
        {
            graphics.bprint(2, 45-t2, "Tile:", 196, 196, 255 - help.glow, false);
            graphics.bprint(58, 45-t2, help.String(ed.dmtile), 196, 196, 255 - help.glow, false);
            FillRect(graphics.backBuffer, 44,44-t2,10,10, graphics.getRGB(196, 196, 255 - help.glow));
            FillRect(graphics.backBuffer, 45,45-t2,8,8, graphics.getRGB(0,0,0));

            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==0)
            {
                graphics.drawtile(45,45-t2,ed.dmtile);
            }
            else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==5)
            {
                graphics.drawtile3(45,45-t2,ed.dmtile);
            }
            else
            {
                graphics.drawtile2(45,45-t2,ed.dmtile);
            }
        }
        else
        {
            graphics.bprint(2, 12, "Tile:", 196, 196, 255 - help.glow, false);
            graphics.bprint(58, 12, help.String(ed.dmtile), 196, 196, 255 - help.glow, false);
            FillRect(graphics.backBuffer, 44,11,10,10, graphics.getRGB(196, 196, 255 - help.glow));
            FillRect(graphics.backBuffer, 45,12,8,8, graphics.getRGB(0,0,0));

            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==0)
            {
                graphics.drawtile(45,12,ed.dmtile);
            }
            else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tileset==5)
            {
                graphics.drawtile3(45,12,ed.dmtile);
            }
            else
            {
                graphics.drawtile2(45,12,ed.dmtile);
            }
        }
    }




    //Draw GUI
    if(ed.boundarymod>0)
    {
        if(ed.boundarymod==1)
        {
            FillRect(graphics.backBuffer, 0,230,320,240, graphics.getRGB(32,32,32));
            FillRect(graphics.backBuffer, 0,231,320,240, graphics.getRGB(0,0,0));
            switch(ed.boundarytype)
            {
            case 0:
                graphics.Print(4, 232, "SCRIPT BOX: Click on top left", 255,255,255, false);
                break;
            case 1:
                graphics.Print(4, 232, "ENEMY BOUNDS: Click on top left", 255,255,255, false);
                break;
            case 2:
                graphics.Print(4, 232, "PLATFORM BOUNDS: Click on top left", 255,255,255, false);
                break;
            case 3:
                graphics.Print(4, 232, "COPY TILES: Click on top left", 255,255,255, false);
                break;
            case 4:
                graphics.Print(4, 232, "TOWER ENTRY: Click on top row", 255,255,255, false);
                break;
            default:
                graphics.Print(4, 232, "Click on top left", 255,255,255, false);
                break;
            }
        }
        else if(ed.boundarymod==2)
        {
            FillRect(graphics.backBuffer, 0,230,320,240, graphics.getRGB(32,32,32));
            FillRect(graphics.backBuffer, 0,231,320,240, graphics.getRGB(0,0,0));
            switch(ed.boundarytype)
            {
            case 0:
                graphics.Print(4, 232, "SCRIPT BOX: Click on bottom right", 255,255,255, false);
                break;
            case 1:
                graphics.Print(4, 232, "ENEMY BOUNDS: Click on bottom right", 255,255,255, false);
                break;
            case 2:
                graphics.Print(4, 232, "PLATFORM BOUNDS: Click on bottom right", 255,255,255, false);
                break;
            case 3:
                graphics.Print(4, 232, "COPY TILES: Click on bottom right", 255,255,255, false);
                break;
            case 4:
                graphics.Print(4, 232, "ACTIVITY ZONE: Click on top left", 255,255,255, false);
                break;
            default:
                graphics.Print(4, 232, "Click on bottom right", 255,255,255, false);
                break;
            }
        }
    }
    else if(ed.scripteditmod)
    {
        //Elaborate C64 BASIC menu goes here!
        FillRect(graphics.backBuffer, 0,0,320,240, graphics.getBGR(123, 111, 218));
        FillRect(graphics.backBuffer, 14,16,292,208, graphics.getRGB(162,48,61));
        switch(ed.scripthelppage)
        {
        case 0:
            graphics.Print(16,28,"**** VVVVVV SCRIPT EDITOR ****", 123, 111, 218, true);
            graphics.Print(16,44,"PRESS ESC TO RETURN TO MENU", 123, 111, 218, true);

            if(!ed.hooklist.empty())
            {
                for(int i=0; i<9; i++)
                {
                    if(ed.hookmenupage+i<(int)ed.hooklist.size())
                    {
                        if(ed.hookmenupage+i==ed.hookmenu)
                        {
                            std::string tstring="> " + ed.hooklist[(ed.hooklist.size()-1)-(ed.hookmenupage+i)] + " <";
                            std::transform(tstring.begin(), tstring.end(),tstring.begin(), ::toupper);
                            graphics.Print(16,68+(i*16),tstring,123, 111, 218, true);
                        }
                        else
                        {
                            graphics.Print(16,68+(i*16),ed.hooklist[(ed.hooklist.size()-1)-(ed.hookmenupage+i)],123, 111, 218, true);
                        }
                    }
                }
            }
            else
            {
                graphics.Print(16,110,"NO SCRIPT IDS FOUND", 123, 111, 218, true);
                graphics.Print(16,130,"CREATE A SCRIPT WITH EITHER", 123, 111, 218, true);
                graphics.Print(16,140,"THE TERMINAL OR SCRIPT BOX TOOLS", 123, 111, 218, true);
            }
            break;
        case 1:
            //Current scriptname
            FillRect(graphics.backBuffer, 14,226,292,12, graphics.getRGB(162,48,61));
            graphics.Print(16,228,"CURRENT SCRIPT: " + ed.sbscript, 123, 111, 218, true);
            //Draw text
            int y = 20;
            for(int i=0; i<25; i++)
            {
                if(i+ed.pagey<(int)ed.sb.size())
                {
                    auto text = ed.sb[i+ed.pagey];
                    if (i == ed.sby && ed.entframe < 2) text += "_";
                    if (graphics.Print(16,y,text, 123, 111, 218, false)) {
                        y += 16;
                    } else {
                        y += 8;
                    }
                }
            }
            break;
        }
    }
    else if (ed.trialstartpoint) {
        FillRect(graphics.backBuffer, 0,230,320,240, graphics.getRGB(32,32,32));
        FillRect(graphics.backBuffer, 0,231,320,240, graphics.getRGB(0,0,0));
        graphics.Print(4, 232, "TIME TRIALS: Place start point", 255,255,255, false);
    }
    else if(ed.settingsmod)
    {
        if(!game.colourblindmode)
        {
            graphics.drawtowerbackground();
        }
        else
        {
            FillRect(graphics.backBuffer, 0, 0, 320, 240, 0x00000000);
        }

        int tr = map.r - (help.glow / 4) - int(fRandom() * 4);
        int tg = map.g - (help.glow / 4) - int(fRandom() * 4);
        int tb = map.b - (help.glow / 4) - int(fRandom() * 4);
        if (tr < 0) tr = 0;
        if(tr>255) tr=255;
        if (tg < 0) tg = 0;
        if(tg>255) tg=255;
        if (tb < 0) tb = 0;
        if(tb>255) tb=255;
        editormenurender(tr, tg, tb);

        graphics.drawmenu(tr, tg, tb);
    } else if (ed.textmod) {
        FillRect(graphics.backBuffer, 0,221,320,240, graphics.getRGB(32,32,32));
        FillRect(graphics.backBuffer, 0,222,320,240, graphics.getRGB(0,0,0));
        graphics.Print(4, 224, ed.textdesc, 255,255,255, false);
        std::string input = key.keybuffer;
        if (ed.entframe < 2)
            input += "_";
        else
            input += " ";
        graphics.Print(4, 232, input, 196, 196, 255 - help.glow, true);
    }
    else if(ed.warpmod)
    {
        //placing warp token
        FillRect(graphics.backBuffer, 0,221,320,240, graphics.getRGB(32,32,32));
        FillRect(graphics.backBuffer, 0,222,320,240, graphics.getRGB(0,0,0));
        graphics.Print(4, 224, "Left click to place warp destination", 196, 196, 255 - help.glow, false);
        graphics.Print(4, 232, "Right click to cancel", 196, 196, 255 - help.glow, false);
    }
    else
    {
        if(ed.spacemod)
        {
            FillRect(graphics.backBuffer, 0,208,320,240, graphics.getRGB(32,32,32));
            FillRect(graphics.backBuffer, 0,209,320,240, graphics.getRGB(0,0,0));

            //Draw little icons for each thingy
            int tx=6, ty=211, tg=32;

            if(ed.spacemenu==0)
            {
                for(int i=0; i<10; i++)
                {
                    FillRect(graphics.backBuffer, 4+(i*tg), 209,20,20,graphics.getRGB(32,32,32));
                }
                FillRect(graphics.backBuffer, 4+(ed.drawmode*tg), 209,20,20,graphics.getRGB(64,64,64));
                //0:
                graphics.drawtile(tx,ty,83);
                graphics.drawtile(tx+8,ty,83);
                graphics.drawtile(tx,ty+8,83);
                graphics.drawtile(tx+8,ty+8,83);
                //1:
                tx+=tg;
                graphics.drawtile(tx,ty,680);
                graphics.drawtile(tx+8,ty,680);
                graphics.drawtile(tx,ty+8,680);
                graphics.drawtile(tx+8,ty+8,680);
                //2:
                tx+=tg;
                graphics.drawtile(tx+4,ty+4,8);
                //3:
                tx+=tg;
                graphics.drawsprite(tx,ty,22,196,196,196);
                //4:
                tx+=tg;
                graphics.drawsprite(tx,ty,21,196,196,196);
                //5:
                tx+=tg;
                graphics.drawtile(tx,ty+4,3);
                graphics.drawtile(tx+8,ty+4,4);
                //6:
                tx+=tg;
                graphics.drawtile(tx,ty+4,24);
                graphics.drawtile(tx+8,ty+4,24);
                //7:
                tx+=tg;
                graphics.drawtile(tx,ty+4,1);
                graphics.drawtile(tx+8,ty+4,1);
                //8:
                tx+=tg;
                graphics.drawsprite(tx,ty,78+ed.entframe,196,196,196);
                //9:
                tx+=tg;
                FillRect(graphics.backBuffer, tx+2,ty+8,12,1,graphics.getRGB(255,255,255));


                std::string toolkeys [10] = {"1","2","3","4","5","6","7","8","9","0"};
                for (int i = 0; i < 10; i++) {
                    int col = 96;
                    int col2 = 164;
                    if ((ed.drawmode) == i) {
                        col = 200;
                        col2 = 255;
                    }
                    fillboxabs(4+(i*tg), 209,20,20,graphics.getRGB(col,col,col));
                    graphics.Print((22+(i*tg)+4) - (toolkeys[i].length() * 8), 225-4, toolkeys[i],col2,col2,col2,false);
                }

                graphics.Print(4, 232, "1/2", 196, 196, 255 - help.glow, false);
            } else {
                for(int i=0; i<10; i++)
                    FillRect(graphics.backBuffer, 4+(i*tg), 209,20,20,graphics.getRGB(32,32,32));
                FillRect(graphics.backBuffer, 4+((ed.drawmode-10)*tg), 209,20,20,graphics.getRGB(64,64,64));
                //10:
                graphics.Print(tx,ty,"A",196, 196, 255 - help.glow, false);
                graphics.Print(tx+8,ty,"B",196, 196, 255 - help.glow, false);
                graphics.Print(tx,ty+8,"C",196, 196, 255 - help.glow, false);
                graphics.Print(tx+8,ty+8,"D",196, 196, 255 - help.glow, false);
                //11:
                tx+=tg;
                graphics.drawsprite(tx,ty,17,196,196,196);
                //12:
                tx+=tg;
                fillboxabs(tx+4,ty+4,8,8,graphics.getRGB(96,96,96));
                //13:
                tx+=tg;
                graphics.drawsprite(tx,ty,18+(ed.entframe%2),196,196,196);
                //14:
                tx+=tg;
                FillRect(graphics.backBuffer, tx+6,ty+2,4,12,graphics.getRGB(255,255,255));
                //15:
                tx+=tg;
                graphics.drawsprite(tx,ty,186,graphics.col_crewblue);
                //16:
                tx+=tg;
                graphics.drawsprite(tx,ty,184,graphics.col_crewcyan);
                //17:
                tx+=tg;
                graphics.drawsprite(tx,ty,192,graphics.col_crewcyan);
                //18:
                tx+=tg;
                graphics.huetilesetcol(8);
                graphics.drawhuetile(tx,   ty,   48);
                graphics.drawhuetile(tx+8, ty,   48);
                graphics.drawhuetile(tx,   ty+8, 48);
                graphics.drawhuetile(tx+8, ty+8, 48);
                //19:
                tx+=tg;
                graphics.drawtelepart(tx, ty, 1, 100);

                std::string toolkeys [10] = {"R","T","Y","U","I","O","P","^1","^2","^3"};
                for (int i = 0; i < 10; i++) {
                    int col = 96;
                    int col2 = 164;
                    if ((ed.drawmode - 10) == i) {
                        col = 200;
                        col2 = 255;
                    }
                    fillboxabs(4+(i*tg), 209,20,20,graphics.getRGB(col,col,col));
                    graphics.Print((22+(i*tg)+4) - (toolkeys[i].length() * 8), 225-4, toolkeys[i],col2,col2,col2,false);
                }

                graphics.Print(4, 232, "2/2", 196, 196, 255 - help.glow, false);
            }

            graphics.Print(128, 232, "< and > keys change tool", 196, 196, 255 - help.glow, false);

            FillRect(graphics.backBuffer, 0,198,120,10, graphics.getRGB(32,32,32));
            FillRect(graphics.backBuffer, 0,199,119,9, graphics.getRGB(0,0,0));
            switch(ed.drawmode)
            {
            case 0:
                graphics.bprint(2,199, "1: Walls",196, 196, 255 - help.glow);
                break;
            case 1:
                graphics.bprint(2,199, "2: Backing",196, 196, 255 - help.glow);
                break;
            case 2:
                graphics.bprint(2,199, "3: Spikes",196, 196, 255 - help.glow);
                break;
            case 3:
                graphics.bprint(2,199, "4: Trinkets",196, 196, 255 - help.glow);
                break;
            case 4:
                graphics.bprint(2,199, "5: Checkpoint",196, 196, 255 - help.glow);
                break;
            case 5:
                graphics.bprint(2,199, "6: Disappear",196, 196, 255 - help.glow);
                break;
            case 6:
                graphics.bprint(2,199, "7: Conveyors",196, 196, 255 - help.glow);
                break;
            case 7:
                graphics.bprint(2,199, "8: Moving",196, 196, 255 - help.glow);
                break;
            case 8:
                graphics.bprint(2,199, "9: Enemies",196, 196, 255 - help.glow);
                break;
            case 9:
                graphics.bprint(2,199, "0: Grav Line",196, 196, 255 - help.glow);
                break;
            case 10:
                graphics.bprint(2,199, "R: Roomtext",196, 196, 255 - help.glow);
                break;
            case 11:
                graphics.bprint(2,199, "T: Terminal",196, 196, 255 - help.glow);
                break;
            case 12:
                graphics.bprint(2,199, "Y: Script Box",196, 196, 255 - help.glow);
                break;
            case 13:
                graphics.bprint(2,199, "U: Warp Token",196, 196, 255 - help.glow);
                break;
            case 14:
                graphics.bprint(2,199, "I: Warp Lines",196, 196, 255 - help.glow);
                break;
            case 15:
                graphics.bprint(2,199, "O: Crewmate",196, 196, 255 - help.glow);
                break;
            case 16:
                graphics.bprint(2,199, "P: Start Point",196, 196, 255 - help.glow);
                break;
            case 17:
                graphics.bprint(2,199, "^1: Flip Token",196, 196, 255 - help.glow);
                break;
            case 18:
                graphics.bprint(2,199, "^2: Coin",196, 196, 255 - help.glow);
                break;
            case 19:
                graphics.bprint(2,199, "^3: Teleporter",196, 196, 255 - help.glow);
                break;
            default:
                graphics.bprint(2,199, "?: ???",196, 196, 255 - help.glow);
                break;
            }

            FillRect(graphics.backBuffer, 260-24,198,80+24,10, graphics.getRGB(32,32,32));
            FillRect(graphics.backBuffer, 261-24,199,80+24,9, graphics.getRGB(0,0,0));
            graphics.bprint(rmstrx, 199, rmstr, 196, 196, 255 - help.glow, false);
        } else {
            //FillRect(graphics.backBuffer, 0,230,72,240, graphics.RGB(32,32,32));
            //FillRect(graphics.backBuffer, 0,231,71,240, graphics.RGB(0,0,0));
            if(ed.level[ed.levx+(ed.maxwidth*ed.levy)].roomname!="")
            {
                if(ed.tiley<28)
                {
                    if(ed.roomnamehide>0) ed.roomnamehide--;
                }
                else
                {
                    if(ed.roomnamehide<12) ed.roomnamehide++;
                }
                if (graphics.translucentroomname)
                {
                    graphics.footerrect.y = 230+ed.roomnamehide;
                    SDL_BlitSurface(graphics.footerbuffer, NULL, graphics.backBuffer, &graphics.footerrect);
                }
                else
                {
                    FillRect(graphics.backBuffer, 0,230+ed.roomnamehide,320,10, graphics.getRGB(0,0,0));
                }
                graphics.bprint(5,231+ed.roomnamehide,ed.level[ed.levx+(ed.maxwidth*ed.levy)].roomname, 196, 196, 255 - help.glow, true);
                graphics.bprint(4, 222, "Ctrl+F1: Help", 196, 196, 255 - help.glow, false);
                graphics.bprint(rmstrx, 222, rmstr,196, 196, 255 - help.glow, false);
            }
            else
            {
                graphics.bprint(4, 232, "Ctrl+F1: Help", 196, 196, 255 - help.glow, false);
                graphics.bprint(rmstrx,232, rmstr,196, 196, 255 - help.glow, false);
            }
        }

        if(ed.shiftmenu)
        {
            fillboxabs(0, 47,161+8,200,graphics.getRGB(64,64,64));
            FillRect(graphics.backBuffer, 0,48,160+8,200, graphics.getRGB(0,0,0));
            graphics.Print(4, 50, "Space: Mode Select", 164, 164, 164, false);
            if (tower) {
                graphics.Print(4, 60, "F1: Tower Direction",164,164,164,false);
                graphics.Print(4, 70, "F2: Tower Entry",164,164,164,false);
            } else {
                graphics.Print(4,  60, "F1: Change Tileset",164,164,164,false);
                graphics.Print(4,  70, "F2: Change Colour",164,164,164,false);
            }
            graphics.Print(4,  80, "F3: Change Enemies",164,164,164,false);
            graphics.Print(4,  90, "F4: Enemy Bounds",164,164,164,false);
            graphics.Print(4, 100, "F5: Platform Bounds",164,164,164,false);

            if (tower) {
                graphics.Print(4, 120, "F6: Next Tower",164,164,164,false);
                graphics.Print(4, 130, "F7: Previous Tower",164,164,164,false);
            } else {
                graphics.Print(4, 120, "F6: New Alt State",164,164,164,false);
                graphics.Print(4, 130, "F7: Remove Alt State",164,164,164,false);
            }

            graphics.Print(4, 150, "F8: Tower Mode",164,164,164,false);
            graphics.Print(4, 160, "F9: Custom Tileset",164,164,164,false);
            graphics.Print(4, 180, "F10: Direct Mode",164,164,164,false);

            if (tower) {
                graphics.Print(4, 200, "+: Scroll Down",164,164,164,false);
                graphics.Print(4, 210, "-: Scroll Up",164,164,164,false);
            } else {
                graphics.Print(4, 200, "A: Change Alt State",164,164,164,false);
                graphics.Print(4, 210, "W: Change Warp Dir",164,164,164,false);
            }
            graphics.Print(4, 220, "E: Change Roomname",164,164,164,false);

            fillboxabs(220, 207,100,60,graphics.getRGB(64,64,64));
            FillRect(graphics.backBuffer, 221,208,160,60, graphics.getRGB(0,0,0));
            graphics.Print(224, 210, "S: Save Map",164,164,164,false);
            graphics.Print(224, 220, "L: Load Map",164,164,164,false);
        }
    }


    if(!ed.settingsmod && !ed.scripteditmod)
    {
        //Same as above, without borders
        switch(ed.drawmode)
        {
        case 0:
            graphics.bprint(2,2, "1: Walls",196, 196, 255 - help.glow);
            break;
        case 1:
            graphics.bprint(2,2, "2: Backing",196, 196, 255 - help.glow);
            break;
        case 2:
            graphics.bprint(2,2, "3: Spikes",196, 196, 255 - help.glow);
            break;
        case 3:
            graphics.bprint(2,2, "4: Trinkets",196, 196, 255 - help.glow);
            break;
        case 4:
            graphics.bprint(2,2, "5: Checkpoint",196, 196, 255 - help.glow);
            break;
        case 5:
            graphics.bprint(2,2, "6: Disappear",196, 196, 255 - help.glow);
            break;
        case 6:
            graphics.bprint(2,2, "7: Conveyors",196, 196, 255 - help.glow);
            break;
        case 7:
            graphics.bprint(2,2, "8: Moving, Speed: " + std::to_string(ed.entspeed + ed.level[ed.levx+(ed.maxwidth*ed.levy)].platv),196, 196, 255 - help.glow);
            break;
        case 8:
            graphics.bprint(2,2, "9: Enemies, Speed: " + std::to_string(ed.entspeed + ed.level[ed.levx+(ed.maxwidth*ed.levy)].enemyv),196, 196, 255 - help.glow);
            break;
        case 9:
            graphics.bprint(2,2, "0: Grav Line",196, 196, 255 - help.glow);
            break;
        case 10:
            graphics.bprint(2,2, "R: Roomtext",196, 196, 255 - help.glow);
            break;
        case 11:
            graphics.bprint(2,2, "T: Terminal",196, 196, 255 - help.glow);
            break;
        case 12:
            if (ed.zmod) {
                graphics.bprint(2,2, "Y+Z: Activity Zone",196, 196, 255 - help.glow);
            } else if (ed.xmod) {
                graphics.bprint(2,2, "Y+X: One-Time Script Box",196, 196, 255 - help.glow);
            } else {
                graphics.bprint(2,2, "Y: Script Box",196, 196, 255 - help.glow);
            }
            break;
        case 13:
            graphics.bprint(2,2, "U: Warp Token",196, 196, 255 - help.glow);
            break;
        case 14:
            graphics.bprint(2,2, "I: Warp Lines",196, 196, 255 - help.glow);
            break;
        case 15:
            graphics.bprint(2,2, "O: Crewmate",196, 196, 255 - help.glow);
            break;
        case 16:
            graphics.bprint(2,2, "P: Start Point",196, 196, 255 - help.glow);
            break;
        case 17:
            graphics.bprint(2,2, "^1: Flip Token",196, 196, 255 - help.glow);
            break;
        case 18:
            if (ed.zmod)      graphics.bprint(2,2, "^2+Z: 10 Coin",196, 196, 255 - help.glow);
            else if (ed.xmod) graphics.bprint(2,2, "^2+X: 20 Coin",196, 196, 255 - help.glow);
            else if (ed.cmod) graphics.bprint(2,2, "^2+C: 50 Coin",196, 196, 255 - help.glow);
            else if (ed.vmod) graphics.bprint(2,2, "^2+V: 100 Coin",196, 196, 255 - help.glow);
            else              graphics.bprint(2,2, "^2: Coin",196, 196, 255 - help.glow);
            break;
        case 19:
            graphics.bprint(2,2, "^3: Teleporter",196, 196, 255 - help.glow);
            break;
        default:
            graphics.bprint(2,2, "?: ???",196, 196, 255 - help.glow);
            break;
        }
    }

    if(ed.notedelay>0 || ed.oldnotedelay>0)
    {
        float alpha = graphics.lerp(ed.oldnotedelay, ed.notedelay);
        FillRect(graphics.backBuffer, 0,115,320,18, graphics.getRGB(92,92,92));
        FillRect(graphics.backBuffer, 0,116,320,16, graphics.getRGB(0,0,0));
        graphics.Print(0,121, ed.note,196-((45.0f-alpha)*4), 196-((45.0f-alpha)*4), 196-((45.0f-alpha)*4), true);
    }

    graphics.drawfade();

    graphics.render();
}

void editorlogic()
{
    //Misc
    help.updateglow();
    graphics.updatetitlecolours();

    game.customcol=ed.getlevelcol(ed.levx+(ed.levy*ed.maxwidth))+1;
    ed.entcol=ed.getenemycol(game.customcol);
    if (ed.grayenemieskludge) {
        ed.entcol = 18;
        ed.grayenemieskludge = false;
    }

    graphics.setcol(ed.entcol);
    ed.entcolreal = graphics.ct.colour;

    if (game.shouldreturntoeditor)
    {
        game.shouldreturntoeditor = false;
    }

    map.bypos -= 2;
    map.bscroll = -2;

    ed.entframedelay--;
    if(ed.entframedelay<=0)
    {
        ed.entframe=(ed.entframe+1)%4;
        ed.entframedelay=8;
    }

    ed.oldnotedelay = ed.notedelay;
    if(ed.notedelay>0)
    {
        ed.notedelay--;
    }

    if (game.ghostsenabled)
    {
        for (size_t i = 0; i < ed.ghosts.size(); i++)
        {
            GhostInfo& ghost = ed.ghosts[i];

            if ((int) i > ed.currentghosts || ghost.rx != ed.levx || ghost.ry != ed.levy)
            {
                continue;
            }

            graphics.setcol(ghost.col);
            ghost.realcol = graphics.ct.colour;
        }

        if (ed.currentghosts + 1 < (int)ed.ghosts.size()) {
            ed.currentghosts++;
            if (ed.zmod) ed.currentghosts++;
        } else {
            ed.currentghosts = (int)ed.ghosts.size() - 1;
        }
    }

    if (!ed.settingsmod)
    {
        switch(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir)
        {
        case 1:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.updatebackground(3);
            break;
        case 2:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.updatebackground(4);
            break;
        case 3:
            graphics.rcol=ed.getwarpbackground(ed.levx, ed.levy);
            graphics.updatebackground(5);
            break;
        default:
            break;
        }
    }
    else if (!game.colourblindmode)
    {
        graphics.updatetowerbackground();
    }

    if (graphics.fademode == 1)
    {
        //Return to game
        map.nexttowercolour();
        map.colstate = 10;
        game.gamestate = TITLEMODE;
        script.hardreset();
        graphics.fademode = 4;
        music.haltdasmusik();
        FILESYSTEM_unmountassets(); // should be before music.play(6)
        music.play(6);
        map.nexttowercolour();
        ed.settingsmod=false;
        ed.trialmod=false;
        graphics.backgrounddrawn=false;
        game.returntomenu(Menu::playerworlds);
    }
}


void editormenuactionpress()
{
    switch (game.currentmenuname)
    {
    case Menu::ed_trials:
        if (game.currentmenuoption == (int)ed.customtrials.size())
        {
            customtrial temp;
            temp.name = "Trial " + std::to_string(ed.customtrials.size() + 1);
            ed.customtrials.push_back(temp);
            ed.edtrial = (int)ed.customtrials.size() - 1;
            music.playef(11);
            game.createmenu(Menu::ed_edit_trial);
        }
        else if (game.currentmenuoption == (int)ed.customtrials.size()+1)
        {
            music.playef(11);
            game.returnmenu();
            map.nexttowercolour();
        }
        else
        {
            ed.edtrial = game.currentmenuoption;
            music.playef(11);
            game.createmenu(Menu::ed_edit_trial);
        }
        break;
    case Menu::ed_edit_trial:
        switch (game.currentmenuoption) {
        case 0:
            ed.textentry=true;
            ed.trialnamemod=true;
            key.enabletextentry();
            key.keybuffer=ed.customtrials[ed.edtrial].name;
            break;
        case 1:
            ed.trialstartpoint = true;
            ed.settingsmod = false;
            music.playef(11);
            break;
        case 2:
            music.playef(11);
            ed.customtrials[ed.edtrial].music++;
            if (ed.customtrials[ed.edtrial].music > 15) ed.customtrials[ed.edtrial].music = 0;
            break;
        case 3:
            music.playef(11);
            ed.trialmod = true;
            break;
        case 4:
            music.playef(11);
            ed.trialmod = true;
            break;
        case 5:
            music.playef(11);
            game.createmenu(Menu::ed_remove_trial);
            map.nexttowercolour();
            break;
        case 6:
            music.playef(11);
            game.returnmenu();
            map.nexttowercolour();
            break;
        }
        break;
    case Menu::ed_remove_trial:
        switch (game.currentmenuoption) {
        case 0:
            ed.customtrials.erase(ed.customtrials.begin() + ed.edtrial);
            music.playef(11);
            game.returntomenu(Menu::ed_trials);
            map.nexttowercolour();
            break;
        default:
            music.playef(11);
            game.returnmenu();
            map.nexttowercolour();
            break;
        }
        break;
    case Menu::ed_desc:
        switch (game.currentmenuoption)
        {
        case 0:
            ed.textentry=true;
            ed.titlemod=true;
            key.enabletextentry();
            key.keybuffer=EditorData::GetInstance().title;
            break;
        case 1:
            ed.textentry=true;
            ed.creatormod=true;
            key.enabletextentry();
            key.keybuffer=EditorData::GetInstance().creator;
            break;
        case 2:
            ed.textentry=true;
            ed.desc1mod=true;
            key.enabletextentry();
            key.keybuffer=ed.Desc1;
            break;
        case 3:
            ed.textentry=true;
            ed.websitemod=true;
            key.enabletextentry();
            key.keybuffer=ed.website;
            break;
        case 4:
            music.playef(11);
            game.returnmenu();
            map.nexttowercolour();
            break;
        }
        break;
    case Menu::ed_settings:
        switch (game.currentmenuoption)
        {
        case 0:
            //Change level description stuff
            music.playef(11);
            game.createmenu(Menu::ed_desc);
            map.nexttowercolour();
            break;
        case 1:
            //Enter script editormode
            music.playef(11);
            ed.scripteditmod=true;
            ed.clearscriptbuffer();
            key.enabletextentry();
            key.keybuffer="";
            ed.hookmenupage=0;
            ed.hookmenu=0;
            ed.scripthelppage=0;
            ed.scripthelppagedelay=0;
            ed.sby=0;
            ed.sbx=0, ed.pagey=0;
            break;
        case 2:
            music.playef(11);
            game.createmenu(Menu::ed_trials);
            map.nexttowercolour();
            break;
        case 3:
            music.playef(11);
            game.createmenu(Menu::ed_music);
            map.nexttowercolour();
            if(ed.levmusic>0) music.play(ed.levmusic);
            break;
        case 4:
            music.playef(11);
            game.ghostsenabled = !game.ghostsenabled;
            break;
        case 5:
            //Load level
            ed.settingsmod=false;
            map.nexttowercolour();

            ed.keydelay = 6;
            ed.getlin(TEXT_LOAD, "Enter map filename "
                      "to load:", &(ed.filename));
            game.mapheld=true;
            graphics.backgrounddrawn=false;
            break;
        case 6:
            //Save level
            ed.settingsmod=false;
            map.nexttowercolour();

            ed.keydelay = 6;
            ed.getlin(TEXT_SAVE, "Enter map filename "
                      "to save map as:", &(ed.filename));
            game.mapheld=true;
            graphics.backgrounddrawn=false;
            break;
        case 7:
            music.playef(11);
            game.createmenu(Menu::ed_settings2, true);
            map.nexttowercolour();
            break;
        case 8:
            music.playef(11);
            game.createmenu(Menu::ed_quit);
            map.nexttowercolour();
            break;
        }
        break;
    case Menu::ed_settings2:
        switch (game.currentmenuoption) {
        case 0: {
            int tower = ed.get_tower(ed.levx, ed.levy);
            if (tower) {
                // Change Scroll Direction
                ed.towers[tower-1].scroll = !ed.towers[tower-1].scroll;
                ed.notedelay=45;
                if (ed.towers[tower-1].scroll)
                    ed.note="Tower now Descending";
                else
                    ed.note="Tower now Ascending";
                ed.updatetiles=true;
                ed.keydelay=6;
            } else {
                ed.switch_tileset(true);
                graphics.backgrounddrawn=false;
            }
            break;
        }
        case 1:
            key.fakekey = SDLK_F2;
            key.fakekeytimer = 6;
            ed.settingsmod = false;
            break;
        case 2:
            key.fakekey = SDLK_F3;
            key.fakekeytimer = 6;
            ed.settingsmod = false;
            break;
        case 3:
            key.fakekey = SDLK_F4;
            key.fakekeytimer = 6;
            ed.settingsmod = false;
            break;
        case 4:
            key.fakekey = SDLK_F5;
            key.fakekeytimer = 6;
            ed.settingsmod = false;
            break;
        case 5: {
            int tower = ed.get_tower(ed.levx, ed.levy);
            if (tower) {
                if (ed.level[ed.levx + ed.levy*ed.maxwidth].tower < ed.maxwidth * ed.maxheight) {
                    ed.level[ed.levx + ed.levy*ed.maxwidth].tower++;
                }

                ed.note = "Tower Changed";
                ed.keydelay = 6;
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.snap_tower_entry(ed.levx, ed.levy);
            } else {
                int newaltstate = ed.getnumaltstates(ed.levx, ed.levy) + 1;
                ed.addaltstate(ed.levx, ed.levy, newaltstate);
                ed.keydelay = 6;
                ed.notedelay = 45;
                // But did we get a new alt state?
                if (ed.getedaltstatenum(ed.levx, ed.levy, newaltstate) == -1) {
                    // Don't switch to the new alt state, or we'll segfault!
                    ed.note = "ERROR: Couldn't add new alt state";
                } else {
                    ed.note = "Added new alt state " + help.String(newaltstate);
                    ed.levaltstate = newaltstate;
                }
            }
            break;
        }
        case 6: {
            int tower = ed.get_tower(ed.levx, ed.levy);
            if (tower) {
                if (ed.level[ed.levx + ed.levy*ed.maxwidth].tower > 1) {
                    ed.level[ed.levx + ed.levy*ed.maxwidth].tower--;
                }

                ed.note = "Tower Changed";
                ed.keydelay = 6;
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.snap_tower_entry(ed.levx, ed.levy);
            } else {
                if (ed.levaltstate == 0) {
                    ed.note = "Cannot remove main state";
                } else {
                    ed.removealtstate(ed.levx, ed.levy, ed.levaltstate);
                    ed.note = "Removed alt state " + help.String(ed.levaltstate);
                    ed.levaltstate--;
                }
                ed.keydelay = 6;
                ed.notedelay = 45;
            }
            break;
        }
        }
        if (game.currentmenuoption == (int) game.menuoptions.size() - 1) {
            music.playef(11);
            game.createmenu(Menu::ed_settings3, true);
            map.nexttowercolour();
        }
        break;
    case Menu::ed_settings3:
        switch (game.currentmenuoption) {
        case 0:
            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tower) {
                ed.level[ed.levx+(ed.levy*ed.maxwidth)].tower=0;
                ed.note="Tower Mode Disabled";
            } else {
                ed.enable_tower();
                ed.note="Tower Mode Enabled";
            }
            graphics.backgrounddrawn=false;

            ed.notedelay=45;
            ed.updatetiles=true;
            ed.keydelay=6;

            game.createmenu(Menu::ed_settings3, true);
            break;
        case 1:
            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode==1)
            {
                ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode=0;
                ed.note="Direct Mode Disabled";
            }
            else
            {
                ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode=1;
                ed.note="Direct Mode Enabled";
            }
            graphics.backgrounddrawn=false;

            ed.notedelay=45;
            ed.updatetiles=true;
            break;
        case 2: {
            int tower = ed.get_tower(ed.levx, ed.levy);
            if (tower) {
                music.playef(2);
            } else {
                if (ed.getedaltstatenum(ed.levx, ed.levy, ed.levaltstate + 1) != -1) {
                    ed.levaltstate++;
                    ed.note = "Switched to alt state " + help.String(ed.levaltstate);
                } else if (ed.levaltstate == 0) {
                    ed.note = "No alt states in this room";
                } else {
                    ed.levaltstate = 0;
                    ed.note = "Switched to main state";
                }
                ed.notedelay = 45;
            }
            break;
        }
        case 3: {
            int tower = ed.get_tower(ed.levx, ed.levy);
            if (tower) {
                music.playef(2);
            } else {
                int j=0, tx=0, ty=0;
                for(size_t i=0; i<edentity.size(); i++)
                {
                    if(edentity[i].t==50)
                    {
                        tx=(edentity[i].p1-(edentity[i].p1%40))/40;
                        ty=(edentity[i].p2-(edentity[i].p2%30))/30;
                        if(tx==ed.levx && ty==ed.levy &&
                        edentity[i].state==ed.levaltstate &&
                        edentity[i].intower==tower)
                        {
                            j++;
                        }
                    }
                }
                if(j>0)
                {
                    ed.note="ERROR: Cannot have both warp types";
                    ed.notedelay=45;
                }
                else
                {
                    ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir=(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir+1)%4;
                    if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==0)
                    {
                        ed.note="Room warping disabled";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==1)
                    {
                        ed.note="Room warps horizontally";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==2)
                    {
                        ed.note="Room warps vertically";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==3)
                    {
                        ed.note="Room warps in all directions";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                }
                ed.keydelay=6;
            }
            break;
        }
        case 4:
            ed.getlin(TEXT_ROOMNAME, "Enter new room name:",
                &(ed.level[ed.levx+(ed.levy*ed.maxwidth)].roomname));
            ed.settingsmod = 0;
            break;
        case 5:
            game.createmenu(Menu::ed_dimensions);
            break;
        }
        if (game.currentmenuoption == (int) game.menuoptions.size() - 1) {
            music.playef(11);
            game.createmenu(Menu::ed_settings, true);
            map.nexttowercolour();
        }
        break;
    case Menu::ed_music:
        switch (game.currentmenuoption)
        {
        case 0:
            ed.levmusic++;
            //if(ed.levmusic==5) ed.levmusic=6;
            //if(ed.levmusic==7) ed.levmusic=8;
            //if(ed.levmusic==9) ed.levmusic=10;
            //if(ed.levmusic==15) ed.levmusic=0;
            if(ed.levmusic==16) ed.levmusic=0;
            if(ed.levmusic>0)
            {
                music.play(ed.levmusic);
            }
            else
            {
                music.haltdasmusik();
            }
            music.playef(11);
            break;
        case 1:
            music.playef(11);
            music.fadeout();
            game.returnmenu();
            map.nexttowercolour();
            break;
        }
        break;
    case Menu::ed_quit:
        switch (game.currentmenuoption)
        {
        case 0:
            //Saving and quit
            ed.saveandquit=true;
            ed.settingsmod=false;
            map.nexttowercolour();

            ed.keydelay = 6;
            ed.getlin(TEXT_SAVE, "Enter map filename "
                      "to save map as:", &(ed.filename));
            game.mapheld=true;
            graphics.backgrounddrawn=false;
            break;
        case 1:
            //Quit without saving
            music.playef(11);
            music.fadeout();
            graphics.fademode = 2;
            break;
        case 2:
            //Go back to editor
            music.playef(11);
            game.returnmenu();
            map.nexttowercolour();
            break;
        }
        break;
    default:
        break;
    }
}

void editorinput()
{
    game.mx = (float) key.mx;
    game.my = (float) key.my;
    ed.tilex=(game.mx - (game.mx%8))/8;
    ed.tiley=(game.my - (game.my%8))/8;
    if (game.stretchMode == 1) {
        // In this mode specifically, we have to fix the mouse coordinates
        int winwidth, winheight;
        graphics.screenbuffer->GetWindowSize(&winwidth, &winheight);
        ed.tilex = ed.tilex * 320 / winwidth;
        ed.tiley = ed.tiley * 240 / winheight;
    }

    game.press_left = false;
    game.press_right = false;
    game.press_action = false;
    game.press_map = false;

    if (key.isDown(KEYBOARD_LEFT) || key.isDown(KEYBOARD_a) || key.controllerWantsLeft(true))
    {
        game.press_left = true;
    }
    if (key.isDown(KEYBOARD_RIGHT) || key.isDown(KEYBOARD_d) || key.controllerWantsRight(true))
    {
        game.press_right = true;
    }
    if (key.isDown(KEYBOARD_z) || key.isDown(KEYBOARD_SPACE) || key.isDown(KEYBOARD_v) || key.isDown(game.controllerButton_flip))
    {
        game.press_action = true;
    }

    if (key.keymap[SDLK_F11] && (ed.keydelay==0)) {
        ed.keydelay = 30;
        ed.note="Reloaded resources";
        ed.notedelay=45;
        graphics.reloadresources();
    }

    int tower = ed.get_tower(ed.levx, ed.levy);

    if (key.isDown(KEYBOARD_ENTER) || key.isDown(SDL_CONTROLLER_BUTTON_BACK)) game.press_map = true;
    if ((key.isDown(27) || key.isDown(SDL_CONTROLLER_BUTTON_START)) && !ed.settingskey)
    {
        ed.settingskey=true;
        if (ed.textmod) {
            key.disabletextentry();
            if (ed.textmod >= FIRST_ENTTEXT && ed.textmod <= LAST_ENTTEXT) {
                *ed.textptr = ed.oldenttext;
                if (ed.oldenttext == "")
                    removeedentity(ed.textent);
            }

            ed.textmod = TEXT_NONE;

            ed.shiftmenu = false;
            ed.shiftkey = false;
        } else if (ed.textentry) {
            key.disabletextentry();
            ed.textentry=false;
            ed.titlemod=false;
            ed.trialnamemod=false;
            ed.desc1mod=false;
            ed.desc2mod=false;
            ed.desc3mod=false;
            ed.websitemod=false;
            ed.creatormod=false;

            ed.shiftmenu=false;
            ed.shiftkey=false;
        }
        else if(ed.boundarymod>0)
        {
            ed.boundarymod=0;
        }
        else if (ed.trialstartpoint) {
            ed.trialstartpoint = false;
            ed.settingsmod = true;
        }
        else
        {
            ed.settingsmod=!ed.settingsmod;
            ed.trialmod = false;
            graphics.backgrounddrawn=false;

            if (ed.settingsmod)
            {
                bool edsettings_in_stack = false;
                for (size_t i = 0; i < game.menustack.size(); i++)
                {
                    if (game.menustack[i].name == Menu::ed_settings)
                    {
                        edsettings_in_stack = true;
                        break;
                    }
                }
                if (edsettings_in_stack)
                {
                    game.returntomenu(Menu::ed_settings);
                }
                else
                {
                    game.createmenu(Menu::ed_settings);
                }
                map.nexttowercolour();
            }
        }
    }

    if (!key.isDown(27) && !key.isDown(SDL_CONTROLLER_BUTTON_START))
    {
        ed.settingskey=false;
    }

    if(ed.scripteditmod)
    {
        if(ed.scripthelppage==0)
        {
            //hook select menu
            if(ed.keydelay>0) ed.keydelay--;

            if((key.keymap[SDLK_UP] || key.keymap[SDLK_KP_8]) && ed.keydelay<=0)
            {
                ed.keydelay=6;
                ed.hookmenu--;
            }

            if((key.keymap[SDLK_DOWN] || key.keymap[SDLK_KP_2]) && ed.keydelay<=0)
            {
                ed.keydelay=6;
                ed.hookmenu++;
            }

            if(ed.hookmenu>=(int)ed.hooklist.size())
            {
                ed.hookmenu=ed.hooklist.size()-1;
            }
            if(ed.hookmenu<0) ed.hookmenu=0;

            if(ed.hookmenu<ed.hookmenupage)
            {
                ed.hookmenupage=ed.hookmenu;
            }

            if(ed.hookmenu>=ed.hookmenupage+9)
            {
                ed.hookmenupage=ed.hookmenu+8;
            }

            if(!key.keymap[SDLK_BACKSPACE]) ed.deletekeyheld=0;

            if(key.keymap[SDLK_BACKSPACE] && ed.deletekeyheld==0 && !ed.hooklist.empty())
            {
                ed.deletekeyheld=1;
                music.playef(2);
                ed.removehook(ed.hooklist[(ed.hooklist.size()-1)-ed.hookmenu]);
            }

            if (!game.press_action && !game.press_left && !game.press_right
                    && !key.keymap[SDLK_UP] && !key.keymap[SDLK_DOWN]
                    && !key.keymap[SDLK_KP_8] && !key.keymap[SDLK_KP_2] && !key.isDown(27)) game.jumpheld = false;
            if (!game.jumpheld)
            {
                if (game.press_action || game.press_left || game.press_right || game.press_map
                        || key.keymap[SDLK_UP] || key.keymap[SDLK_DOWN]
                        || key.keymap[SDLK_KP_8] || key.keymap[SDLK_KP_2] || key.isDown(27))
                {
                    game.jumpheld = true;
                }
                if ((game.press_action || game.press_map) && !ed.hooklist.empty())
                {
                    game.mapheld=true;
                    ed.scripthelppage=1;
                    key.keybuffer="";
                    ed.sbscript=ed.hooklist[(ed.hooklist.size()-1)-ed.hookmenu];
                    ed.loadhookineditor(ed.sbscript);

                    ed.sby=ed.sb.size()-1;
                    ed.pagey=0;
                    while(ed.sby>=20)
                    {
                        ed.pagey++;
                        ed.sby--;
                    }
                    key.keybuffer=ed.sb[ed.pagey+ed.sby];
                    ed.sbx = graphics.strwidth(ed.sb[ed.pagey+ed.sby]) / 8;
                }

                if (key.isDown(27) || key.controllerButtonDown())
                {
                    ed.scripteditmod=false;
                    ed.settingsmod=false;
                    ed.trialmod=false;
                }
            }
        }
        else if(ed.scripthelppage==1)
        {
            //Script editor!
            if (key.isDown(27))
            {
                ed.scripthelppage=0;
                game.jumpheld = true;
                //save the script for use again!
                ed.addhook(ed.sbscript);
            }

            if(ed.keydelay>0) ed.keydelay--;

            if(key.keymap[SDLK_UP] && ed.keydelay<=0)
            {
                ed.keydelay=6;
                ed.sby--;
                if(ed.sby<=5)
                {
                    if(ed.pagey>0)
                    {
                        ed.pagey--;
                        ed.sby++;
                    }
                    else
                    {
                        if(ed.sby<0) ed.sby=0;
                    }
                }
                key.keybuffer=ed.sb[ed.pagey+ed.sby];
            }

            if(key.keymap[SDLK_DOWN] && ed.keydelay<=0)
            {
                ed.keydelay=6;
                if(ed.sby+ed.pagey<(int)ed.sb.size()-1)
                {
                    ed.sby++;
                    if(ed.sby>=20)
                    {
                        ed.pagey++;
                        ed.sby--;
                    }
                }
                key.keybuffer=ed.sb[ed.pagey+ed.sby];
            }

            if(key.linealreadyemptykludge)
            {
                ed.keydelay=6;
                key.linealreadyemptykludge=false;
            }

            if(key.pressedbackspace && ed.sb[ed.pagey+ed.sby]=="" && ed.keydelay<=0)
            {
                //Remove this line completely
                ed.removeline(ed.pagey+ed.sby);
                ed.sby--;
                if(ed.sby<=5)
                {
                    if(ed.pagey>0)
                    {
                        ed.pagey--;
                        ed.sby++;
                    }
                    else
                    {
                        if(ed.sby<0) ed.sby=0;
                    }
                }
                key.keybuffer=ed.sb[ed.pagey+ed.sby];
                ed.keydelay=6;
            }

            ed.sb[ed.pagey+ed.sby]=key.keybuffer;
            ed.sbx = graphics.strwidth(ed.sb[ed.pagey+ed.sby]) / 8;

            if(!game.press_map && !key.isDown(27)) game.mapheld=false;
            if (!game.mapheld)
            {
                if(game.press_map)
                {
                    game.mapheld=true;
                    //Continue to next line
                    if(ed.sby+ed.pagey>=(int)ed.sb.size()) //we're on the last line
                    {
                        ed.sby++;
                        if(ed.sby>=20)
                        {
                            ed.pagey++;
                            ed.sby--;
                        }
                        key.keybuffer=ed.sb[ed.pagey+ed.sby];
                        ed.sbx = graphics.strwidth(ed.sb[ed.pagey+ed.sby]) / 8;
                    }
                    else
                    {
                        //We're not, insert a line instead
                        ed.sby++;
                        if(ed.sby>=20)
                        {
                            ed.pagey++;
                            ed.sby--;
                        }
                        ed.insertline(ed.sby+ed.pagey);
                        key.keybuffer="";
                        ed.sbx = 0;
                    }
                }
            }
        }
    } else if (ed.textmod) {
        *ed.textptr = key.keybuffer;

        if (!game.press_map && !key.isDown(27))
            game.mapheld = false;
        if ((!game.mapheld && game.press_map) || !key.textentrymode) {
            game.mapheld = true;
            if (!ed.textcount)
                key.disabletextentry();

            std::vector<std::string> coords;
            std::string filename = ed.filename+".vvvvvv";
            switch (ed.textmod) {
            case TEXT_GOTOROOM:
                coords = split(key.keybuffer, ',');
                if (coords.size() == 2) {
                    ed.levx = (atoi(coords[0].c_str()) - 1) % ed.mapwidth;
                    if (ed.levx < 0)
                        ed.levx = 0;
                    ed.levy = (atoi(coords[1].c_str()) - 1) % ed.mapheight;
                    if (ed.levy < 0)
                        ed.levy = 0;
                }
                break;
            case TEXT_LOAD:
                if (ed.load(filename))
                    // don't use filename, it has the full path
                    ed.note = "[ Loaded map: "+ed.filename+".vvvvvv ]";
                else
                    ed.note = "[ ERROR: Could not load level! ]";
                ed.notedelay = 45;
                break;
            case TEXT_SAVE:
                if (ed.save(filename))
                    ed.note="[ Saved map: " + ed.filename+".vvvvvv ]";
                else {
                    ed.note="[ ERROR: Could not save level! ]";
                    ed.saveandquit = false;
                }
                ed.notedelay=45;

                if(ed.saveandquit)
                    graphics.fademode = 2; // quit editor
                break;
            case TEXT_ACTIVITYZONE:
                if (ed.textcount == 2) {
                    ed.textptr = &(edentity[ed.textent].activitycolor);
                    ed.textdesc = "Enter activity zone color:";
                } else if (ed.textcount == 1) {
                    ed.textptr = &(edentity[ed.textent].scriptname);
                    ed.textdesc = "Enter script name:";
                }

                if (ed.textcount) {
                    key.keybuffer = *ed.textptr;
                    ed.oldenttext = key.keybuffer;
                    break;
                }

                [[fallthrough]];
            case TEXT_SCRIPT:
                ed.clearscriptbuffer();
                if (!ed.checkhook(key.keybuffer))
                    ed.addhook(key.keybuffer);
                break;
            default:
                break;
            }

            if (!ed.textcount) {
                ed.shiftmenu = false;
                ed.shiftkey = false;
                ed.textmod = TEXT_NONE;
            } else
                ed.textcount--;
        }
    } else if (ed.textentry) {
        if(ed.titlemod)
        {
            EditorData::GetInstance().title=key.keybuffer;
        }
        else if (ed.trialnamemod) {
            ed.customtrials[ed.edtrial].name=key.keybuffer;
        }
        else if(ed.creatormod)
        {
            EditorData::GetInstance().creator=key.keybuffer;
        }
        else if(ed.websitemod)
        {
            ed.website=key.keybuffer;
        }
        else if(ed.desc1mod)
        {
            ed.Desc1=key.keybuffer;
        }
        else if(ed.desc2mod)
        {
            ed.Desc2=key.keybuffer;
        }
        else if(ed.desc3mod)
        {
            ed.Desc3=key.keybuffer;
        }

        if(!game.press_map && !key.isDown(27)) game.mapheld=false;
        if (!game.mapheld)
        {
            if(game.press_map)
            {
                game.mapheld=true;
                if(ed.titlemod)
                {
                    EditorData::GetInstance().title=key.keybuffer;
                    ed.titlemod=false;
                }
                else if (ed.trialnamemod) {
                    ed.customtrials[ed.edtrial].name = key.keybuffer;
                    ed.trialnamemod=false;
                }
                else if(ed.creatormod)
                {
                    EditorData::GetInstance().creator=key.keybuffer;
                    ed.creatormod=false;
                }
                else if(ed.websitemod)
                {
                    ed.website=key.keybuffer;
                    ed.websitemod=false;
                }
                else if(ed.desc1mod)
                {
                    ed.Desc1=key.keybuffer;
                }
                else if(ed.desc2mod)
                {
                    ed.Desc2=key.keybuffer;
                }
                else if(ed.desc3mod)
                {
                    ed.Desc3=key.keybuffer;
                    ed.desc3mod=false;
                }
                ed.textentry=false;

                if(ed.desc1mod)
                {
                    ed.desc1mod=false;

                    ed.textentry=true;
                    ed.desc2mod=true;
                    key.enabletextentry();
                    key.keybuffer=ed.Desc2;
                }
                else if(ed.desc2mod)
                {
                    ed.desc2mod=false;

                    ed.textentry=true;
                    ed.desc3mod=true;
                    key.enabletextentry();
                    key.keybuffer=ed.Desc3;
                }
            }
        }
    }
    else
    {
        if(ed.settingsmod)
        {
            if (!game.press_action && !game.press_left && !game.press_right
                    && !key.keymap[SDLK_UP] && !key.keymap[SDLK_DOWN] && !key.keymap[SDLK_PAGEUP] && !key.keymap[SDLK_PAGEDOWN]) game.jumpheld = false;
            if (!game.jumpheld)
            {
                if (game.press_action || game.press_left || game.press_right || game.press_map
                        || key.keymap[SDLK_UP] || key.keymap[SDLK_DOWN] || key.keymap[SDLK_PAGEUP] || key.keymap[SDLK_PAGEDOWN])
                {
                    game.jumpheld = true;
                }

                if(game.menustart)
                {
                    if (game.press_left || key.keymap[SDLK_UP])
                    {
                        if (!ed.trialmod) game.currentmenuoption--;
                    }
                    else if (game.press_right || key.keymap[SDLK_DOWN])
                    {
                        if (!ed.trialmod) game.currentmenuoption++;
                    }
                }

                if (game.currentmenuname != Menu::ed_trials) {
                    if (game.currentmenuoption < 0) game.currentmenuoption = game.menuoptions.size()-1;
                    if (game.currentmenuoption >= (int) game.menuoptions.size() ) game.currentmenuoption = 0;
                } else {
                    if (game.currentmenuoption < 0) game.currentmenuoption = (int)ed.customtrials.size()+1;
                    if (game.currentmenuoption > (int)ed.customtrials.size()+1) game.currentmenuoption = 0;
                }

                if (ed.trialmod) {
                    if (game.press_action) {
                        ed.trialmod = false;
                        game.jumpheld = true;
                        music.playef(11);
                    }
                    if (game.currentmenuoption == 3) {
                        if (game.press_left || key.keymap[SDLK_UP]) ed.customtrials[ed.edtrial].trinkets--;
                        if (game.press_right || key.keymap[SDLK_DOWN]) ed.customtrials[ed.edtrial].trinkets++;
                        if (ed.customtrials[ed.edtrial].trinkets > 99) ed.customtrials[ed.edtrial].trinkets = 0;
                        if (ed.customtrials[ed.edtrial].trinkets < 0) ed.customtrials[ed.edtrial].trinkets = 99;
                    }
                    if (game.currentmenuoption == 4) {
                        if (game.press_left || key.keymap[SDLK_UP]) ed.customtrials[ed.edtrial].par--;
                        if (game.press_right || key.keymap[SDLK_DOWN]) ed.customtrials[ed.edtrial].par++;
                        if (key.keymap[SDLK_PAGEDOWN]) ed.customtrials[ed.edtrial].par += 60;
                        if (key.keymap[SDLK_PAGEUP]) ed.customtrials[ed.edtrial].par -= 60;
                        if (ed.customtrials[ed.edtrial].par > 600) ed.customtrials[ed.edtrial].par = 0;
                        if (ed.customtrials[ed.edtrial].par < 0) ed.customtrials[ed.edtrial].par = 600;
                    }
                }
                else if (game.press_action)
                {
                    editormenuactionpress();
                }
            }
        } else if (ed.keydelay) {
            ed.keydelay--;
        } else if (ed.trialstartpoint) {
            // Allow the player to switch rooms
            ed.switchroomsinput();
            if(key.leftbutton) {
                ed.trialstartpoint = false;
                ed.customtrials[ed.edtrial].startx = (ed.tilex*8) - 4;
                ed.customtrials[ed.edtrial].starty = (ed.tiley*8);
                ed.customtrials[ed.edtrial].startf = 0;
                ed.customtrials[ed.edtrial].roomx = ed.levx;
                ed.customtrials[ed.edtrial].roomy = ed.levy;
                ed.settingsmod = true;
            }
        } else if ((key.keymap[SDLK_LSHIFT] || key.keymap[SDLK_RSHIFT]) &&
                   (key.keymap[SDLK_LCTRL] || key.keymap[SDLK_RCTRL])) {
            // Ctrl+Shift modifiers
            // TODO: Better Direct Mode interface
            ed.dmtileeditor=10;
            if(key.keymap[SDLK_LEFT]) {
                ed.dmtile--;
                ed.keydelay=3;
                if(ed.dmtile<0) ed.dmtile+=dmcap();
            } else if(key.keymap[SDLK_RIGHT]) {
                ed.dmtile++;
                ed.keydelay=3;

                if (ed.dmtile>=dmcap())
                    ed.dmtile-=dmcap();
            }
            if(key.keymap[SDLK_UP]) {
                ed.dmtile-=dmwidth();
                ed.keydelay=3;
                if(ed.dmtile<0) ed.dmtile+=dmcap();
            } else if(key.keymap[SDLK_DOWN]) {
                ed.dmtile+=dmwidth();
                ed.keydelay=3;

                if(ed.dmtile>=dmcap()) ed.dmtile-=dmcap();
            }

            // CONTRIBUTORS: keep this a secret :)
            if (key.keymap[SDLK_6]) {
                ed.drawmode=-6;
                ed.keydelay = 6;
            }

            if(key.keymap[SDLK_F9]) {
                int nextspritesheet = cycle_through_custom_resources(ed.getcustomsprites(), graphics.customsprites, false);

                ed.level[ed.levx + ed.levy*ed.maxwidth].customspritesheet = nextspritesheet;

                if (nextspritesheet == 0)
                    ed.note = "Now using default spritesheet";
                else
                    ed.note = "Now using sprites" + std::to_string(nextspritesheet) + ".png";
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.keydelay = 6;
            }
        } else if (key.keymap[SDLK_LCTRL] || key.keymap[SDLK_RCTRL]) {
            // Ctrl modifiers
            if (key.keymap[SDLK_F1]) {
                // Help screen
                ed.shiftmenu = !ed.shiftmenu;
                ed.keydelay = 6;
            }

            if (key.keymap[SDLK_p] || key.keymap[SDLK_o] ||
                key.keymap[SDLK_t]) {
                // Jump to player location, next crewmate or trinket
                int ent = 16; // player
                if (key.keymap[SDLK_o])
                    ent = 15;
                else if (key.keymap[SDLK_t])
                    ent = 9;

                if (ed.lastentcycle != ent) {
                    ed.entcycle = 0;
                    ed.lastentcycle = ent;
                }

                // Find next entity of this kind
                ed.entcycle++;
                int num_ents = 0;
                size_t i;
                for (i = 0; i < edentity.size(); i++)
                    if (edentity[i].t == ent)
                        num_ents++;

                if (ed.entcycle > num_ents)
                    ed.entcycle = 1;

                num_ents = 0;
                for (i = 0; i < edentity.size(); i++) {
                    if (edentity[i].t == ent) {
                        num_ents++;
                        if (ed.entcycle == num_ents)
                            break;
                    }
                }

                int roomx = ed.levx;
                int roomy = ed.levy;
                if (num_ents && !edentity[i].intower) {
                    roomx = edentity[i].x / 40;
                    roomy = edentity[i].y / 30;
                }

                if (roomx != ed.levx || roomy != ed.levy) {
                    ed.levx = mod(roomx, ed.mapwidth);
                    ed.levy = mod(roomy, ed.mapheight);
                    ed.updatetiles = true;
                    ed.changeroom = true;
                    graphics.backgrounddrawn=false;
                    ed.levaltstate = 0;
                    ed.keydelay = 12;
                }
            }
            int speedcap = 16;

            if (key.keymap[SDLK_COMMA] || key.isDown(SDL_CONTROLLER_BUTTON_X)) {
                ed.keydelay = 6;
                ed.entspeed--;
                if (ed.entspeed < -speedcap) ed.entspeed = speedcap;
            }

            if (key.keymap[SDLK_PERIOD] || key.isDown(SDL_CONTROLLER_BUTTON_Y)) {
                ed.keydelay = 6;
                ed.entspeed++;
                if (ed.entspeed > speedcap) ed.entspeed = -speedcap;
            }

            if(key.keymap[SDLK_F9]) {
                int nextspritesheet = cycle_through_custom_resources(ed.getcustomsprites(), graphics.customsprites, true);

                ed.level[ed.levx + ed.levy*ed.maxwidth].customspritesheet = nextspritesheet;

                if (nextspritesheet == 0)
                    ed.note = "Now using default spritesheet";
                else
                    ed.note = "Now using sprites" + std::to_string(nextspritesheet) + ".png";
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.keydelay = 6;
            }

        } else if (key.keymap[SDLK_LSHIFT] || key.keymap[SDLK_RSHIFT]) {
            // Shift modifiers
            if (key.keymap[SDLK_UP] || key.keymap[SDLK_DOWN] ||
                key.keymap[SDLK_LEFT] || key.keymap[SDLK_RIGHT] ||
                key.keymap[SDLK_KP_8] || key.keymap[SDLK_KP_2] ||
                key.keymap[SDLK_KP_4] || key.keymap[SDLK_KP_6]) {
                ed.keydelay = 6;
                if (key.keymap[SDLK_UP] || key.keymap[SDLK_KP_8])
                    ed.mapheight--;
                else if (key.keymap[SDLK_DOWN] || key.keymap[SDLK_KP_2])
                    ed.mapheight++;
                else if (key.keymap[SDLK_LEFT] || key.keymap[SDLK_KP_4])
                    ed.mapwidth--;
                else if (key.keymap[SDLK_RIGHT] || key.keymap[SDLK_KP_6])
                    ed.mapwidth++;

                if (ed.mapheight < 1)
                    ed.mapheight = 1;
                if (ed.mapheight > ed.maxheight)
                    ed.mapheight = ed.maxheight;
                if (ed.mapwidth < 1)
                    ed.mapwidth = 1;
                if (ed.mapwidth > ed.maxwidth)
                    ed.mapwidth = ed.maxwidth;

                ed.note = "Mapsize is now [" + help.String(ed.mapwidth) + "," +
                    help.String(ed.mapheight) + "]";
                ed.notedelay=45;
            }

            if (tower && (key.keymap[SDLK_F1] || key.keymap[SDLK_F2])) {
                ed.notedelay=45;
                ed.note="Unavailable in Tower Mode";
                ed.updatetiles=true;
                ed.keydelay=6;
            }
            if (key.keymap[SDLK_F1]) {
                ed.switch_tileset(true);
                graphics.backgrounddrawn=false;
                ed.keydelay = 6;
            }
            if (key.keymap[SDLK_F2]) {
                ed.switch_tilecol(true);
                graphics.backgrounddrawn=false;
                ed.keydelay = 6;
            }

            if (key.keymap[SDLK_1]) ed.drawmode=17;
            if (key.keymap[SDLK_2]) ed.drawmode=18;
            if (key.keymap[SDLK_3]) ed.drawmode=19;

            if(key.keymap[SDLK_F9]) {
                int nexttilesheet = cycle_through_custom_resources(ed.getcustomtiles(), graphics.customtiles, false);

                ed.level[ed.levx + ed.levy*ed.maxwidth].customtileset = nexttilesheet;

                if (nexttilesheet == 0)
                    ed.note = "Now using default tilesheet";
                else
                    ed.note = "Now using tiles" + std::to_string(nexttilesheet) + ".png";
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.keydelay = 6;
                graphics.backgrounddrawn = false;
            }

        } else {
            // No modifiers
            if (key.keymap[SDLK_COMMA] || key.keymap[SDLK_PERIOD] || key.isDown(SDL_CONTROLLER_BUTTON_X) || key.isDown(SDL_CONTROLLER_BUTTON_Y)) {
                ed.keydelay = 6;
                if (key.keymap[SDLK_PERIOD] || key.isDown(SDL_CONTROLLER_BUTTON_Y))
                    if (ed.drawmode != -6)
                        ed.drawmode++;
                    else
                        ed.drawmode = 0;
                else {
                    ed.drawmode--;
                    if (ed.drawmode < 0 && ed.drawmode != -6)
                        ed.drawmode = 19;
                }
                if (ed.drawmode != -6) ed.drawmode %= 20;

                ed.spacemenu = 0;
                if (ed.drawmode > 9)
                    ed.spacemenu = 1;
            }
            if (tower && (key.keymap[SDLK_w] || key.keymap[SDLK_a])) {
                ed.notedelay=45;
                ed.note="Unavailable in Tower Mode";
                ed.updatetiles=true;
                ed.keydelay=6;
            }
            if (tower && key.keymap[SDLK_F1]) {
                // Change Scroll Direction
                ed.towers[tower-1].scroll = !ed.towers[tower-1].scroll;
                ed.notedelay=45;
                if (ed.towers[tower-1].scroll)
                    ed.note="Tower now Descending";
                else
                    ed.note="Tower now Ascending";
                ed.updatetiles=true;
                ed.keydelay=6;
            }
            if (tower && key.keymap[SDLK_F2]) {
                // Change Tower Entry
                ed.keydelay=6;
                ed.boundarytype=4;
                ed.boundarymod=1;
            }
            if (tower &&
                (key.keymap[SDLK_F6] || key.keymap[SDLK_F7])) {
                // Change Used Tower
                if (key.keymap[SDLK_F7]) {
                    if (ed.level[ed.levx + ed.levy*ed.maxwidth].tower > 1)
                        ed.level[ed.levx + ed.levy*ed.maxwidth].tower--;
                } else if (ed.level[ed.levx + ed.levy*ed.maxwidth].tower < ed.maxwidth * ed.maxheight)
                    ed.level[ed.levx + ed.levy*ed.maxwidth].tower++;

                ed.note = "Tower Changed";
                ed.keydelay = 6;
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.snap_tower_entry(ed.levx, ed.levy);
            }
            if (tower &&
                (key.keymap[SDLK_PLUS] || key.keymap[SDLK_KP_PLUS] ||
                 key.keymap[SDLK_EQUALS] || key.keymap[SDLK_KP_EQUALS] ||
                 key.keymap[SDLK_MINUS] || key.keymap[SDLK_KP_MINUS] ||
                 key.keymap[SDLK_HOME] || key.keymap[SDLK_END] ||
                 key.keymap[SDLK_PAGEUP] || key.keymap[SDLK_PAGEDOWN])) {
                int modpos = 1;
                if (key.keymap[SDLK_LSHIFT] || key.keymap[SDLK_RSHIFT])
                    modpos = 5;
                if (key.keymap[SDLK_PAGEUP] || key.keymap[SDLK_PAGEDOWN])
                    modpos = 30;
                if (key.keymap[SDLK_HOME] || key.keymap[SDLK_END])
                    modpos = ed.tower_size(tower);
                if (key.keymap[SDLK_MINUS] || key.keymap[SDLK_KP_MINUS] ||
                    key.keymap[SDLK_HOME] || key.keymap[SDLK_PAGEUP])
                    modpos *= -1;
                ed.ypos += modpos;
                ed.snap_tower_entry(ed.levx, ed.levy);
            }
            if(!tower && key.keymap[SDLK_F1])
            {
                ed.switch_tileset(false);
                graphics.backgrounddrawn=false;
                ed.keydelay = 6;
            }
            if(!tower && key.keymap[SDLK_F2])
            {
                ed.switch_tilecol(false);
                graphics.backgrounddrawn=false;
                ed.keydelay = 6;
            }
            if(key.keymap[SDLK_F3])
            {
                ed.level[ed.levx+(ed.levy*ed.maxwidth)].enemytype=(ed.level[ed.levx+(ed.levy*ed.maxwidth)].enemytype+1)%28;
                ed.keydelay=6;
                ed.notedelay=45;
                ed.note="Enemy Type Changed";
            }
            if(key.keymap[SDLK_F4])
            {
                ed.keydelay=6;
                ed.boundarytype=1;
                ed.boundarymod=1;
            }
            if(key.keymap[SDLK_F5])
            {
                ed.keydelay=6;
                ed.boundarytype=2;
                ed.boundarymod=1;
            }
            if (key.keymap[SDLK_F6]) {
                int newaltstate = ed.getnumaltstates(ed.levx, ed.levy) + 1;
                ed.addaltstate(ed.levx, ed.levy, newaltstate);
                ed.keydelay = 6;
                ed.notedelay = 45;
                // But did we get a new alt state?
                if (ed.getedaltstatenum(ed.levx, ed.levy, newaltstate) == -1) {
                    // Don't switch to the new alt state, or we'll segfault!
                    ed.note = "ERROR: Couldn't add new alt state";
                } else {
                    ed.note = "Added new alt state " + help.String(newaltstate);
                    ed.levaltstate = newaltstate;
                }
            }
            if (key.keymap[SDLK_F7]) {
                if (ed.levaltstate == 0) {
                    ed.note = "Cannot remove main state";
                } else {
                    ed.removealtstate(ed.levx, ed.levy, ed.levaltstate);
                    ed.note = "Removed alt state " + help.String(ed.levaltstate);
                    ed.levaltstate--;
                }
                ed.keydelay = 6;
                ed.notedelay = 45;
            }
            if(key.keymap[SDLK_F8]) {
                if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].tower) {
                    ed.level[ed.levx+(ed.levy*ed.maxwidth)].tower=0;
                    ed.note="Tower Mode Disabled";
                } else {
                    ed.enable_tower();
                    ed.note="Tower Mode Enabled";
                }
                graphics.backgrounddrawn=false;

                ed.notedelay=45;
                ed.updatetiles=true;
                ed.keydelay=6;
            }
            if(key.keymap[SDLK_F9]) {
                int nexttilesheet = cycle_through_custom_resources(ed.getcustomtiles(), graphics.customtiles, true);

                ed.level[ed.levx + ed.levy*ed.maxwidth].customtileset = nexttilesheet;

                if (nexttilesheet == 0)
                    ed.note = "Now using default tilesheet";
                else
                    ed.note = "Now using tiles" + std::to_string(nexttilesheet) + ".png";
                ed.notedelay = 45;
                ed.updatetiles = true;
                ed.keydelay = 6;
                graphics.backgrounddrawn = false;
            }
            if(key.keymap[SDLK_F10])
            {
                if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode==1)
                {
                    ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode=0;
                    ed.note="Direct Mode Disabled";
                }
                else
                {
                    ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode=1;
                    ed.note="Direct Mode Enabled";
                }
                graphics.backgrounddrawn=false;

                ed.notedelay=45;
                ed.updatetiles=true;
                ed.keydelay=6;
            }
            if(key.keymap[SDLK_1]) ed.drawmode=0;
            if(key.keymap[SDLK_2]) ed.drawmode=1;
            if(key.keymap[SDLK_3]) ed.drawmode=2;
            if(key.keymap[SDLK_4]) ed.drawmode=3;
            if(key.keymap[SDLK_5]) ed.drawmode=4;
            if(key.keymap[SDLK_6]) ed.drawmode=5;
            if(key.keymap[SDLK_7]) ed.drawmode=6;
            if(key.keymap[SDLK_8]) ed.drawmode=7;
            if(key.keymap[SDLK_9]) ed.drawmode=8;
            if(key.keymap[SDLK_0]) ed.drawmode=9;
            if(key.keymap[SDLK_r]) ed.drawmode=10;
            if(key.keymap[SDLK_t]) ed.drawmode=11;
            if(key.keymap[SDLK_y]) ed.drawmode=12;
            if(key.keymap[SDLK_u]) ed.drawmode=13;
            if(key.keymap[SDLK_i]) ed.drawmode=14;
            if(key.keymap[SDLK_o]) ed.drawmode=15;
            if(key.keymap[SDLK_p]) ed.drawmode=16;

            if(key.keymap[SDLK_w])
            {
                int j=0, tx=0, ty=0;
                for(size_t i=0; i<edentity.size(); i++)
                {
                    if(edentity[i].t==50)
                    {
                        tx=(edentity[i].p1-(edentity[i].p1%40))/40;
                        ty=(edentity[i].p2-(edentity[i].p2%30))/30;
                        if(tx==ed.levx && ty==ed.levy &&
                           edentity[i].state==ed.levaltstate &&
                           edentity[i].intower==tower)
                        {
                            j++;
                        }
                    }
                }
                if(j>0)
                {
                    ed.note="ERROR: Cannot have both warp types";
                    ed.notedelay=45;
                }
                else
                {
                    ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir=(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir+1)%4;
                    if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==0)
                    {
                        ed.note="Room warping disabled";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==1)
                    {
                        ed.note="Room warps horizontally";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==2)
                    {
                        ed.note="Room warps vertically";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                    else if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].warpdir==3)
                    {
                        ed.note="Room warps in all directions";
                        ed.notedelay=45;
                        graphics.backgrounddrawn=false;
                    }
                }
                ed.keydelay=6;
            }
            if (key.keymap[SDLK_e]) {
                ed.keydelay = 6;
                ed.getlin(TEXT_ROOMNAME, "Enter new room name:",
                          &(ed.level[ed.levx+(ed.levy*ed.maxwidth)].roomname));
                game.mapheld=true;
            }
            if (key.keymap[SDLK_g]) {
                ed.keydelay = 6;
                ed.getlin(TEXT_GOTOROOM, "Enter room coordinates (x,y):",
                          NULL);
                game.mapheld=true;
            }
            if (key.keymap[SDLK_a]) {
                if (ed.getedaltstatenum(ed.levx, ed.levy, ed.levaltstate + 1) != -1) {
                    ed.levaltstate++;
                    ed.note = "Switched to alt state " + help.String(ed.levaltstate);
                } else if (ed.levaltstate == 0) {
                    ed.note = "No alt states in this room";
                } else {
                    ed.levaltstate = 0;
                    ed.note = "Switched to main state";
                }
                ed.notedelay = 45;
                ed.keydelay = 6;
            }

            //Save and load
            if(key.keymap[SDLK_s])
            {
                ed.keydelay = 6;
                ed.getlin(TEXT_SAVE, "Enter map filename to save map as:",
                          &(ed.filename));
                game.mapheld=true;
                graphics.backgrounddrawn=false;
            }

            if(key.keymap[SDLK_l])
            {
                ed.keydelay = 6;
                ed.getlin(TEXT_LOAD, "Enter map filename to load:",
                          &(ed.filename));
                game.mapheld=true;
                graphics.backgrounddrawn=false;
            }

            if(!game.press_map) game.mapheld=false;
            if (!game.mapheld)
            {
                if(game.press_map)
                {
                    game.mapheld=true;

                    //Ok! Scan the room for the closest checkpoint
                    int testeditor = -1;
                    int startpoint = 0;
                    int ex = 0;
                    int ey = 0;
                    int enttyp = 16;
                    do {
                        for (size_t i=0; i<edentity.size(); i++) {
                            if (edentity[i].t != enttyp ||
                                edentity[i].intower != tower ||
                                edentity[i].state != ed.levaltstate)
                                continue;

                            ex = edentity[i].x;
                            ey = edentity[i].y;
                            if (!tower) {
                                ex -= ed.levx * 40;
                                ey -= ed.levy * 30;
                            } else
                                ey -= ed.ypos;

                            ex *= 8;
                            ey *= 8;
                            ex += edentity[i].subx;
                            ey += edentity[i].suby;

                            // Even in towers, only allow spawn within screen
                            if (ex >= 0 && ex < 320 && ey >= 0 && ey < 240) {
                                testeditor = i;
                                startpoint = enttyp;
                                break;
                            }
                        }

                        enttyp -= 6;
                    } while (enttyp >= 10 && !startpoint);

                    if (testeditor==-1) {
                        ed.note="ERROR: No checkpoint to spawn at";
                        ed.notedelay=45;
                    } else {
                        ed.currentghosts = 0;
                        game.edsavex = ex;
                        game.edsavey = ey;
                        game.edsaverx = 100 + ed.levx;
                        game.edsavery = 100 + ed.levy;
                        game.edsavey--;
                        if (startpoint == 10) {
                            game.edsavegc = edentity[testeditor].p1;
                            game.edsavedir = 0;
                            if (game.edsavegc != 0)
                                game.edsavey -= 7;
                        } else {
                            game.edsavex -= 4;
                            game.edsavegc = 0;
                            game.edsavedir = 1 - edentity[testeditor].p1;
                        }

                        if (tower)
                            game.edsavey += ed.ypos * 8;

                        music.haltdasmusik();
                        graphics.backgrounddrawn=false;
                        ed.returneditoralpha = 1000; // Let's start it higher than 255 since it gets clamped
                        ed.oldreturneditoralpha = 1000;
                        script.startgamemode(21);
                    }
                }
            }
            if(key.keymap[SDLK_h])
            {
                ed.hmod=true;
            }
            else
            {
                ed.hmod=false;
            }

            if(key.keymap[SDLK_b])
            {
                ed.bmod=true;
            }
            else
            {
                ed.bmod=false;
            }
            if(key.keymap[SDLK_v] || key.controllerWantsRUp())
            {
                ed.vmod=true;
            }
            else
            {
                ed.vmod=false;
            }

            if(key.keymap[SDLK_c] || key.controllerWantsRDown())
            {
                ed.cmod=true;
            }
            else
            {
                ed.cmod=false;
            }

            if(key.keymap[SDLK_x] || key.controllerWantsRRight(false))
            {
                ed.xmod=true;
            }
            else
            {
                ed.xmod=false;
            }


            if(key.keymap[SDLK_z] || key.controllerWantsRLeft(false))
            {
                ed.zmod=true;
            }
            else
            {
                ed.zmod=false;
            }

            ed.switchroomsinput();

            if(key.keymap[SDLK_SPACE]) {
                ed.spacemod = !ed.spacemod;
                ed.keydelay=6;
            }
        }

        if(!ed.settingsmod && !ed.trialstartpoint)
        {
            if(ed.boundarymod>0)
            {
                if(key.leftbutton)
                {
                    if(ed.lclickdelay==0)
                    {
                        if(ed.boundarymod==1)
                        {
                            ed.lclickdelay=1;
                            ed.boundx1=(ed.tilex*8);
                            ed.boundy1=(ed.tiley*8);
                            ed.boundarymod=2;
                            if (ed.boundarytype == 4) {
                                int tmp=ed.levx+(ed.levy*ed.maxwidth);
                                ed.level[tmp].tower_row=ed.tiley + ed.ypos;
                                ed.boundarymod = 0;
                            }
                        }
                        else if(ed.boundarymod==2)
                        {
                            if((ed.tilex*8)+8>=ed.boundx1 || (ed.tiley*8)+8>=ed.boundy1)
                            {
                                ed.boundx2=(ed.tilex*8)+8;
                                ed.boundy2=(ed.tiley*8)+8;
                            }
                            else
                            {
                                ed.boundx2=ed.boundx1+8;
                                ed.boundy2=ed.boundy1+8;
                            }

                            if (!tower) {
                                ed.boundx1 += (ed.levx * 320);
                                ed.boundx2 += (ed.levx * 320);
                                ed.boundy1 += (ed.levy * 240);
                                ed.boundy2 += (ed.levy * 240);
                            } else {
                                ed.boundy1 += (ed.ypos * 8);
                                ed.boundy2 += (ed.ypos * 8);
                            }
                            if(ed.boundarytype==0 || ed.boundarytype==5)
                            {
                                //Script trigger
                                ed.lclickdelay=1;
                                ed.textent=edentity.size();
                                addedentity((ed.boundx1/8),(ed.boundy1/8),19,
                                            (ed.boundx2-ed.boundx1)/8, (ed.boundy2-ed.boundy1)/8);
                                ed.getlin(TEXT_SCRIPT, "Enter script name:",
                                          &(edentity[ed.textent].scriptname));
                                if (ed.boundarytype==5)
                                    // Don't forget to subtract 1 from index because we added an entity
                                    edentity[edentity.size()-1].onetime = true;
                            }
                            else if(ed.boundarytype==4)
                            {
                                //Activity zone
                                ed.lclickdelay=1;
                                ed.textent=edentity.size();
                                ed.textcount = 2;
                                addedentity((ed.boundx1/8),(ed.boundy1/8),20,
                                            (ed.boundx2-ed.boundx1)/8, (ed.boundy2-ed.boundy1)/8);
                                ed.getlin(TEXT_ACTIVITYZONE,
                                          "Enter activity zone text:",
                                          &(edentity[ed.textent].activityname));
                            }
                            else if(ed.boundarytype==1)
                            {
                                //Enemy bounds
                                int tmp=ed.levx+(ed.levy*ed.maxwidth);
                                ed.level[tmp].enemyx1=ed.boundx1;
                                ed.level[tmp].enemyy1=ed.boundy1;
                                ed.level[tmp].enemyx2=ed.boundx2;
                                ed.level[tmp].enemyy2=ed.boundy2;
                            }
                            else if(ed.boundarytype==2)
                            {
                                //Platform bounds
                                int tmp=ed.levx+(ed.levy*ed.maxwidth);
                                ed.level[tmp].platx1=ed.boundx1;
                                ed.level[tmp].platy1=ed.boundy1;
                                ed.level[tmp].platx2=ed.boundx2;
                                ed.level[tmp].platy2=ed.boundy2;
                            }
                            else if(ed.boundarytype==3)
                            {
                                //Copy
                            }
                            ed.boundarymod=0;
                            ed.lclickdelay=1;
                        }
                    }
                }
                else
                {
                    ed.lclickdelay=0;
                }
                if(key.rightbutton)
                {
                    ed.boundarymod=0;
                }
            }
            else if(ed.warpmod)
            {
                //Placing warp token
                if(key.leftbutton)
                {
                    if(ed.lclickdelay==0)
                    {
                        if(ed.free(ed.tilex, ed.tiley)==0)
                        {
                            int tower = ed.get_tower(ed.levx, ed.levy);
                            int ex = ed.tilex+(ed.levx*40);
                            int ey = ed.tiley+(ed.levy*30);
                            if (tower) {
                                ex = ed.tilex;
                                ey = ed.tiley + ed.ypos;
                            }
                            edentity[ed.warpent].p1=ex;
                            edentity[ed.warpent].p2=ey;
                            edentity[ed.warpent].p3=tower;
                            ed.warpmod=false;
                            ed.warpent=-1;
                            ed.lclickdelay=1;
                        }
                    }
                }
                else
                {
                    ed.lclickdelay=0;
                }
                if(key.rightbutton)
                {
                    removeedentity(ed.warpent);
                    ed.warpmod=false;
                    ed.warpent=-1;
                }
            } else {
                int tx = ed.tilex;
                int ty = ed.tiley;
                if (!tower) {
                    tx += (ed.levx * 40);
                    ty += (ed.levy * 30);
                } else
                    ty += ed.ypos;

                //Mouse input
                if(key.leftbutton)
                {
                    if(ed.lclickdelay==0)
                    {
                        //Depending on current mode, place something
                        if(ed.drawmode==0)
                        {
                            //place tiles
                            //Are we in direct mode?
                            if(ed.level[ed.levx+(ed.levy*ed.maxwidth)].directmode>=1)
                            {
                                if(ed.bmod)
                                {
                                    for(int i=0; i<30; i++)
                                    {
                                        ed.placetilelocal(ed.tilex, i, ed.dmtile);
                                    }
                                }
                                else if(ed.hmod)
                                {
                                    for(int i=0; i<40; i++)
                                    {
                                        ed.placetilelocal(i, ed.tiley, ed.dmtile);
                                    }
                                }
                                else if(ed.vmod)
                                {
                                    for(int j=-4; j<5; j++)
                                    {
                                        for(int i=-4; i<5; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, ed.dmtile);
                                        }
                                    }
                                }
                                else if(ed.cmod)
                                {
                                    for(int j=-3; j<4; j++)
                                    {
                                        for(int i=-3; i<4; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, ed.dmtile);
                                        }
                                    }
                                }
                                else if(ed.xmod)
                                {
                                    for(int j=-2; j<3; j++)
                                    {
                                        for(int i=-2; i<3; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, ed.dmtile);
                                        }
                                    }
                                }
                                else if(ed.zmod)
                                {
                                    for(int j=-1; j<2; j++)
                                    {
                                        for(int i=-1; i<2; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, ed.dmtile);
                                        }
                                    }
                                }
                                else
                                {
                                    ed.placetilelocal(ed.tilex, ed.tiley, ed.dmtile);
                                }
                            }
                            else
                            {
                                if(ed.bmod)
                                {
                                    for(int i=0; i<30; i++)
                                    {
                                        ed.placetilelocal(ed.tilex, i, 80);
                                    }
                                }
                                else if(ed.hmod)
                                {
                                    for(int i=0; i<40; i++)
                                    {
                                        ed.placetilelocal(i, ed.tiley, 80);
                                    }
                                }
                                else if(ed.vmod)
                                {
                                    for(int j=-4; j<5; j++)
                                    {
                                        for(int i=-4; i<5; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, 80);
                                        }
                                    }
                                }
                                else if(ed.cmod)
                                {
                                    for(int j=-3; j<4; j++)
                                    {
                                        for(int i=-3; i<4; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, 80);
                                        }
                                    }
                                }
                                else if(ed.xmod)
                                {
                                    for(int j=-2; j<3; j++)
                                    {
                                        for(int i=-2; i<3; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, 80);
                                        }
                                    }
                                }
                                else if(ed.zmod)
                                {
                                    for(int j=-1; j<2; j++)
                                    {
                                        for(int i=-1; i<2; i++)
                                        {
                                            ed.placetilelocal(ed.tilex+i, ed.tiley+j, 80);
                                        }
                                    }
                                }
                                else
                                {
                                    ed.placetilelocal(ed.tilex, ed.tiley, 80);
                                }
                            }
                        }
                        else if(ed.drawmode==1)
                        {
                            //place background tiles
                            if(ed.bmod)
                            {
                                for(int i=0; i<30; i++)
                                {
                                    ed.placetilelocal(ed.tilex, i, 2);
                                }
                            }
                            else if(ed.hmod)
                            {
                                for(int i=0; i<40; i++)
                                {
                                    ed.placetilelocal(i, ed.tiley, 2);
                                }
                            }
                            else if(ed.vmod)
                            {
                                for(int j=-4; j<5; j++)
                                {
                                    for(int i=-4; i<5; i++)
                                    {
                                        ed.placetilelocal(ed.tilex+i, ed.tiley+j, 2);
                                    }
                                }
                            }
                            else if(ed.cmod)
                            {
                                for(int j=-3; j<4; j++)
                                {
                                    for(int i=-3; i<4; i++)
                                    {
                                        ed.placetilelocal(ed.tilex+i, ed.tiley+j, 2);
                                    }
                                }
                            }
                            else if(ed.xmod)
                            {
                                for(int j=-2; j<3; j++)
                                {
                                    for(int i=-2; i<3; i++)
                                    {
                                        ed.placetilelocal(ed.tilex+i, ed.tiley+j, 2);
                                    }
                                }
                            }
                            else if(ed.zmod)
                            {
                                for(int j=-1; j<2; j++)
                                {
                                    for(int i=-1; i<2; i++)
                                    {
                                        ed.placetilelocal(ed.tilex+i, ed.tiley+j, 2);
                                    }
                                }
                            }
                            else
                            {
                                ed.placetilelocal(ed.tilex, ed.tiley, 2);
                            }
                        }
                        else if(ed.drawmode==2)
                        {
                            //place spikes
                            ed.placetilelocal(ed.tilex, ed.tiley, 8);
                        }

                        int tmp=edentat(tx, ty, ed.levaltstate, tower);
                        if(tmp==-1)
                        {
                            //Room text and script triggers can be placed in walls
                            if(ed.drawmode==10)
                            {
                                ed.lclickdelay=1;
                                ed.textent=edentity.size();
                                addedentity(tx, ty, 17);
                                ed.getlin(TEXT_ROOMTEXT, "Enter roomtext:",
                                          &(edentity[ed.textent].scriptname));
                                graphics.backgrounddrawn=false;
                            }
                            else if(ed.drawmode==12)   //Script Trigger
                            {
                                if (ed.zmod) {
                                    ed.boundarytype=4;
                                } else if (ed.xmod) {
                                    ed.boundarytype=5;
                                } else {
                                    ed.boundarytype=0;
                                }
                                ed.boundx1=ed.tilex*8;
                                ed.boundy1=ed.tiley*8;
                                ed.boundarymod=2;
                                ed.lclickdelay=1;
                            }
                        }
                        if(tmp==-1 && ed.free(ed.tilex,ed.tiley)==0)
                        {
                            if(ed.drawmode==3)
                            {
                                if(ed.numtrinkets()<100)
                                {
                                    addedentity(tx, ty, 9);
                                    ed.lclickdelay=1;
                                }
                                else
                                {
                                    ed.note="ERROR: Max number of trinkets is 100";
                                    ed.notedelay=45;
                                }
                            }
                            else if(ed.drawmode==4)
                            {
                                addedentity(tx, ty, 10, 1);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==5)
                            {
                                addedentity(tx, ty, 3);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==6)
                            {
                                addedentity(tx, ty, 2, 5);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==7)
                            {
                                addedentity(tx, ty, 2, 0, ed.entspeed);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==8) // Enemies
                            {
                                addedentity(tx, ty, 1, 0, ed.entspeed);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==9)
                            {
                                addedentity(tx, ty, 11, 0);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==11)
                            {
                                ed.lclickdelay=1;
                                ed.textent=edentity.size();
                                addedentity(tx, ty, 18, 0);
                                ed.getlin(TEXT_SCRIPT, "Enter script name",
                                          &(edentity[ed.textent].scriptname));
                            }
                            else if(ed.drawmode==13)
                            {
                                ed.warpmod=true;
                                ed.warpent=edentity.size();
                                addedentity(tx, ty, 13);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==14)
                            {
                                //Warp lines
                                if(ed.level[ed.levx+(ed.maxwidth*ed.levy)].warpdir==0)
                                {
                                    if (ed.tilex == 0)
                                        addedentity(tx, ty, 50, 0);
                                    else if (ed.tilex == 39)
                                        addedentity(tx, ty, 50, 1);
                                    else if (!tower && ed.tiley == 0)
                                        addedentity(tx, ty, 50, 2);
                                    else if (!tower && ed.tiley == 29)
                                        addedentity(tx, ty, 50, 3);
                                    else if (tower) {
                                        ed.note = "ERROR: Warp lines must be on vertical edges";
                                        ed.notedelay=45;
                                    } else {
                                        ed.note="ERROR: Warp lines must be on edges";
                                        ed.notedelay=45;
                                    }
                                } else {
                                    ed.note="ERROR: Cannot have both warp types";
                                    ed.notedelay=45;
                                }
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==15)  //Crewmate
                            {
                                if(ed.numcrewmates()<100)
                                {
                                    addedentity(tx, ty, 15,
                                                int(fRandom() * 6));
                                    ed.lclickdelay=1;
                                } else {
                                    ed.note="ERROR: Max number of crewmates is 100";
                                    ed.notedelay=45;
                                }
                            }
                            else if(ed.drawmode==16)  //Start Point
                            {
                                if (ed.levaltstate != 0) {
                                    ed.note = "ERROR: Start point must be in main state";
                                    ed.notedelay = 45;
                                } else {
                                    //If there is another start point, destroy it
                                    for(size_t i=0; i<edentity.size(); i++)
                                    {
                                        if(edentity[i].t==16)
                                        {
                                            removeedentity(i);
                                            i--;
                                        }
                                    }
                                    addedentity(tx, ty, 16, 0);
                                }
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==17)  // Flip Tokens
                            {
                                addedentity(tx, ty, 5, 181);
                                ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==18)  // Coins
                            {
                                if (ed.zmod)      addedentity(tx, ty, 8, 1);
                                else if (ed.xmod) addedentity(tx, ty, 8, 2);
                                else if (ed.cmod) addedentity(tx, ty, 8, 3);
                                else if (ed.vmod) addedentity(tx, ty, 8, 4);
                                else              addedentity(tx, ty, 8, 0);
                                //ed.lclickdelay=1;
                            }
                            else if(ed.drawmode==19)  // Teleporter
                            {
                                addedentity(tx, ty, 14);
                                ed.lclickdelay=1;
                                map.remteleporter(ed.levx, ed.levy);
                                map.setteleporter(ed.levx, ed.levy);
                            }
                            else if(ed.drawmode==-6)  // ??????
                            {
                                addedentity(tx, ty, 999);
                                ed.lclickdelay=1;
                            }
                        }
                        else if(tmp == -1)
                        {
                            //Important! Do nothing, or else Undefined Behavior will happen
                        }
                        else if(edentity[tmp].t==1)
                        {
                            edentity[tmp].p1=(edentity[tmp].p1+1)%4;
                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==2)
                        {
                            if(edentity[tmp].p1>=5)
                            {
                                edentity[tmp].p1=(edentity[tmp].p1+1)%9;
                                if(edentity[tmp].p1<5) edentity[tmp].p1=5;
                            }
                            else
                            {
                                edentity[tmp].p1=(edentity[tmp].p1+1)%4;
                            }
                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==10) //Checkpoint sprite changing
                        {
                                 if (edentity[tmp].p1 == 0)
                                     edentity[tmp].p1 = 2;
                            else if (edentity[tmp].p1 == 2)
                                     edentity[tmp].p1 = 3;
                            else if (edentity[tmp].p1 == 3)
                                     edentity[tmp].p1 = 1;
                            else if (edentity[tmp].p1 == 1)
                                     edentity[tmp].p1 = 0;

                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==11)
                        {
                            edentity[tmp].p1=(edentity[tmp].p1+1)%2;
                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==15)
                        {
                            edentity[tmp].p1=(edentity[tmp].p1+1)%6;
                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==16)
                        {
                            edentity[tmp].p1=(edentity[tmp].p1+1)%2;
                            ed.lclickdelay=1;
                        }
                        else if(edentity[tmp].t==17)
                        {
                            ed.lclickdelay=1;
                            ed.getlin(TEXT_ROOMTEXT, "Enter roomtext:",
                                      &(edentity[tmp].scriptname));
                            ed.textent=tmp;
                        }
                        else if(edentity[tmp].t==18 || edentity[tmp].t==19)
                        {
                            ed.lclickdelay=1;
                            ed.getlin(TEXT_ROOMTEXT, "Enter script name:",
                                      &(edentity[ed.textent].scriptname));
                            ed.textent=tmp;

                            // A bit meh that the easiest way is doing this at the same time you start changing the script name, but oh well
                            if (edentity[tmp].p1 == 0) // Currently not flipped
                                edentity[tmp].p1 = 1; // Flip it, then
                            else if (edentity[tmp].p1 == 1) // Currently is flipped
                                edentity[tmp].p1 = 0; // Unflip it, then
                        }
                    }
                }
                else
                {
                    ed.lclickdelay=0;
                }

                if(key.rightbutton)
                {
                    //place tiles
                    if(ed.bmod)
                    {
                        for(int i=0; i<30; i++)
                        {
                            ed.placetilelocal(ed.tilex, i, 0);
                        }
                    }
                    else if(ed.hmod)
                    {
                        for(int i=0; i<40; i++)
                        {
                            ed.placetilelocal(i, ed.tiley, 0);
                        }
                    }
                    else if(ed.vmod)
                    {
                        for(int j=-4; j<5; j++)
                        {
                            for(int i=-4; i<5; i++)
                            {
                                ed.placetilelocal(ed.tilex+i, ed.tiley+j, 0);
                            }
                        }
                    }
                    else if(ed.cmod)
                    {
                        for(int j=-3; j<4; j++)
                        {
                            for(int i=-3; i<4; i++)
                            {
                                ed.placetilelocal(ed.tilex+i, ed.tiley+j, 0);
                            }
                        }
                    }
                    else if(ed.xmod)
                    {
                        for(int j=-2; j<3; j++)
                        {
                            for(int i=-2; i<3; i++)
                            {
                                ed.placetilelocal(ed.tilex+i, ed.tiley+j, 0);
                            }
                        }
                    }
                    else if(ed.zmod)
                    {
                        for(int j=-1; j<2; j++)
                        {
                            for(int i=-1; i<2; i++)
                            {
                                ed.placetilelocal(ed.tilex+i, ed.tiley+j, 0);
                            }
                        }
                    }
                    else
                    {
                        ed.placetilelocal(ed.tilex, ed.tiley, 0);
                    }
                    for(size_t i=0; i<edentity.size(); i++)
                    {
                        if (edentity[i].x==tx && edentity[i].y==ty &&
                            edentity[i].state==ed.levaltstate &&
                            edentity[i].intower==tower) {
                            if (edentity[i].t==14) {
                                map.remteleporter(ed.levx, ed.levy);
                            }
                            removeedentity(i);
                        }
                    }
                }

                if(key.middlebutton)
                {
                    ed.dmtile=ed.gettilelocal(ed.tilex, ed.tiley);
                }
            }
        }
    }

    if(ed.updatetiles && ed.level[ed.levx + (ed.levy*ed.maxwidth)].directmode==0)
    {
        ed.updatetiles=false;
        //Correctly set the tiles in the current room
        switch(ed.level[ed.levx + (ed.levy*ed.maxwidth)].tileset)
        {
        case 0: //The Space Station
            for(int j=0; j<30; j++)
            {
                for(int i=0; i<40; i++)
                {
                    if(ed.gettilelocal(i, j)>=3 && ed.gettilelocal(i, j)<80)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.spikedir(i,j));
                    }
                    else if(ed.gettilelocal(i, j)==2 || ed.gettilelocal(i, j)>=680)
                    {
                        //Fix background
                        ed.settilelocal(i, j, ed.backedgetile(i,j)+ed.backbase(ed.levx,ed.levy));
                    }
                    else if(ed.gettilelocal(i, j)>0)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.edgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 1: //Outside
            for(int j=0; j<30; j++)
            {
                for(int i=0; i<40; i++)
                {
                    if(ed.gettilelocal(i, j)>=3 && ed.gettilelocal(i, j)<80)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.spikedir(i,j));
                    }
                    else if(ed.gettilelocal(i, j)==2 || ed.gettilelocal(i, j)>=680)
                    {
                        //Fix background
                        ed.settilelocal(i, j, ed.outsideedgetile(i,j)+ed.backbase(ed.levx,ed.levy));
                    }
                    else if(ed.gettilelocal(i, j)>0)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.edgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 2: //Lab
            for(int j=0; j<30; j++)
            {
                for(int i=0; i<40; i++)
                {
                    if(ed.gettilelocal(i, j)>=3 && ed.gettilelocal(i, j)<80)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.labspikedir(i,j, ed.level[ed.levx + (ed.maxwidth*ed.levy)].tilecol));
                    }
                    else if(ed.gettilelocal(i, j)==2 || ed.gettilelocal(i, j)>=680)
                    {
                        //Fix background
                        ed.settilelocal(i, j, 713);
                    }
                    else if(ed.gettilelocal(i, j)>0)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.edgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 3: //Warp Zone/Intermission
            for(int j=0; j<30; j++)
            {
                for(int i=0; i<40; i++)
                {
                    if(ed.gettilelocal(i, j)>=3 && ed.gettilelocal(i, j)<80)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.spikedir(i,j));
                    }
                    else if(ed.gettilelocal(i, j)==2 || ed.gettilelocal(i, j)>=680)
                    {
                        //Fix background
                        ed.settilelocal(i, j, 713);
                    }
                    else if(ed.gettilelocal(i, j)>0)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.edgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 4: //The ship
            for(int j=0; j<30; j++)
            {
                for(int i=0; i<40; i++)
                {
                    if(ed.gettilelocal(i, j)>=3 && ed.gettilelocal(i, j)<80)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.spikedir(i,j));
                    }
                    else if(ed.gettilelocal(i, j)==2 || ed.gettilelocal(i, j)>=680)
                    {
                        //Fix background
                        ed.settilelocal(i, j, ed.backedgetile(i,j)+ed.backbase(ed.levx,ed.levy));
                    }
                    else if(ed.gettilelocal(i, j)>0)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.edgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 5: //The Tower
            for(int j=0; j<30; j++) {
                if (ed.intower() &&
                    ((j + ed.ypos) < 0 ||
                     (j + ed.ypos) >= ed.tower_size(ed.get_tower(ed.levx,
                                                                 ed.levy))))
                    continue;

                for(int i=0; i<40; i++)
                {
                    if(ed.gettiletyplocal(i, j) == TILE_SPIKE)
                    {
                        //Fix spikes
                        ed.settilelocal(i, j, ed.towerspikedir(i,j)+ed.spikebase(ed.levx,ed.levy));
                    }
                    else if(ed.gettiletyplocal(i, j) == TILE_BACKGROUND)
                    {
                        //Fix background
                        ed.settilelocal(i, j, ed.backbase(ed.levx,ed.levy));
                    }
                    else if(ed.gettiletyplocal(i, j) == TILE_FOREGROUND)
                    {
                        //Fix tiles
                        ed.settilelocal(i, j, ed.toweredgetile(i,j)+ed.base(ed.levx,ed.levy));
                    }
                }
            }
            break;
        case 6: //Custom Set 1
            break;
        case 7: //Custom Set 2
            break;
        case 8: //Custom Set 3
            break;
        case 9: //Custom Set 4
            break;
        }
    }
}
#endif /* NO_EDITOR */

int editorclass::getedaltstatenum(int rxi, int ryi, int state)
{
    for (size_t i = 0; i < altstates.size(); i++)
        if (altstates[i].x == rxi && altstates[i].y == ryi && altstates[i].state == state)
            return i;

    return -1;
}

void editorclass::addaltstate(int rxi, int ryi, int state)
{
    for (size_t i = 0; i < altstates.size(); i++)
        if (altstates[i].x == -1 || altstates[i].y == -1) {
            altstates[i].x = rxi;
            altstates[i].y = ryi;
            altstates[i].state = state;

            // Copy the tiles from the main state
            for (int ty = 0; ty < 30; ty++)
                for (int tx = 0; tx < 40; tx++)
                    altstates[i].tiles[tx + ty*40] = contents[tx + rxi*40 + vmult[ty + ryi*30]];

            // Copy the entities from the main state
            // But since we're incrementing edentity.size(), don't use it as a bounds check!
            int limit = edentity.size();
            for (int i = 0; i < limit; i++)
                if (edentity[i].x >= rxi*40 && edentity[i].x < (rxi+1)*40
                && edentity[i].y >= ryi*30 && edentity[i].y < (ryi+1)*30
                && edentity[i].state == 0 && edentity[i].intower == 0) {
                    if (edentity[i].t == 9) {
                        // TODO: If removing the 100 trinkets limit, update this
                        if (numtrinkets() >= 100)
                            continue;
                    } else if (edentity[i].t == 15) {
                        // TODO: If removing the 100 crewmates limit, update this
                        if (numcrewmates() >= 100)
                            continue;
                    } else if (edentity[i].t == 16) {
                        // Don't copy the start point
                        continue;
                    }
                    edentities newentity = edentity[i];
                    newentity.state = state;
                    edentity.push_back(newentity);
                }

            break;
        }
}

void editorclass::removealtstate(int rxi, int ryi, int state)
{
    int n = getedaltstatenum(rxi, ryi, state);
    if (n == -1) {
        printf("Trying to remove nonexistent altstate (%i,%i)@%i!\n", rxi, ryi, state);
        return;
    }

    altstates[n].x = -1;
    altstates[n].y = -1;

    for (size_t i = 0; i < edentity.size(); i++)
        if (edentity[i].x >= rxi*40 && edentity[i].x < (rxi+1)*40
        && edentity[i].y >= ryi*30 && edentity[i].y < (ryi+1)*30
        && edentity[i].state == state && edentity[i].intower == 0) {
            removeedentity(i);
        }

    // Ok, now update the rest
    // This looks like it's O(n^2), and, well, it probably is lmao
    int dothisstate = state;
    while (true) {
        dothisstate++;
        int nextstate = getedaltstatenum(rxi, ryi, dothisstate);
        if (nextstate == -1)
            break;
        altstates[nextstate].state--;
    }

    // Don't forget to update entities
    for (size_t i = 0; i < edentity.size(); i++)
        if (edentity[i].x >= rxi*40 && edentity[i].x < (rxi+1)*40
        && edentity[i].y >= ryi*30 && edentity[i].y < (ryi+1)*30
        && edentity[i].state > state && edentity[i].intower == 0)
            edentity[i].state--;
}

int editorclass::getnumaltstates(int rxi, int ryi)
{
    int num = 0;
    for (size_t i = 0; i < altstates.size(); i++)
        if (altstates[i].x == rxi && altstates[i].y == ryi)
            num++;
    return num;
}

// Pass in the `tilecol` of a room to this function
int editorclass::gettowerplattile(int col)
{
    // Re-use the Warp Zone textures from entcolours.png
    switch (col / 5) {
    case 0:
        // Red
        return 47+1;
    case 1:
        // Yellow
        return 47+4;
    case 2:
        // Green
        return 47+5;
    case 3:
        // Cyan
        return 47;
    case 4:
        // Purple
        return 47+3;
    case 5:
        // Pink
        return 47+2;
    }

    // Default to red
    return 47+1;
}

void editorclass::switchroomsinput()
{
    if (key.keymap[SDLK_UP] || key.keymap[SDLK_DOWN] ||
        key.keymap[SDLK_LEFT] || key.keymap[SDLK_RIGHT] ||
        key.keymap[SDLK_KP_8] || key.keymap[SDLK_KP_2] ||
        key.keymap[SDLK_KP_4] || key.keymap[SDLK_KP_6] ||
        key.isDown(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_UP) ||
        key.isDown(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        ed.keydelay = 6;
        if (key.keymap[SDLK_UP] || key.keymap[SDLK_KP_8] || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_UP))
            ed.levy--;
        else if (key.keymap[SDLK_DOWN] || key.keymap[SDLK_KP_2] || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            ed.levy++;
        else if (key.keymap[SDLK_LEFT] || key.keymap[SDLK_KP_4] || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            ed.levx--;
        else if (key.keymap[SDLK_RIGHT] || key.keymap[SDLK_KP_6] || key.isDown(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            ed.levx++;
        ed.updatetiles = true;
        ed.changeroom = true;
        graphics.backgrounddrawn=false;
        ed.levaltstate = 0;
        ed.levx = (ed.levx + ed.mapwidth) % ed.mapwidth;
        ed.levy = (ed.levy + ed.mapheight) % ed.mapheight;
    }
}

// Return a graphics-ready color based off of the given tileset and tilecol
// Much kudos to Dav999 for saving me a lot of work, because I stole these colors from const.lua in Ved! -Info Teddy
Uint32 editorclass::getonewaycol(int rx, int ry)
{
    int roomnum = rx + ry*maxwidth;
    edlevelclass& room = level[roomnum];
    switch (room.tileset) {

    case 0: // Space Station
        switch (room.tilecol) {
        case -1:
            return graphics.getRGB(109, 109, 109);
        case 0:
            return graphics.getRGB(131, 141, 235);
        case 1:
            return graphics.getRGB(227, 140, 227);
        case 2:
            return graphics.getRGB(242, 126, 151);
        case 3:
            return graphics.getRGB(229, 235, 133);
        case 4:
            return graphics.getRGB(148, 238, 130);
        case 5:
            return graphics.getRGB(140, 165, 227);
        case 6:
            return graphics.getRGB(227, 140, 148);
        case 7:
            return graphics.getRGB(140, 173, 228);
        case 8:
            return graphics.getRGB(142, 235, 137);
        case 9:
            return graphics.getRGB(137, 235, 206);
        case 10:
            return graphics.getRGB(235, 139, 223);
        case 11:
            return graphics.getRGB(238, 130, 138);
        case 12:
            return graphics.getRGB(137, 235, 178);
        case 13:
            return graphics.getRGB(125, 205, 247);
        case 14:
            return graphics.getRGB(190, 137, 235);
        case 15:
            return graphics.getRGB(235, 137, 206);
        case 16:
            return graphics.getRGB(229, 247, 127);
        case 17:
            return graphics.getRGB(127, 200, 247);
        case 18:
            return graphics.getRGB(197, 137, 235);
        case 19:
            return graphics.getRGB(235, 131, 175);
        case 20:
            return graphics.getRGB(242, 210, 123);
        case 21:
            return graphics.getRGB(131, 235, 158);
        case 22:
            return graphics.getRGB(242, 126, 151);
        case 23:
            return graphics.getRGB(219, 243, 123);
        case 24:
            return graphics.getRGB(131, 234, 145);
        case 25:
            return graphics.getRGB(131, 199, 234);
        case 26:
            return graphics.getRGB(141, 131, 234);
        case 27:
            return graphics.getRGB(226, 140, 144);
        case 28:
            return graphics.getRGB(129, 236, 144);
        case 29:
            return graphics.getRGB(235, 231, 131);
        case 30:
            return graphics.getRGB(153, 235, 131);
        case 31:
            return graphics.getRGB(207, 131, 235);
        }
        break;

    case 1: // Outside
        switch (room.tilecol) {
        case 0:
            return graphics.getRGB(57, 86, 140);
        case 1:
            return graphics.getRGB(156, 42, 42);
        case 2:
            return graphics.getRGB(42, 156, 155);
        case 3:
            return graphics.getRGB(125, 36, 162);
        case 4:
            return graphics.getRGB(191, 198, 0);
        case 5:
            return graphics.getRGB(0, 198, 126);
        case 6:
            return graphics.getRGB(224, 110, 177);
        case 7:
            return graphics.getRGB(255, 142, 87);
        }
        break;

    case 2: // Lab
        switch (room.tilecol) {
        case 0:
            return graphics.getRGB(0, 165, 206);
        case 1:
            return graphics.getRGB(206, 5, 0);
        case 2:
            return graphics.getRGB(222, 0, 173);
        case 3:
            return graphics.getRGB(27, 67, 255);
        case 4:
            return graphics.getRGB(194, 206, 0);
        case 5:
            return graphics.getRGB(0, 206, 39);
        case 6:
            return graphics.getRGB(0, 165, 206);
        }
        break;

    case 3: // Warp Zone
        switch (room.tilecol) {
        case 0:
            return graphics.getRGB(113, 178, 197);
        case 1:
            return graphics.getRGB(197, 113, 119);
        case 2:
            return graphics.getRGB(196, 113, 197);
        case 3:
            return graphics.getRGB(149, 113, 197);
        case 4:
            return graphics.getRGB(197, 182, 113);
        case 5:
            return graphics.getRGB(141, 197, 113);
        case 6:
            return graphics.getRGB(109, 109, 109);
        }
        break;

    case 4: // Ship
        switch (room.tilecol) {
        case 0:
            return graphics.getRGB(0, 206, 39);
        case 1:
            return graphics.getRGB(0, 165, 206);
        case 2:
            return graphics.getRGB(194, 206, 0);
        case 3:
            return graphics.getRGB(206, 0, 160);
        case 4:
            return graphics.getRGB(27, 67, 255);
        case 5:
            return graphics.getRGB(206, 5, 0);
        }
        break;

    // Towers WOULD be case 5, but tiles3.png doesn't have one-ways...
    // TODO: If tiles3.png ends up having one-ways, update this

    }

    // Uh, I guess return solid white
    return graphics.getRGB(255, 255, 255);
}

// This version detects the room automatically
Uint32 editorclass::getonewaycol()
{
    if (game.gamestate == EDITORMODE)
        return getonewaycol(ed.levx, ed.levy);
    else if (map.custommode)
        return getonewaycol(game.roomx - 100, game.roomy - 100);

    // Uh, I guess return solid white
    return graphics.getRGB(255, 255, 255);
}

// Return the number of the custom tilesheet to use for rendering
int editorclass::getcustomtiles(int rx, int ry)
{
    return level[rx + ry*maxwidth].customtileset;
}

int editorclass::getcustomsprites(int rx, int ry)
{
    return level[rx + ry*maxwidth].customspritesheet;
}

// This version detects the room automatically
int editorclass::getcustomtiles()
{
    if (game.gamestate == EDITORMODE)
        return getcustomtiles(ed.levx, ed.levy);
    else if (map.custommode)
        return getcustomtiles(game.roomx - 100, game.roomy - 100);

    return 0;
}

int editorclass::getcustomsprites()
{
    if (game.gamestate == EDITORMODE)
        return getcustomsprites(ed.levx, ed.levy);
    else if (map.custommode)
        return getcustomsprites(game.roomx - 100, game.roomy - 100);

    return 0;
}

int editorclass::numtrinkets()
{
    int temp = 0;
    for (size_t i = 0; i < edentity.size(); i++)
    {
        if (edentity[i].t == 9)
        {
            temp++;
        }
    }
    return temp;
}

int editorclass::numcrewmates()
{
    int temp = 0;
    for (size_t i = 0; i < edentity.size(); i++)
    {
        if (edentity[i].t == 15)
        {
            temp++;
        }
    }
    return temp;
}

int editorclass::numcoins()
{
    int temp = 0;
    for (size_t i = 0; i < edentity.size(); i++)
    {
        if (edentity[i].t == 8) {
            if (edentity[i].p1 == 0) temp++;
            else if (edentity[i].p1 == 1) temp += 10;
            else if (edentity[i].p1 == 2) temp += 20;
            else if (edentity[i].p1 == 3) temp += 50;
            else if (edentity[i].p1 == 4) temp += 100;
        }
    }
    return temp;
}

#endif /* NO_CUSTOM_LEVELS */
