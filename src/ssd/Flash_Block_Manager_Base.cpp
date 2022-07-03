#include "Flash_Block_Manager.h"
#include <algorithm>

namespace SSD_Components
{
	std::ofstream result_file;
	unsigned int Block_Pool_Slot_Type::Page_vector_size = 0;
	Flash_Block_Manager_Base::Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: gc_and_wl_unit(gc_and_wl_unit), max_allowed_block_erase_count(max_allowed_block_erase_count), total_concurrent_streams_no(total_concurrent_streams_no),
		channel_count(channel_count), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(die_no_per_chip), plane_no_per_die(plane_no_per_die),
		block_no_per_plane(block_no_per_plane), superblock_no_per_die(block_no_per_plane), block_no_per_superblock(plane_no_per_die),
		pages_no_per_block(page_no_per_block)
	{
		result_file.open("log_file_phase2.txt", std::ios_base::app);
		// result_file << "\n----------new scenario----------\n";

		// die_manager initializing
		die_manager = new DieBookKeepingType**[channel_count];
		for (unsigned int channelID = 0; channelID < channel_count; channelID++) {
			die_manager[channelID] = new DieBookKeepingType*[chip_no_per_channel];
			for (unsigned int chipID = 0; chipID < chip_no_per_channel; chipID++) {
				die_manager[channelID][chipID] = new DieBookKeepingType[die_no_per_chip];
				for (unsigned int dieID = 0; dieID < die_no_per_chip; dieID++) {
					die_manager[channelID][chipID][dieID].Superblocks = new Superblock_Slot_Type[superblock_no_per_die];
					die_manager[channelID][chipID][dieID].plane_manager_die = new PlaneBookKeepingType[plane_no_per_die];
					die_manager[channelID][chipID][dieID].plane_no_per_die = plane_no_per_die;
					die_manager[channelID][chipID][dieID].superblock_no_per_die = superblock_no_per_die;

					//Initialize superblock book keeping data structure for die
					for (unsigned int superblockID = 0; superblockID < superblock_no_per_die; superblockID++) {
						die_manager[channelID][chipID][dieID].Superblocks[superblockID].SuperblockID = superblockID;						
						die_manager[channelID][chipID][dieID].Superblocks[superblockID].Blocks = new Block_Pool_Slot_Type*[block_no_per_superblock];						
						die_manager[channelID][chipID][dieID].Superblocks[superblockID].block_no_per_superblock = block_no_per_superblock;					
					}

					//Initialize plane book keeping data structure for die
					for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Total_pages_count = block_no_per_plane * pages_no_per_block;
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Free_pages_count = block_no_per_plane * pages_no_per_block;
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Valid_pages_count = 0;
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Invalid_pages_count = 0;
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Ongoing_erase_operations.clear();
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks = new Block_Pool_Slot_Type[block_no_per_plane];
						
						//Initialize block pool for plane
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++) {
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].BlockID = blockID;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Current_page_write_index = 0;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Current_status = Block_Service_Status::IDLE;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Invalid_page_count = 0;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Erase_count = 0;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Holds_mapping_data = false;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Has_ongoing_gc_wl = false;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Erase_transaction = NULL;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Ongoing_user_program_count = 0;
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Ongoing_user_read_count = 0;
							Block_Pool_Slot_Type::Page_vector_size = pages_no_per_block / (sizeof(uint64_t) * 8) + (pages_no_per_block % (sizeof(uint64_t) * 8) == 0 ? 0 : 1);
							die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Invalid_page_bitmap = new uint64_t[Block_Pool_Slot_Type::Page_vector_size];
							for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++) {
								die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID].Invalid_page_bitmap[i] = All_VALID_PAGE;
							}
							die_manager[channelID][chipID][dieID].Superblocks[blockID].Blocks[planeID] = &die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Blocks[blockID];
						}

						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Data_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].Translation_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						die_manager[channelID][chipID][dieID].plane_manager_die[planeID].GC_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
					}

					for (unsigned int superblockID = 0; superblockID < superblock_no_per_die; superblockID++) {
						die_manager[channelID][chipID][dieID].Add_to_free_superblock_pool(&die_manager[channelID][chipID][dieID].Superblocks[superblockID], false);
					}

					// for (unsigned int superblockID = 0; superblockID < superblock_no_per_die; superblockID++) {
					// 	result_file << die_manager[channelID][chipID][dieID].Superblocks[superblockID].SuperblockID << ", " << &die_manager[channelID][chipID][dieID].Superblocks[superblockID] << "\n";
					// }

					die_manager[channelID][chipID][dieID].Data_swf = new Superblock_Slot_Type*[total_concurrent_streams_no];
					die_manager[channelID][chipID][dieID].Translation_swf = new Superblock_Slot_Type*[total_concurrent_streams_no];
					die_manager[channelID][chipID][dieID].GC_swf = new Superblock_Slot_Type*[total_concurrent_streams_no];
					result_file << "\nchannelID: " << channelID << " , chipID: " << chipID << " , dieID: " << dieID << "\n";
					for (unsigned int stream_cntr = 0; stream_cntr < total_concurrent_streams_no; stream_cntr++) {
						die_manager[channelID][chipID][dieID].Set_swf(0, stream_cntr);
						die_manager[channelID][chipID][dieID].Set_swf(1, stream_cntr);
						die_manager[channelID][chipID][dieID].Set_swf(2, stream_cntr);
					}
				}
			}
		}
		result_file.close();
	}

	Flash_Block_Manager_Base::~Flash_Block_Manager_Base() 
	{
		// delete die_manager
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++) {
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++) {
				for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++) {

					// plane_manager
					for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++) {
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++) {
							delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die[plane_id].Blocks[blockID].Invalid_page_bitmap;
						}
						delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die[plane_id].Blocks;
						delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die[plane_id].GC_wf;
						delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die[plane_id].Data_wf;
						delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die[plane_id].Translation_wf;
					}
					delete[] die_manager[channel_id][chip_id][die_id].plane_manager_die;
				
					// die_manager
					for (unsigned int superblock_id = 0; superblock_id < superblock_no_per_die; superblock_id++) {
						delete[] die_manager[channel_id][chip_id][die_id].Superblocks[superblock_id].Blocks;
					}
					delete[] die_manager[channel_id][chip_id][die_id].GC_swf;
					delete[] die_manager[channel_id][chip_id][die_id].Data_swf;
					delete[] die_manager[channel_id][chip_id][die_id].Translation_swf;
					delete[] die_manager[channel_id][chip_id][die_id].Superblocks;

				}
				delete[] die_manager[channel_id][chip_id];
			}
			delete[] die_manager[channel_id];
		}
		delete[] die_manager;
	}

	void Flash_Block_Manager_Base::Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* gcwl)
	{
		this->gc_and_wl_unit = gcwl;
	}

	void Block_Pool_Slot_Type::Erase()
	{
		Current_page_write_index = 0;
		Invalid_page_count = 0;
		Erase_count++;
		for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++) {
			Invalid_page_bitmap[i] = All_VALID_PAGE;
		}
		Stream_id = NO_STREAM;
		Holds_mapping_data = false;
		Erase_transaction = NULL;
	}

	// -----------------------------------------------------------------------------------------------

	// unsigned int Superblock_Slot_Type::Get_total_pages_count_superblock(){
	// 	unsigned int Total_pages_count = 0;
	// 	for (int i = 0; i < block_no_per_superblock; i++){
	// 		Total_pages_count += 
	// 	}
	// 	return Total_pages_count;
	// }
	// unsigned int Superblock_Slot_Type::Get_free_pages_count_superblock(){

	// }
	// unsigned int Superblock_Slot_Type::Get_valid_pages_count_superblock(){

	// }
	unsigned int Superblock_Slot_Type::Get_invalid_pages_count_superblock(){
		unsigned int Invalid_pages_count = 0;
		for (int i = 0; i < block_no_per_superblock; i++){
			Invalid_pages_count += Blocks[i]->Invalid_page_count;
		}
		return Invalid_pages_count;
	}
	unsigned int Superblock_Slot_Type::Get_erase_count_superblock(){
		unsigned int Erase_count = 0;
		for (int i = 0; i < block_no_per_superblock; i++){
			Erase_count += Blocks[i]->Erase_count;
		}
		return Erase_count;
	}

	// -----------------------------------------------------------------------------------------------
	void DieBookKeepingType::Set_swf(unsigned int type, unsigned int stream_cntr)
	{
		if (type == 0)
		{
			Data_swf[stream_cntr] = Get_a_free_superblock(stream_cntr, false);
			result_file << "get superblock " << Data_swf[stream_cntr]->SuperblockID << " for stream " << stream_cntr << " as Data_swf\n";
			for (unsigned int i = 0; i < plane_no_per_die; i++){
				result_file << "    -get block " << i << " with address " << Data_swf[stream_cntr]->Blocks[i] << "\n";
			}
			for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
				plane_manager_die[planeID].Data_wf[stream_cntr] = Data_swf[stream_cntr]->Blocks[planeID];
			}
		}
		else if (type == 1)
		{
			Translation_swf[stream_cntr] = Get_a_free_superblock(stream_cntr, false);
			result_file << "get superblock " << Translation_swf[stream_cntr]->SuperblockID << " for stream " << stream_cntr << " as Translation_swf\n";
			for (unsigned int i = 0; i < plane_no_per_die; i++){
				result_file << "    -get block " << i << " with address " << Translation_swf[stream_cntr]->Blocks[i] << "\n";
			}
			for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
				plane_manager_die[planeID].Translation_wf[stream_cntr] = Translation_swf[stream_cntr]->Blocks[planeID];
			}
		}
		else if (type == 2)
		{
			GC_swf[stream_cntr] = Get_a_free_superblock(stream_cntr, false);
			result_file << "get superblock " << GC_swf[stream_cntr]->SuperblockID << " for stream " << stream_cntr << " as GC_swf\n";
			for (unsigned int i = 0; i < plane_no_per_die; i++){
				result_file << "    -get block " << i << " with address " << GC_swf[stream_cntr]->Blocks[i] << "\n";
			}
			for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
				plane_manager_die[planeID].GC_wf[stream_cntr] = GC_swf[stream_cntr]->Blocks[planeID];
			}
		}
	}

	// -----------------------------------------------------------------------------------------------
	template<typename K, typename V>
	typename std::multimap<K, V>::const_iterator find_item_by_key_and_value(const std::multimap<K, V>& mm, K k, V v) {
    	auto range = mm.equal_range(k);
    	return std::find_if(range.first, range.second, [v](const auto& p) {return p.second == v; });
	}

	template<typename K, typename V>
	void erase_item_by_key_and_value(std::multimap<K, V>& mm, K k, V v) {
    	auto iter = find_item_by_key_and_value(mm, k, v);
		// std::count << iter << "\n";
    	if (iter != mm.end())
        	mm.erase(iter);
	}

	Block_Pool_Slot_Type* PlaneBookKeepingType::Get_a_free_block(stream_id_type stream_id, bool for_mapping_data)
	{
		Block_Pool_Slot_Type* new_block = NULL;
		new_block = (*Free_block_pool.begin()).second;//Assign a new write frontier block
		if (Free_block_pool.size() == 0) {
			PRINT_ERROR("Requesting a free block from an empty pool in plane!")
		}
		Free_block_pool.erase(Free_block_pool.begin());
		new_block->Stream_id = stream_id;
		new_block->Holds_mapping_data = for_mapping_data;
		Block_usage_history.push(new_block->BlockID);

		return new_block;
	}

	Superblock_Slot_Type* DieBookKeepingType::Get_a_free_superblock(stream_id_type stream_id, bool for_mapping_data)
	{
		Superblock_Slot_Type* new_superblock = NULL;
		new_superblock = (*Free_superblock_pool.begin()).second;//Assign a new write frontier block
		if (Free_superblock_pool.size() == 0) {
			PRINT_ERROR("Requesting a free superblock from an empty pool in die!")
		}
		Free_superblock_pool.erase(Free_superblock_pool.begin());
		new_superblock->Stream_id = stream_id;
		new_superblock->Holds_mapping_data = for_mapping_data;
		Superblock_usage_history.push(new_superblock->SuperblockID);

		// delete blocks of this superblock from Free_block_pool
		// plane_manager_die->Free_block_pool.erase(<new_superblock->Erase_count, new_superblock->Blocks[0]>)
		
		// for (int i = 0; i < plane_no_per_die; i++){
		// 	erase_item_by_key_and_value(plane_manager_die->Free_block_pool, new_superblock->Blocks[i]->Erase_count, new_superblock->Blocks[i]);
		// }
		
		return new_superblock;
	}
	
	// -----------------------------------------------------------------------------------------------
	
	void PlaneBookKeepingType::Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (Total_pages_count != Free_pages_count + Valid_pages_count + Invalid_pages_count) {
			PRINT_ERROR("Inconsistent status in the plane bookkeeping record!")
		}
		if (Free_pages_count == 0) {
			PRINT_ERROR("Plane " << "@" << plane_address.ChannelID << "@" << plane_address.ChipID << "@" << plane_address.DieID << "@" << plane_address.PlaneID << " pool size: " << Get_free_block_pool_size() << " ran out of free pages! Bad resource management! It is not safe to continue simulation!");
		}
	}

	// -----------------------------------------------------------------------------------------------

	unsigned int PlaneBookKeepingType::Get_free_block_pool_size()
	{
		return (unsigned int)Free_block_pool.size();
	}

	unsigned int DieBookKeepingType::Get_free_superblock_pool_size()
	{
		return (unsigned int)Free_superblock_pool.size();
	}

	// -----------------------------------------------------------------------------------------------

	void PlaneBookKeepingType::Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl)
	{
		if (consider_dynamic_wl) {
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(block->Erase_count, block);
			Free_block_pool.insert(entry);
		} else {
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(0, block);
			Free_block_pool.insert(entry);
		}
	}

	void DieBookKeepingType::Add_to_free_superblock_pool(Superblock_Slot_Type* superblock, bool consider_dynamic_wl)
	{
		for (unsigned int i = 0; i < plane_no_per_die; i++){
			// plane_manager_die[i].Add_to_free_block_pool(&plane_manager_die[i].Blocks[superblock->SuperblockID], false);
			plane_manager_die[i].Add_to_free_block_pool(superblock->Blocks[i], false);
		}
		if (consider_dynamic_wl) {
			std::pair<unsigned int, Superblock_Slot_Type*> entry(superblock->Get_erase_count_superblock(), superblock);
			Free_superblock_pool.insert(entry);
		} else {
			std::pair<unsigned int, Superblock_Slot_Type*> entry(0, superblock);
			Free_superblock_pool.insert(entry);
		}
	}

	// -----------------------------------------------------------------------------------------------

	unsigned int Flash_Block_Manager_Base::Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		unsigned int max_erased_block = 0;
		PlaneBookKeepingType *plane_record = &die_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID].plane_manager_die[plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++) {
			if (plane_record->Blocks[i].Erase_count > plane_record->Blocks[max_erased_block].Erase_count) {
				max_erased_block = i;
			}
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count) {
				min_erased_block = i;
			}
		}

		return max_erased_block - min_erased_block;
	}

	// -----------------------------------------------------------------------------------------------

	flash_block_ID_type Flash_Block_Manager_Base::Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		PlaneBookKeepingType *plane_record = &die_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID].plane_manager_die[plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++) {
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count) {
				min_erased_block = i;
			}
		}
		
		return min_erased_block;
	}

	flash_superblock_ID_type Flash_Block_Manager_Base::Get_coldest_superblock_id(const NVM::FlashMemory::Physical_Page_Address& die_address)
	{
		unsigned int min_erased_superblock = 0;
		DieBookKeepingType *die_record = Get_die_bookkeeping_entry(die_address);
		
		for (unsigned int i = 1; i < superblock_no_per_die; i++) {
			if (die_record->Superblocks[i].Get_erase_count_superblock() < die_record->Superblocks[min_erased_superblock].Get_erase_count_superblock()) {
				min_erased_superblock = i;
			}
		}
		
		return min_erased_superblock;
	}

	// -----------------------------------------------------------------------------------------------

	PlaneBookKeepingType* Flash_Block_Manager_Base::Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return &(die_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID].plane_manager_die[plane_address.PlaneID]);
	}

	DieBookKeepingType* Flash_Block_Manager_Base::Get_die_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& die_address){
		return &(die_manager[die_address.ChannelID][die_address.ChipID][die_address.DieID]);
	}

	bool Flash_Block_Manager_Base::Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl;
	}
	
	bool Flash_Block_Manager_Base::Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		return (plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count + plane_record->Blocks[block_address.BlockID].Ongoing_user_read_count == 0);
	}

	void Flash_Block_Manager_Base::GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = true;
	}

	void Flash_Block_Manager_Base::program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count++;
	}
	
	void Flash_Block_Manager_Base::Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count++;
	}

	void Flash_Block_Manager_Base::Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count--;
	}

	void Flash_Block_Manager_Base::Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count--;
	}
	
	bool Flash_Block_Manager_Base::Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count > 0;
	}

	void Flash_Block_Manager_Base::GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = false;
	}
	
	bool Flash_Block_Manager_Base::Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id)
	{
		if ((block->Invalid_page_bitmap[page_id / 64] & (((uint64_t)1) << page_id)) == 0) {
			return true;
		}
		return false;
	}
}
