#include "Die.h"

namespace NVM
{
	namespace FlashMemory
	{
		Die::Die(unsigned int PlanesNoPerDie, unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock) :
			Plane_no(PlanesNoPerDie),
			Superblock_no(BlocksNoPerPlane),
			Status(DieStatus::IDLE), CommandFinishEvent(NULL), Expected_finish_time(INVALID_TIME), RemainingSuspendedExecTime(INVALID_TIME),
			CurrentCMD(NULL), SuspendedCMD(NULL), Suspended(false),
			STAT_TotalProgramTime(0), STAT_TotalReadTime(0), STAT_TotalEraseTime(0), STAT_TotalXferTime(0)
		{
			Planes = new Plane*[PlanesNoPerDie];
			for (unsigned int i = 0; i < PlanesNoPerDie; i++) {
				Planes[i] = new Plane(BlocksNoPerPlane, PagesNoPerBlock);
			}
		
			unsigned int SuperblocksNoPerDie = BlocksNoPerPlane;
			unsigned int BlocksNoPerSuperblock = PlanesNoPerDie;
			Superblocks = new Superblock*[SuperblocksNoPerDie];
			for (unsigned int i = 0; i < SuperblocksNoPerDie; i++){
				Superblocks[i] = new Superblock(BlocksNoPerSuperblock, PagesNoPerBlock);
			}
			
			for (unsigned int j = 0; j < PlanesNoPerDie; j++){
				for (unsigned int i = 0; i < BlocksNoPerPlane; i++) {
					Block* _block = new Block(PagesNoPerBlock, i);
					Planes[j]->Blocks[i] = _block;
					Superblocks[i]->Blocks[j] = _block;
				}
			}
		}

		Die::~Die()
		{
			for (unsigned int planeID = 0; planeID < Plane_no; planeID++) {
				delete Planes[planeID];
			}
			delete[] Planes;

			for (unsigned int SuperblockID = 0; SuperblockID < Superblock_no; SuperblockID++){
				delete Superblocks[SuperblockID];
			}
			delete[] Superblocks;
		}
	}
}
