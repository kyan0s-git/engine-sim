#include "../include/yds_sdl_monitor.h"

ysSdlMonitor::ysSdlMonitor() : ysMonitor(Platform::Sdl) {
    InitializeDeviceName(256);
}

ysSdlMonitor::~ysSdlMonitor() {
    /* void */
}
