#pragma once
#include <deque>
#include <vector>
#include <cstddef>

// Bounded history, off until enabled. Payloads are emulator snapshots, never written to disk.
class RewindHistory
{
    std::deque<std::vector<unsigned char>> entries;
    size_t used=0,limit=0;
public:
    void Clear(){entries.clear();used=0;}
    void Limit(size_t bytes){limit=bytes;while(!entries.empty()&&used>limit){used-=entries.front().size();entries.pop_front();}}
    size_t Size()const{return entries.size();}
    size_t Bytes()const{return used;}
    bool Push(std::vector<unsigned char> value)
    {
        if(!limit||value.size()>limit)return false;
        while(!entries.empty() && used>limit-value.size()){used-=entries.front().size();entries.pop_front();}
        used+=value.size();entries.push_back(std::move(value));return true;
    }
    std::vector<unsigned char> Pop()
    {
        if(entries.empty())return {};
        auto value=std::move(entries.back());entries.pop_back();used-=value.size();return value;
    }
};
