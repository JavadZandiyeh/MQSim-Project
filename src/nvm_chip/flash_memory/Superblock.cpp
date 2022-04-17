#include "Superblock.h"

namespace NVM
{
	namespace FlashMemory
	{
		// Superblock::Superblock(unsigned int BlocksNoPerSuperblock, unsigned int PagesNoPerBlock) :
		Superblock::Superblock(unsigned int BlocksNoPerSuperblock) :
			Read_count(0), Progam_count(0), Erase_count(0)
		{
			Healthy_block_no = BlocksNoPerSuperblock;
			Blocks = new Block*[BlocksNoPerSuperblock];
			// for (unsigned int i = 0; i < BlocksNoPerSuperblock; i++) {
			// 	Blocks[i] = new Block(PagesNoPerBlock, i);
			// }
			Allocated_streams = NULL;
		}

		Superblock::~Superblock()
		{
			// for (unsigned int i = 0; i < Healthy_block_no; i++) {
			// 	delete Blocks[i];
			// }
			// delete[] Blocks;
		}
	}
}