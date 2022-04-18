#include "Die.h"

namespace NVM
{
	namespace FlashMemory
	{
		Die::Die(unsigned int PlanesNoPerDie, unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock) :
			Plane_no(PlanesNoPerDie),
			Superblock_no(BlocksNoPerPlane), // number of superblocks
			BlocksPerPlane_no(BlocksNoPerPlane), // number of blocks per plane
			Status(DieStatus::IDLE), CommandFinishEvent(NULL), Expected_finish_time(INVALID_TIME), RemainingSuspendedExecTime(INVALID_TIME),
			CurrentCMD(NULL), SuspendedCMD(NULL), Suspended(false),
			STAT_TotalProgramTime(0), STAT_TotalReadTime(0), STAT_TotalEraseTime(0), STAT_TotalXferTime(0)
		{	
			// creating an array of Planes for Die //
			Planes = new Plane*[PlanesNoPerDie];
			for (unsigned int i = 0; i < PlanesNoPerDie; i++) {
				Planes[i] = new Plane(BlocksNoPerPlane);
			}

			/* finding the number of superblocks in each Die and the 
			   number of blocks in each superblock */
			unsigned int SuperblocksNoPerDie = BlocksNoPerPlane;
			unsigned int BlocksNoPerSuperblock = PlanesNoPerDie;
			// creating an array of Superblocks for Die // 
			Superblocks = new Superblock*[SuperblocksNoPerDie];
			for (unsigned int i = 0; i < SuperblocksNoPerDie; i++){
				Superblocks[i] = new Superblock(BlocksNoPerSuperblock);
			}

			// initializing blocks and pointing to by Superblocks and Planes //
			BlocksOfDie = new Block**[PlanesNoPerDie];
			for (unsigned int i = 0; i < PlanesNoPerDie; i++){
				BlocksOfDie[i] = new Block*[BlocksNoPerPlane];
				for (unsigned int j = 0; j < BlocksNoPerPlane; j++){
					Superblocks[j]->Blocks[i] = Planes[i]->Blocks[j] = BlocksOfDie[i][j] = new Block(PagesNoPerBlock, j); 
				}
			}
			
			// check if Superblocks and Planes point to the same blocks //
			// std::cout << &(*(Superblocks[0]->Blocks[0])) << "  " << &(*(Planes[0]->Blocks[0])) << std::endl;	

			// allocate all Superblocks to FlowNo users
			unsigned int FlowNo = 2;
			allocate_superblocks(SuperblocksNoPerDie, FlowNo);
		}

		Die::~Die()
		{
			for (unsigned int planeID = 0; planeID < Plane_no; planeID++) {
				delete Planes[planeID];
			}
			delete[] Planes;

			// deleting Superblocks array
			for (unsigned int SuperblockID = 0; SuperblockID < Superblock_no; SuperblockID++){
				delete Superblocks[SuperblockID];
			}
			delete[] Superblocks;

			// deleting BlocksOfDie array
			for (unsigned int blockIDID = 0; blockIDID < Plane_no; blockIDID++){
				for (unsigned int BlockID = 0; BlockID < BlocksPerPlane_no; BlockID++){
					delete BlocksOfDie[blockIDID][BlockID];
				}
				delete[] BlocksOfDie[blockIDID];
			}
			delete[] BlocksOfDie;
		}

		// allocating Superblocks to flows/users
		void allocate_superblocks(unsigned int Superblock_no, unsigned int Flow_no){
			// random allocation
			int allocatation[Superblock_no];
			for (unsigned int i = 0; i < Superblock_no; i++){
				allocatation[i] = rand() % Flow_no;
			}
		}
	}
}
