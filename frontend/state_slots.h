#pragma once
#include <filesystem>
#include <chrono>
#include <functional>
#include <string>
#include <cstdio>
#include <ctime>

namespace StateSlots
{
namespace fs=std::filesystem;
inline std::string Path(const std::string& quick,int slot){return quick+".slot"+std::to_string(slot);}
inline bool Exists(const std::string& path){std::error_code ec;return fs::is_regular_file(fs::u8path(path),ec)&&fs::file_size(fs::u8path(path),ec)>4&&!ec;}
inline bool PreviewCurrent(const std::string& path)
{
    std::error_code ec;
    const auto state=fs::last_write_time(fs::u8path(path),ec);if(ec)return false;
    const auto image=fs::last_write_time(fs::u8path(path+".bmp"),ec);
    return !ec && image>=state;
}
inline std::string Date(const std::string& path)
{
    std::error_code ec;auto date=fs::last_write_time(fs::u8path(path),ec);if(ec)return "EMPTY";
    auto system=std::chrono::time_point_cast<std::chrono::system_clock::duration>(date-fs::file_time_type::clock::now()+std::chrono::system_clock::now());
    std::time_t t=std::chrono::system_clock::to_time_t(system);std::tm* local=std::localtime(&t);char text[32]={};
    if(local)std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M",local);
    return text;
}
// When it was saved, as the slot grid says it: "Today, 15:40", "Yesterday, 22:15", "3 Nov, 19:47";
// "" for an empty slot.
inline std::string Friendly(const std::string& path)
{
    if(!Exists(path))return "";
    std::error_code ec;auto date=fs::last_write_time(fs::u8path(path),ec);if(ec)return "";
    auto system=std::chrono::time_point_cast<std::chrono::system_clock::duration>(date-fs::file_time_type::clock::now()+std::chrono::system_clock::now());
    std::time_t t=std::chrono::system_clock::to_time_t(system),now=std::time(nullptr);
    std::tm when{},today{};
    if(std::tm* p=std::localtime(&t))when=*p;else return "";
    if(std::tm* p=std::localtime(&now))today=*p;
    char clock[8];std::snprintf(clock,sizeof clock,"%02d:%02d",when.tm_hour,when.tm_min);
    std::tm midnight=today;midnight.tm_hour=midnight.tm_min=midnight.tm_sec=0;
    const std::time_t start=std::mktime(&midnight);
    if(t>=start)return std::string("Today, ")+clock;
    if(t>=start-86400)return std::string("Yesterday, ")+clock;
    static const char* const months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    return std::to_string(when.tm_mday)+" "+months[when.tm_mon%12]+(when.tm_year!=today.tm_year?" "+std::to_string(1900+when.tm_year):std::string())+", "+clock;
}
inline bool Save(const std::string& path,const std::function<bool(const std::string&)>& write)
{
    const fs::path target=fs::u8path(path),temp=fs::u8path(path+".pending"),backup=fs::u8path(path+".previous");
    std::error_code ec;
    if(fs::exists(temp,ec)||ec)return false; // Keep an interrupted write for recovery.
    if(!write(temp.u8string())||!Exists(temp.u8string()))return false;
    bool had=fs::exists(target,ec);if(ec)return false;
    if(had){fs::remove(backup,ec);if(ec)return false;fs::rename(target,backup,ec);if(ec)return false;}
    fs::rename(temp,target,ec);
    if(ec){if(had){std::error_code recovery;fs::rename(backup,target,recovery);}return false;}
    return true;
}
}
