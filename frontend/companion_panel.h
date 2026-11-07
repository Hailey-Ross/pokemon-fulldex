#pragma once
#include "ui.h"
#include "companion_rom.h"
#include "companion_types.h"
#include "companion_guides.h"
#include "companion_battle.h"

// A portable companion canvas. Android only displays pixels and reports coordinates;
// the same renderer can be used beside the game on other platforms.
class CompanionPanel
{
    int tab=0,mon=-1,page=0,species=1,guide=0;
    bool reveal=false,mapVisible=false;
    int lastPages=1;
    int pocket=-1,itemDetail=0;
    std::vector<int> itemRows;
    Companion::Profile bagProfile;
    std::vector<std::string> lines;
    static std::string Fit(std::string s,size_t n){return s.size()>n?s.substr(0,n-1)+".":s;}
    void Add(const std::string& text)
    {
        std::string line,word;
        auto flush=[&](){if(!word.empty()){if(line.size()+word.size()+1>48){lines.push_back(line);line.clear();}if(!line.empty())line+=' ';line+=word;word.clear();}};
        for(char c:text){if(c==' '||c=='\n'){flush();if(c=='\n'){lines.push_back(line);line.clear();}}else word+=c;}flush();if(!line.empty())lines.push_back(line);
    }
public:
    struct Viewport{int x,y,w,h;};
    static Viewport Overlay(int w,int h)
    {
        int width=std::min(960,w*3/5),height=width*3/4;
        if(height>h*3/4){height=h*3/4;width=height*4/3;}
        return {(w-width)/2,(h-height)/2,width,height};
    }
    static bool OverlayTouch(int w,int h,float x,float y,int& tx,int& ty)
    {
        auto view=Overlay(w,h);
        if(view.w<=0||view.h<=0||x<view.x||y<view.y||x>=view.x+view.w||y>=view.y+view.h)return false;
        tx=(int)((x-view.x)*640/view.w);ty=(int)((y-view.y)*480/view.h);return true;
    }
    const std::vector<std::string>& TextLines()const{return lines;}
    Canvas canvas;
    void ShowMap(){tab=3;mapVisible=true;page=0;}
    void Touch(int x,int y)
    {
        if(y>=30&&y<65){tab=std::clamp(x/106,0,5);mon=-1;itemDetail=0;page=0;reveal=false;mapVisible=false;return;}
        if(y>=415&&y<440){page=(page+1)%lastPages;return;}
        if(y>=440)
        {
            if(tab==3&&x>=210&&x<=430){mapVisible=!mapVisible;page=0;return;}
            if(tab==1&&itemDetail){if(x>=210&&x<=430){itemDetail=0;page=0;}else page=std::max(0,page+(x<210?-1:1));return;}
            if(tab==1){pocket=(x>=210&&x<=430)?-1:Companion::StepPocket(bagProfile,pocket,x<210?-1:1);page=0;return;}
            if(x<210){if(tab==2){species=std::max(1,species-1);page=0;}else if(tab==5){guide=std::max(0,guide-1);reveal=false;page=0;}else page=std::max(0,page-1);}
            else if(x>430){if(tab==2){species=std::min(649,species+1);page=0;}else if(tab==5){guide++;reveal=false;page=0;}else page++;}
            else{if(tab==0&&mon>=0)mon=-1;else if(tab==5){reveal=!reveal;page=0;}else page=0;}
            return;
        }
        if(tab==1&&!itemDetail&&y>=84&&y<415)
        {
            int row=page*16+(y-84)/21;
            if(row<(int)itemRows.size()&&itemRows[row]>0){itemDetail=itemRows[row];page=0;}
        }
        if(tab==0&&mon<0&&y>=84&&y<372){mon=(y-84)/48;page=0;}
    }
    const Canvas& Draw(const Companion::Snapshot& snapshot,const Companion::Metadata& data,
        const Companion::Profile& profile,const std::vector<Companion::GuideEntry>& entries)
    {
        canvas.Resize(640,480);canvas.Clear();canvas.Fill(0,0,640,480,0xFF101722);
        canvas.Text(14,9,"FULLDEX COMPANION",2,0xFFFFD75E);
        static const char* tabs[]={"PARTY","BAG","DEX","POSITION","BATTLE","GUIDE"};
        for(int i=0;i<6;i++){canvas.Fill(i*106,30,105,34,i==tab?Menu::Accent:0xFF263346);canvas.Text(i*106+8,41,tabs[i],1,0xFFFFFFFF);}
        lines.clear();itemRows.clear();
        if(!snapshot.ready){Add(snapshot.message);Add("YOUR NORMAL GAME AND MENUS REMAIN AVAILABLE.");}
        else if(tab==0)
        {
            if(mon<0)
            {
                for(size_t i=0;i<snapshot.party.size();i++)
                {
                    const auto& p=snapshot.party[i];int y=84+(int)i*48;
                    canvas.Text(14,y,Fit(data.Species(p.species),21),2,0xFFFFFFFF);
                    canvas.Text(300,y,"L"+std::to_string(p.level)+"  "+std::to_string(p.hp)+"/"+std::to_string(p.maxHp),2,0xFFD4DCE8);
                    canvas.Fill(14,y+20,570,7,0xFF34404E);canvas.Fill(14,y+20,p.maxHp?570*p.hp/p.maxHp:0,7,0xFF45C881);
                }
                if(snapshot.party.empty())Add("NO POKEMON IN THE PARTY YET.");
            }
            else if(mon<(int)snapshot.party.size())
            {
                const auto& p=snapshot.party[mon];Add(data.Species(p.species)+"  LEVEL "+std::to_string(p.level));
                Add("HP "+std::to_string(p.hp)+"/"+std::to_string(p.maxHp)+"  "+Companion::Status(p.status));
                Add("ITEM: "+data.ItemName(p.item));
                static const char* stats[]={"HP","ATTACK","DEFENCE","SPEED","SP ATK","SP DEF"};
                for(int i=0;i<6;i++)Add(std::string(stats[i])+" "+std::to_string(p.stats[i])+"  IV "+std::to_string(p.iv[i])+" EV "+std::to_string(p.ev[i]));
                for(int move:p.moves)if(move)Add(data.Move(move));
                Add("UPCOMING LEVEL-UP MOVES");
                for(const auto& entry:data.Upcoming(p))Add("L"+std::to_string(entry.level)+" "+data.Move(entry.move));
            }
            else{mon=-1;Add("PARTY SLOT NO LONGER PRESENT.");}
        }
        else if(tab==1)
        {
            bagProfile=profile;
            if(itemDetail)
            {
                Add(data.ItemName(itemDetail));Add(data.ItemDescription(itemDetail));
                int quantity=0;for(const auto& item:snapshot.items)if(item.id==itemDetail)quantity+=item.quantity;
                Add("IN BAG: "+std::to_string(quantity));
                for(const auto& p:snapshot.party)if(p.item==itemDetail)Add("HELD BY: "+data.Species(p.species));
                Add("VIEW ONLY. USE ITEMS THROUGH THE GAME.");
            }
            else
            {
                if(pocket>=(int)profile.pockets.size())pocket=-1;
                int stacks=0,total=0;
                for(const auto& item:snapshot.items)if(pocket<0||item.pocket==pocket){stacks++;total+=item.quantity;}
                Add(std::string(Companion::PocketName(profile,pocket))+" - "+std::to_string(stacks)+(stacks==1?" STACK / ":" STACKS / ")+std::to_string(total)+(total==1?" ITEM":" ITEMS"));
                Add("TAP AN ITEM FOR ITS DESCRIPTION.");
                itemRows.resize(lines.size(),-1);
                for(const auto& item:snapshot.items)if(pocket<0||item.pocket==pocket)
                {
                    Add(data.ItemName(item.id)+"  X"+std::to_string(item.quantity));
                    itemRows.resize(lines.size(),item.id);
                }
                if(!stacks)Add(pocket<0?"EMPTY BAG":"THIS POCKET IS EMPTY");
            }
        }
        else if(tab==2)
        {
            species=std::clamp(species,1,profile.maxSpecies);Add(std::to_string(species)+" "+data.Species(species));
            if(snapshot.dexReady&&species<(int)snapshot.caught.size()&&species<(int)snapshot.seen.size())Add(snapshot.caught[species]?"CAUGHT":snapshot.seen[species]?"SEEN":"NOT SEEN");
            int first=data.Type(species,0),second=data.Type(species,1);
            if(CompanionTypes::Valid(first))Add(std::string(CompanionTypes::Name(first))+(first!=second?std::string(" / ")+CompanionTypes::Name(second):""));
            if(species<(int)data.descriptions.size())Add(data.descriptions[species]);
            if(species<(int)data.personal.size()&&data.personal[species].size()>=6)
            {
                Add("BASE HP / ATK / DEF / SPEED / SPA / SPD");
                std::string values;for(int i=0;i<6;i++){if(i)values+=" / ";values+=std::to_string(data.personal[species][i]);}Add(values);
            }
            for(const auto& detail:data.AbilityDetails(species))Add(detail);
            for(const auto& detail:data.EvolutionDetails(species))Add(detail);
            Add("LEVEL-UP MOVES");
            for(auto move:data.LearnedMoves(species))Add("L"+std::to_string(move.level)+" "+data.Move(move.move));
        }
        else if(tab==3)
        {
            Add(snapshot.locationLive?"LIVE POSITION":"LAST WARP POSITION");
            Add(data.PlaceName(snapshot.map));Add("X "+std::to_string(snapshot.x)+"  Y "+std::to_string(snapshot.y));
            Add("REPEL STEPS: "+std::to_string(snapshot.repel));
            Add(mapVisible?"FULL REFERENCE MAP - UNLOCKS NOT FILTERED":data.regionMap.Ready()?"SHOW MAP REVEALS THE FULL REGION.":"NO VERIFIED MAP GRAPHICS AVAILABLE.");
        }
        else if(tab==4)
        {
            if(snapshot.battlers.size()<2||!snapshot.battlers[1].species)Add("NO MAPPED BATTLE DATA FOR THIS SCENE / VERSION.");
            else
            {
                const auto& foe=snapshot.battlers[1];Add("FOE: "+data.Species(foe.species));
                Add("LEVEL "+std::to_string(foe.level)+"  HP "+std::to_string(foe.hp)+"/"+std::to_string(foe.maxHp));
                Add(std::string(CompanionTypes::Name(foe.type1))+" / "+CompanionTypes::Name(foe.type2));
                for(const auto& line:CompanionBattle::Lines(snapshot,data))Add(line);
            }
        }
        else if(tab==5)
        {
            if(entries.empty())Add("NO GUIDE ENTRIES FOR THIS VERSION.");
            else{guide=std::clamp(guide,0,(int)entries.size()-1);Add(entries[guide].title);Add(reveal?entries[guide].answer:entries[guide].hint);}
        }
        int pages=std::max(1,((int)lines.size()+15)/16);lastPages=pages;page=std::clamp(page,0,pages-1);
        for(int i=page*16;i<std::min((int)lines.size(),(page+1)*16);i++)canvas.Text(14,84+(i-page*16)*21,lines[i],2,0xFFD4DCE8);
        if(tab==3&&mapVisible&&snapshot.ready&&data.regionMap.Ready())
        {
            const auto& map=data.regionMap;
            int height=std::min(208,608*map.height/map.width),width=height*map.width/map.height;
            int left=(640-width)/2,top=190;
            for(int y=0;y<height;y++)for(int x=0;x<width;x++)canvas.px[(top+y)*640+left+x]=map.pixels[(y*map.height/height)*map.width+x*map.width/width];
            auto marker=data.PlayerMarker(snapshot);
            if(marker.ready)
            {
                int x=left+marker.x*width/map.width,y=top+marker.y*height/map.height;
                canvas.Fill(x-4,y-4,9,9,0xFF101722);canvas.Fill(x-2,y-2,5,5,0xFFFFD75E);
                canvas.Text(14,408,marker.approximate?"MARKER: AREA / EXIT":"MARKER: PLAYER BLOCK",1,0xFFFFD75E);
            }

        }
        if(pages>1)canvas.Text(200,423,"MORE TEXT - TAP HERE",1,0xFFFFD75E);
        canvas.Fill(0,440,640,40,0xFF263346);
        canvas.Text(16,454,tab==1&&!itemDetail?"< POCKET":"< PREVIOUS",1,0xFFFFFFFF);
        canvas.Text(258,454,tab==3?(mapVisible?"HIDE MAP":"SHOW MAP"):tab==1?(itemDetail?"BACK TO BAG":"ALL POCKETS"):tab==5?(reveal?"HIDE ANSWER":"REVEAL"):"BACK / FIRST",1,0xFFFFFFFF);
        canvas.Text(544,454,tab==1&&!itemDetail?"POCKET >":"NEXT >",1,0xFFFFFFFF);
        return canvas;
    }
};
