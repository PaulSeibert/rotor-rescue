// ROTOR RESCUE - the hall of fame on the floppy (by Paul Ross Seibert).
// (c) 2026 Desert Palms Software.
#ifndef _HISCORE_H_
#define _HISCORE_H_

#include <ace/types.h>
#include "logic.h"

// What happened to the table on the disk (shown under the hall of fame)
enum { DISK_NONE, DISK_SAVING, DISK_SAVED, DISK_PROTECTED, DISK_FAILED, DISK_LOADED, DISK_DEFAULT };

// Read the table from the boot disk's current directory into g. DISK_LOADED,
// or DISK_DEFAULT (no file, or not a valid one: g keeps its table).
UBYTE hiscoreLoad(tGame *g);

// Write g's table there. DISK_SAVED, DISK_PROTECTED (the disk is write-protected:
// nothing tried) or DISK_FAILED. Takes a few seconds (it lets the file system
// flush to the floppy while the OS runs): call it between pictures, then
// agkPerfSkip(). Never brings up a requester (ACE turns them off).
UBYTE hiscoreSave(const tGame *g);

#endif // _HISCORE_H_
