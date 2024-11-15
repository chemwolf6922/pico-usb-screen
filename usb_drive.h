#pragma once

/** Singleton library */

#include "disk.h"

int usb_drive_init_singleton(const char* serial, disk_t* disk);
