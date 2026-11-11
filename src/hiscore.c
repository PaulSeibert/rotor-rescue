// ROTOR RESCUE - the hall of fame on the floppy (by Paul Ross Seibert).
// (c) 2026 Desert Palms Software.
//
// One small file, "rrscores" (SCORES_BYTES: logicScoresPack's format, with a
// checksum), next to the game. AmigaDOS with the OS briefly back, as ACE's
// disk files do. A write-protected disk is found with Info() before anything
// is written; requesters are off (ACE sets pr_WindowPtr to -1), so a missing,
// full or protected disk is an error code, never a requester waiting for a
// click nobody can see.

#include <proto/dos.h>
#include <dos/dos.h>
#include <ace/managers/system.h>
#include "hiscore.h"

#define HISCORE_FILE "rrscores"

static struct InfoData s_sInfo __attribute__((aligned(4)));   // (a BPTR target: longword-aligned)
static UBYTE s_pBuf[SCORES_BYTES];

static void osOn(void) {
	systemUse();
	systemReleaseBlitterToOs();   // (trackdisk needs the blitter on Kickstart 1.3)
}

static void osOff(void) {
	systemGetBlitterFromOs();
	systemUnuse();
}

__attribute__((optimize("Os"))) UBYTE hiscoreLoad(tGame *g) {
	UBYTE ubResult = DISK_DEFAULT;
	osOn();
	BPTR fh = Open((CONST_STRPTR)HISCORE_FILE, MODE_OLDFILE);
	if(fh) {
		LONG lRead = Read(fh, s_pBuf, SCORES_BYTES);
		Close(fh);
		if(lRead == SCORES_BYTES && logicScoresUnpack(g, s_pBuf)) ubResult = DISK_LOADED;
	}
	osOff();
	return ubResult;
}

__attribute__((optimize("Os"))) UBYTE hiscoreSave(const tGame *g) {
	UBYTE ubResult = DISK_FAILED;
	logicScoresPack(g, s_pBuf);
	osOn();
	BPTR bpDir = Lock((CONST_STRPTR)"", ACCESS_READ);   // the current directory: the game's
	if(bpDir) {
		if(Info(bpDir, &s_sInfo) && s_sInfo.id_DiskState == ID_WRITE_PROTECTED) ubResult = DISK_PROTECTED;
		UnLock(bpDir);
	}
	if(ubResult != DISK_PROTECTED) {
		BPTR fh = Open((CONST_STRPTR)HISCORE_FILE, MODE_NEWFILE);
		if(fh) {
			LONG lWritten = Write(fh, s_pBuf, SCORES_BYTES);
			Close(fh);
			if(lWritten == SCORES_BYTES) ubResult = DISK_SAVED;
			// the file system writes its buffers and the drive its track a moment
			// after the last access: keep the OS running until it has
			Delay(2 * 50);
		}
		else if(IoErr() == ERROR_DISK_WRITE_PROTECTED) ubResult = DISK_PROTECTED;
	}
	osOff();
	return ubResult;
}
