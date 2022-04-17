#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "../NVM_Types.h"
#include "FlashTypes.h"
#include "Block.h"
#include "Flash_Command.h"

namespace NVM
{
	namespace FlashMemory
	{
		class Superblock
		{
		public:
			// Superblock(unsigned int BlocksNoPerSuperblock, unsigned int PagesNoPerBlock);
			Superblock(unsigned int BlocksNoPerSuperblock);
			~Superblock();
			Block** Blocks;
			unsigned int Healthy_block_no;
			unsigned long Read_count;                     //how many read count in the process of workload
			unsigned long Progam_count;
			unsigned long Erase_count;
			stream_id_type* Allocated_streams;
		};
	}
}
#endif // !SUPERBLOCK_H
