#ifndef BLOCK_POOL_MANAGER_BASE_H
#define BLOCK_POOL_MANAGER_BASE_H

#include <list>
#include <cstdint>
#include <queue>
#include <set>
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "GC_and_WL_Unit_Base.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"

namespace SSD_Components
{
#define All_VALID_PAGE 0x0000000000000000ULL
	class GC_and_WL_Unit_Base;
	/*
	* Block_Service_Status is used to impelement a state machine for each physical block in order to
	* eliminate race conditions between GC page movements and normal user I/O requests.
	* Allowed transitions:
	* 1: IDLE -> GC, IDLE -> USER
	* 2: GC -> IDLE, GC -> GC_UWAIT
	* 3: USER -> IDLE, USER -> GC_USER
	* 4: GC_UWAIT -> GC, GC_UWAIT -> GC_UWAIT
	* 5: GC_USER -> GC
	*/
	enum class Block_Service_Status {IDLE, GC_WL, USER, GC_USER, GC_UWAIT, GC_USER_UWAIT};
	
	class Block_Pool_Slot_Type
	{
	public:
		flash_block_ID_type BlockID;
		flash_page_ID_type Current_page_write_index;
		Block_Service_Status Current_status;
		unsigned int Invalid_page_count;
		unsigned int Erase_count;
		static unsigned int Page_vector_size;
		uint64_t* Invalid_page_bitmap;//A bit sequence that keeps track of valid/invalid status of pages in the block. A "0" means valid, and a "1" means invalid.
		stream_id_type Stream_id = NO_STREAM;
		bool Holds_mapping_data = false;
		bool Has_ongoing_gc_wl = false;
		NVM_Transaction_Flash_ER* Erase_transaction;
		bool Hot_block = false;//Used for hot/cold separation mentioned in the "On the necessity of hot and cold data identification to reduce the write amplification in flash-based SSDs", Perf. Eval., 2014.
		int Ongoing_user_read_count;
		int Ongoing_user_program_count;
		void Erase();
	};

	class PlaneBookKeepingType
	{
	public:
		unsigned int Total_pages_count;
		unsigned int Free_pages_count;
		unsigned int Valid_pages_count;
		unsigned int Invalid_pages_count;
		Block_Pool_Slot_Type* Blocks;
		std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_block_pool;
		Block_Pool_Slot_Type** Data_wf, ** GC_wf; //The write frontier blocks for data and GC pages. MQSim adopts Double Write Frontier approach for user and GC writes which is shown very advantages in: B. Van Houdt, "On the necessity of hot and cold data identification to reduce the write amplification in flash - based SSDs", Perf. Eval., 2014
		Block_Pool_Slot_Type** Translation_wf; //The write frontier blocks for translation GC pages
		std::queue<flash_block_ID_type> Block_usage_history;//A fifo queue that keeps track of flash blocks based on their usage history
		std::set<flash_block_ID_type> Ongoing_erase_operations;
		Block_Pool_Slot_Type* Get_a_free_block(stream_id_type stream_id, bool for_mapping_data);
		unsigned int Get_free_block_pool_size();
		void Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
	};

	class Superblock_Slot_Type
	{
	public:
		flash_superblock_ID_type SuperblockID;
		// unsigned int Total_pages_count;
		// unsigned int Free_pages_count;
		// unsigned int Valid_pages_count;
		// unsigned int Invalid_pages_count;
		unsigned int Erase_count;
		stream_id_type Stream_id = NO_STREAM; //which stream this superblock belong to
		bool Holds_mapping_data = false;
		// bool Has_ongoing_gc_wl = false;
		int Ongoing_user_program_count;
		int Ongoing_user_read_count;
		Block_Pool_Slot_Type** Blocks;
		// std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_block_pool_superblock;
		// std::queue<flash_block_ID_type> Block_usage_history;//A fifo queue that keeps track of flash blocks based on their usage history
		// std::set<flash_block_ID_type> Ongoing_erase_operations;
		// Block_Pool_Slot_Type* Get_a_free_block_superblock(stream_id_type stream_id, bool for_mapping_data);
		// unsigned int Get_free_block_pool_size_superblock();
		// void Check_bookkeeping_correctness_superblock(const NVM::FlashMemory::Physical_Page_Address& superblock_address);
		// void Add_to_free_block_pool_superblock(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
	};

	class DieBookKeepingType
	{
	public:
		// unsigned int Total_pages_count;
		// unsigned int Free_pages_count;
		// unsigned int Valid_pages_count;
		// unsigned int Invalid_pages_count;
		Superblock_Slot_Type* Superblocks;
		PlaneBookKeepingType* plane_manager_die;
		std::multimap<unsigned int, Superblock_Slot_Type*> Free_superblock_pool;
		Superblock_Slot_Type** Data_swf, ** GC_swf; //The write frontier superblocks for data and GC pages. MQSim adopts Double Write Frontier approach for user and GC writes which is shown very advantages in: B. Van Houdt, "On the necessity of hot and cold data identification to reduce the write amplification in flash - based SSDs", Perf. Eval., 2014
		Superblock_Slot_Type** Translation_swf; //The write frontier superblocks for translation GC pages
		std::queue<flash_superblock_ID_type> Superblock_usage_history;//A fifo queue that keeps track of flash superblocks based on their usage history
		std::set<flash_superblock_ID_type> Ongoing_erase_operations;
		Superblock_Slot_Type* Get_a_free_superblock(stream_id_type stream_id, bool for_mapping_data);
		unsigned int Get_free_superblock_pool_size();
		void Check_bookkeeping_correctness_die(const NVM::FlashMemory::Physical_Page_Address& die_address);
		void Add_to_free_superblock_pool(Superblock_Slot_Type* superblock, bool consider_dynamic_wl, unsigned int block_no_per_superblock, bool release);
	};

	class Flash_Block_Manager_Base
	{
		friend class Address_Mapping_Unit_Page_Level;
		friend class GC_and_WL_Unit_Page_Level;
		friend class GC_and_WL_Unit_Base;
	public:
		Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
			unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
			unsigned int block_no_per_plane, unsigned int page_no_per_block);
		virtual ~Flash_Block_Manager_Base();
		void Log_File_Phase2(unsigned int command, unsigned int channel_id, unsigned int chip_id, unsigned int die_id, unsigned int superblock_id, unsigned int flow_no);
		virtual void Allocate_block_and_page_in_plane_for_user_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;		
		virtual void Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address, bool is_for_gc) = 0;
		virtual void Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses) = 0;
		virtual void Invalidate_page_in_block(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Invalidate_page_in_block_for_preconditioning(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual unsigned int Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address) = 0;
		flash_block_ID_type Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		unsigned int Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* );
		PlaneBookKeepingType* Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		bool Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if there is an ongoing gc for block_address
		bool Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if the gc request can be executed on block_address (there shouldn't be any ongoing user read/program requests targeting block_address)
		void GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		void Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		bool Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address);//Cheks if block has any ongoing program request
		void GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record


		// virtual void Allocate_block_and_page_in_superblock_for_user_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;		
		// virtual void Allocate_block_and_page_in_superblock_for_gc_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		// virtual void Allocate_block_and_page_in_superblock_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address, bool is_for_gc) = 0;
		// virtual void Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning_superblock(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& superblock_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses) = 0;
		// virtual unsigned int Get_pool_size_superblock(const NVM::FlashMemory::Physical_Page_Address& superblock_address) = 0;
		// flash_block_ID_type Get_coldest_block_id_superblock(const NVM::FlashMemory::Physical_Page_Address& superblock_address);
		// unsigned int Get_min_max_erase_difference_superblock(const NVM::FlashMemory::Physical_Page_Address& superblock_address);
		// SuperblockBookKeepingType* Get_superblock_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& superblock_address);
		// bool Block_has_ongoing_gc_wl_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if there is an ongoing gc for block_address
		// bool Can_execute_gc_wl_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if the gc request can be executed on block_address (there shouldn't be any ongoing user read/program requests targeting block_address)
		// void GC_WL_started_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		// void Read_transaction_issued_superblock(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		// void Program_transaction_serviced_superblock(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		// void Read_transaction_serviced_superblock(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		// bool Is_having_ongoing_program_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address);//Cheks if block has any ongoing program request
		// void GC_WL_finished_superblock(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record


		unsigned int Get_min_max_erase_difference_die(const NVM::FlashMemory::Physical_Page_Address& die_address);
		flash_superblock_ID_type Get_coldest_superblock_id(const NVM::FlashMemory::Physical_Page_Address& die_address);
		DieBookKeepingType* Get_die_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& die_address);
		bool Superblock_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& superblock_address);//Checks if there is an ongoing gc for superblock_address
		bool Can_execute_gc_wl_die(const NVM::FlashMemory::Physical_Page_Address& superblock_address);//Checks if the gc request can be executed on superblock_address (there shouldn't be any ongoing user read/program requests targeting superblock_address)
		void GC_WL_started_die(const NVM::FlashMemory::Physical_Page_Address& superblock_address);//Updates the superblock bookkeeping record
		void Read_transaction_issued_die(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the superblock bookkeeping record
		void Program_transaction_serviced_die(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the superblock bookkeeping record
		void Read_transaction_serviced_die(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the superblock bookkeeping record
		bool Is_having_ongoing_program_die(const NVM::FlashMemory::Physical_Page_Address& superblock_address);//Cheks if superblock has any ongoing program request
		void GC_WL_finished_die(const NVM::FlashMemory::Physical_Page_Address& superblock_address);//Updates the superblock bookkeeping record



		bool Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id);//Make the page invalid in the block bookkeeping record
	protected:
		PlaneBookKeepingType ****plane_manager;//Keeps track of plane block usage information
		// SuperblockBookKeepingType ****superblock_manager;//Keeps track of superblock block usage information
		DieBookKeepingType ***die_manager;//Keeps track of die superblock usage information

		GC_and_WL_Unit_Base *gc_and_wl_unit;
		unsigned int max_allowed_block_erase_count;
		unsigned int total_concurrent_streams_no;
		unsigned int channel_count;
		unsigned int chip_no_per_channel;
		unsigned int die_no_per_chip;
		unsigned int plane_no_per_die;
		unsigned int block_no_per_plane;
		unsigned int superblock_no_per_die;
		unsigned int block_no_per_superblock;
		unsigned int pages_no_per_block;
		void program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		// void program_transaction_issued_superblock(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void program_transaction_issued_die(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the superblock bookkeeping record
	};
}

#endif//!BLOCK_POOL_MANAGER_BASE_H
