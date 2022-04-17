#include "Die.h"

namespace NVM
{
	namespace FlashMemory
	{
		Die::Die(unsigned int PlanesNoPerDie, unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock) :
			Plane_no(PlanesNoPerDie),
			Superblock_no(BlocksNoPerPlane),
			BlocksPerPlane_no(BlocksNoPerPlane),
			Status(DieStatus::IDLE), CommandFinishEvent(NULL), Expected_finish_time(INVALID_TIME), RemainingSuspendedExecTime(INVALID_TIME),
			CurrentCMD(NULL), SuspendedCMD(NULL), Suspended(false),
			STAT_TotalProgramTime(0), STAT_TotalReadTime(0), STAT_TotalEraseTime(0), STAT_TotalXferTime(0)
		{	
			Planes = new Plane*[PlanesNoPerDie];
			for (unsigned int i = 0; i < PlanesNoPerDie; i++) {
				Planes[i] = new Plane(BlocksNoPerPlane);
			}

			unsigned int SuperblocksNoPerDie = BlocksNoPerPlane;
			unsigned int BlocksNoPerSuperblock = PlanesNoPerDie;
			Superblocks = new Superblock*[SuperblocksNoPerDie];
			for (unsigned int i = 0; i < SuperblocksNoPerDie; i++){
				Superblocks[i] = new Superblock(BlocksNoPerSuperblock);
			}

			BlocksOfDie = new Block**[PlanesNoPerDie];
			for (unsigned int i = 0; i < PlanesNoPerDie; i++){
				BlocksOfDie[i] = new Block*[BlocksNoPerPlane];
				for (unsigned int j = 0; j < BlocksNoPerPlane; j++){
					Superblocks[j]->Blocks[i] = Planes[i]->Blocks[j] = BlocksOfDie[i][j] = new Block(PagesNoPerBlock, j); 
				}
			}
			
			// std::cout << &(*(Superblocks[0]->Blocks[0])) << "  " << &(*(Planes[0]->Blocks[0])) << std::endl;
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

			for (unsigned int blockIDID = 0; blockIDID < Plane_no; blockIDID++){
				for (unsigned int BlockID = 0; BlockID < BlocksPerPlane_no; BlockID++){
					delete BlocksOfDie[blockIDID][BlockID];
				}
				delete[] BlocksOfDie[blockIDID];
			}
			delete[] BlocksOfDie;
		}
	}
}
