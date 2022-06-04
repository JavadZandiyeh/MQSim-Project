
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "Flash_Block_Manager.h"
#include "Stats.h"

namespace SSD_Components
{
	Flash_Block_Manager::Flash_Block_Manager(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: Flash_Block_Manager_Base(gc_and_wl_unit, max_allowed_block_erase_count, total_concurrent_streams_no, channel_count, chip_no_per_channel, die_no_per_chip,
			plane_no_per_die, block_no_per_plane, page_no_per_block)
	{
	}

	Flash_Block_Manager::~Flash_Block_Manager()
	{
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;		
		page_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued(page_address);

		//The current write frontier block is written to the end
		if(plane_record->Data_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
			gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		}

		plane_record->Check_bookkeeping_correctness(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_superblock_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		SuperblockBookKeepingType *superblock_record = &superblock_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.SuperblockID];
		superblock_record->Valid_pages_count++;
		superblock_record->Free_pages_count--;		
		page_address.BlockID = superblock_record->Data_wf[stream_id]->BlockID;
		page_address.PageID = superblock_record->Data_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued_superblock(page_address);

		//The current write frontier block is written to the end
		if(superblock_record->Data_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			superblock_record->Data_wf[stream_id] = superblock_record->Get_a_free_block_superblock(stream_id, false);
			gc_and_wl_unit->Check_gc_required(superblock_record->Get_free_block_pool_size_superblock(), page_address);
		}

		superblock_record->Check_bookkeeping_correctness_superblock(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;		
		page_address.BlockID = plane_record->GC_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->GC_wf[stream_id]->Current_page_write_index++;

		
		//The current write frontier block is written to the end
		if (plane_record->GC_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			plane_record->GC_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
			gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		}
		plane_record->Check_bookkeeping_correctness(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_superblock_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		SuperblockBookKeepingType *superblock_record = &superblock_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.SuperblockID];
		superblock_record->Valid_pages_count++;
		superblock_record->Free_pages_count--;		
		page_address.BlockID = superblock_record->GC_wf[stream_id]->BlockID;
		page_address.PageID = superblock_record->GC_wf[stream_id]->Current_page_write_index++;

		
		//The current write frontier block is written to the end
		if (superblock_record->GC_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			superblock_record->GC_wf[stream_id] = superblock_record->Get_a_free_block_superblock(stream_id, false);
			gc_and_wl_unit->Check_gc_required(superblock_record->Get_free_block_pool_size_superblock(), page_address);
		}
		superblock_record->Check_bookkeeping_correctness_superblock(page_address);
	}
	
	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses)
	{
		if(page_addresses.size() > pages_no_per_block) {
			PRINT_ERROR("Error while precondition a physical block: the size of the address list is larger than the pages_no_per_block!")
		}
			
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];
		if (plane_record->Data_wf[stream_id]->Current_page_write_index > 0) {
			PRINT_ERROR("Illegal operation: the Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning function should be executed for an erased block!")
		}

		//Assign physical addresses
		for (int i = 0; i < page_addresses.size(); i++) {
			plane_record->Valid_pages_count++;
			plane_record->Free_pages_count--;
			page_addresses[i].BlockID = plane_record->Data_wf[stream_id]->BlockID;
			page_addresses[i].PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			plane_record->Check_bookkeeping_correctness(page_addresses[i]);
		}

		//Invalidate the remaining pages in the block
		NVM::FlashMemory::Physical_Page_Address target_address(plane_address);
		while (plane_record->Data_wf[stream_id]->Current_page_write_index < pages_no_per_block) {
			plane_record->Free_pages_count--;
			target_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
			target_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
			plane_record->Check_bookkeeping_correctness(plane_address);
		}

		//Update the write frontier
		plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
	}

	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning_superblock(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& superblock_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses)
	{
		if(page_addresses.size() > pages_no_per_block) {
			PRINT_ERROR("Error while precondition a physical block: the size of the address list is larger than the pages_no_per_block!")
		}
			
		SuperblockBookKeepingType *superblock_record = &superblock_manager[superblock_address.ChannelID][superblock_address.ChipID][superblock_address.DieID][superblock_address.SuperblockID];
		if (superblock_record->Data_wf[stream_id]->Current_page_write_index > 0) {
			PRINT_ERROR("Illegal operation: the Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning function should be executed for an erased block!")
		}

		//Assign physical addresses
		for (int i = 0; i < page_addresses.size(); i++) {
			superblock_record->Valid_pages_count++;
			superblock_record->Free_pages_count--;
			page_addresses[i].BlockID = superblock_record->Data_wf[stream_id]->BlockID;
			page_addresses[i].PageID = superblock_record->Data_wf[stream_id]->Current_page_write_index++;
			superblock_record->Check_bookkeeping_correctness_superblock(page_addresses[i]);
		}

		//Invalidate the remaining pages in the block
		NVM::FlashMemory::Physical_Page_Address target_address(superblock_address);
		while (superblock_record->Data_wf[stream_id]->Current_page_write_index < pages_no_per_block) {
			superblock_record->Free_pages_count--;
			target_address.BlockID = superblock_record->Data_wf[stream_id]->BlockID;
			target_address.PageID = superblock_record->Data_wf[stream_id]->Current_page_write_index++;
			Invalidate_page_in_block_for_preconditioning_superblock(stream_id, target_address);
			superblock_record->Check_bookkeeping_correctness_superblock(superblock_address);
		}

		//Update the write frontier
		superblock_record->Data_wf[stream_id] = superblock_record->Get_a_free_block_superblock(stream_id, false);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;
		page_address.BlockID = plane_record->Translation_wf[streamID]->BlockID;
		page_address.PageID = plane_record->Translation_wf[streamID]->Current_page_write_index++;
		program_transaction_issued(page_address);

		//The current write frontier block for translation pages is written to the end
		if (plane_record->Translation_wf[streamID]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			plane_record->Translation_wf[streamID] = plane_record->Get_a_free_block(streamID, true);
			if (!is_for_gc) {
				gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
			}
		}
		plane_record->Check_bookkeeping_correctness(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_superblock_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
		SuperblockBookKeepingType *superblock_record = &superblock_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.SuperblockID];
		superblock_record->Valid_pages_count++;
		superblock_record->Free_pages_count--;
		page_address.BlockID = superblock_record->Translation_wf[streamID]->BlockID;
		page_address.PageID = superblock_record->Translation_wf[streamID]->Current_page_write_index++;
		program_transaction_issued_superblock(page_address);

		//The current write frontier block for translation pages is written to the end
		if (superblock_record->Translation_wf[streamID]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			superblock_record->Translation_wf[streamID] = superblock_record->Get_a_free_block_superblock(streamID, true);
			if (!is_for_gc) {
				gc_and_wl_unit->Check_gc_required(superblock_record->Get_free_block_pool_size_superblock(), page_address);
			}
		}
		superblock_record->Check_bookkeeping_correctness_superblock(page_address);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		plane_record->Valid_pages_count--;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_superblock(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		SuperblockBookKeepingType* superblock_record = &superblock_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.SuperblockID];
		superblock_record->Invalid_pages_count++;
		superblock_record->Valid_pages_count--;
		if (superblock_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		superblock_record->Blocks[page_address.BlockID].Invalid_page_count++;
		superblock_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_for_preconditioning_superblock(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		SuperblockBookKeepingType* superblock_record = &superblock_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.SuperblockID];
		superblock_record->Invalid_pages_count++;
		if (superblock_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		superblock_record->Blocks[page_address.BlockID].Invalid_page_count++;
		superblock_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	void Flash_Block_Manager::Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		Block_Pool_Slot_Type* block = &(plane_record->Blocks[block_address.BlockID]);
		plane_record->Free_pages_count += block->Invalid_page_count;
		plane_record->Invalid_pages_count -= block->Invalid_page_count;

		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]--;
		block->Erase();
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]++;
		plane_record->Add_to_free_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		plane_record->Check_bookkeeping_correctness(block_address);
	}

	void Flash_Block_Manager::Add_erased_block_to_pool_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		SuperblockBookKeepingType *superblock_record = &superblock_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.SuperblockID];
		Block_Pool_Slot_Type* block = &(superblock_record->Blocks[block_address.BlockID]);
		superblock_record->Free_pages_count += block->Invalid_page_count;
		superblock_record->Invalid_pages_count -= block->Invalid_page_count;

		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.SuperblockID][block->Erase_count]--;
		block->Erase();
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.SuperblockID][block->Erase_count]++;
		superblock_record->Add_to_free_block_pool_superblock(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		superblock_record->Check_bookkeeping_correctness_superblock(block_address);
	}

	inline unsigned int Flash_Block_Manager::Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return (unsigned int) plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_block_pool.size();
	}

	inline unsigned int Flash_Block_Manager::Get_pool_size_superblock(const NVM::FlashMemory::Physical_Page_Address& superblock_address)
	{
		return (unsigned int) superblock_manager[superblock_address.ChannelID][superblock_address.ChipID][superblock_address.DieID][superblock_address.SuperblockID].Free_block_pool_superblock.size();
	}
}
