#pragma once
namespace mango_avatar {
inline const char* status_text(int state){switch(state){
case 0:return "off";case 1:return "waiting for player";case 2:return "preparing file";case 3:return "opening bundle";case 4:return "loading model";case 5:return "binding skeleton";case 6:return "active";case 7:return "waiting for game bones";case 8:return "waiting for model bones";case 9:return "move camera away from wall";
case -1:return "local body missing";case -2:return "engine binding missing";case -3:return "cache file failed";case -4:return "asset type missing";
case -11:return "asset loader binding failed";case -12:return "bundle open failed";case -13:return "asset request missing";case -14:return "asset load timed out";case -15:return "bundle rejected";case -16:return "model request failed";case -17:return "model asset missing";
case -21:return "model clone failed";case -22:return "model transform missing";case -23:return "model meshes missing";case -24:return "pose unavailable";default:return "engine exception";}}
}
