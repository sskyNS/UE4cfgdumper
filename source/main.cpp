// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include "Console.hpp"
#include "Logger.hpp"
#include "SDL.hpp"
// Include the main libnx system header, for Switch development
#include <switch.h>
#include "dmntcht.h"
#include "ue4settings.hpp"
#include <string>
#include <sys/stat.h>
extern "C" {
#include "armadillo.h"
#include "strext.h"
}

DmntCheatProcessMetadata cheatMetadata = {0};
u64 mappings_count = 0;
MemoryInfo* memoryInfoBuffers = 0;
uint8_t utf_encoding = 0;
struct ue4Results {
	const char* iterator;
	bool isFloat = false;
	int default_value_int;
	float default_value_float;
	uint32_t offset;
	uint32_t add;
};

std::vector<ue4Results> ue4_vector;

bool isServiceRunning(const char *serviceName) {	
	Handle handle;	
	SmServiceName service_name = smEncodeName(serviceName);	
	if (R_FAILED(smRegisterService(&handle, service_name, false, 1))) 
		return true;
	else {
		svcCloseHandle(handle);	
		smUnregisterService(service_name);
		return false;
	}
}

template <typename T> T searchString(char* buffer, T string, u64 buffer_size, bool null_terminated = false, bool whole = false) {
	char* buffer_end = &buffer[buffer_size];
	size_t string_len = (std::char_traits<std::remove_pointer_t<std::remove_reference_t<T>>>::length(string) + (null_terminated ? 1 : 0)) * sizeof(std::remove_pointer_t<std::remove_reference_t<T>>);
	T string_end = &string[std::char_traits<std::remove_pointer_t<std::remove_reference_t<T>>>::length(string) + (null_terminated ? 1 : 0)];
	char* result = std::search(buffer, buffer_end, (char*)string, (char*)string_end);
	if (whole) {
		while ((uint64_t)result != (uint64_t)&buffer[buffer_size]) {
			if (!result[-1 * sizeof(std::remove_pointer_t<std::remove_reference_t<T>>)])
				return (T)result;
			result = std::search(result + string_len, buffer_end, (char*)string, (char*)string_end);
		}
	}
	else if ((uint64_t)result != (uint64_t)&buffer[buffer_size]) {
		return (T)result;
	}
	return nullptr;
}

std::string ue4_sdk = "";
bool isUE5 = false;

size_t checkAvailableHeap() {
	size_t startSize = 200 * 1024 * 1024;
	void* allocation = malloc(startSize);
	while (allocation) {
		free(allocation);
		startSize += 1024 * 1024;
		allocation = malloc(startSize);
	}
	return startSize - (1024 * 1024);
}

bool checkIfUE4game() {
	size_t i = 0;
	while (i < mappings_count) {
		if ((memoryInfoBuffers[i].perm & Perm_R) == Perm_R && (memoryInfoBuffers[i].perm & Perm_Rx) != Perm_Rx && memoryInfoBuffers[i].type == MemType_CodeStatic) {
			if (memoryInfoBuffers[i].size > 200'000'000) {
				continue;
			}
			char test_4[] = "SDK MW+EpicGames+UnrealEngine-4";
			char test_5[] = "SDK MW+EpicGames+UnrealEngine-5";
			char* buffer_c = new char[memoryInfoBuffers[i].size];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
			char* result = searchString(buffer_c, (char*)test_4, memoryInfoBuffers[i].size);
			if (result) {
				Console::Printf("%s\n", result);
				ue4_sdk = result;
				delete[] buffer_c;
				return true;
			}
			result = searchString(buffer_c, (char*)test_5, memoryInfoBuffers[i].size);
			if (result) {
				Console::Printf("%s\n", result);
				ue4_sdk = result;
				isUE5 = true;
				delete[] buffer_c;
				return true;
			}
			delete[] buffer_c;
		}
		i++;
	}
	Console::Printf("这个游戏使用的不是UE4或者UE5!\n");
	return false;
}

uint8_t testRUN() {
	size_t i = 0;
	uint8_t encoding = 0;

	size_t size = utf8_to_utf16(nullptr, (const uint8_t*)UE4settingsArray[0].description, 0);
	char16_t* utf16_string = new char16_t[size+1]();
	utf8_to_utf16((uint16_t*)utf16_string, (const uint8_t*)UE4settingsArray[0].description, size+1);

	size = utf8_to_utf32(nullptr, (const uint8_t*)UE4settingsArray[0].description, 0);
	char32_t* utf32_string = new char32_t[size+1]();
	utf8_to_utf32((uint32_t*)utf32_string, (const uint8_t*)UE4settingsArray[0].description, size+1);

	while (i < mappings_count) {
		if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
			if (memoryInfoBuffers[i].size > 200'000'000) {
				continue;
			}
			char* buffer_c = new char[memoryInfoBuffers[i].size];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
			char* result = searchString(buffer_c, (char*)UE4settingsArray[0].description, memoryInfoBuffers[i].size);
			if (result) {
				Console::Printf("编码: UTF-8\n");
				encoding = 8;
			}
			if (!encoding) {
				char16_t* result16 = searchString(buffer_c, utf16_string, memoryInfoBuffers[i].size);
				if (result16) {
					Console::Printf("编码: UTF-16\n");
					encoding = 16;
				}
			}
			if (!encoding) {
				char32_t* result32 = searchString(buffer_c, utf32_string, memoryInfoBuffers[i].size);
				if (result32) {
					Console::Printf("编码: UTF-32\n");
					encoding = 32;
				}
			}
			delete[] buffer_c;
			if (encoding) {
				delete[] utf16_string;
				delete[] utf32_string;
				return encoding;
			}
		}
		i++;
	}	
	delete[] utf16_string;
	delete[] utf32_string;
	Console::Printf("未检测到编码...");
	return encoding;
}

bool searchPointerInMappings(uint64_t string_address, const char* commandName, uint8_t type, size_t itr) {
	size_t k = 0;
	uint64_t pointer_address = 0;
	uint64_t* buffer_u = 0;

	while(k < mappings_count) {
		if ((memoryInfoBuffers[k].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[k].type == MemType_Heap) {
			if (memoryInfoBuffers[k].size > 200'000'000) {
				k++;
				continue;
			}
			buffer_u = new uint64_t[memoryInfoBuffers[k].size / sizeof(uint64_t)];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[k].addr, (void*)buffer_u, memoryInfoBuffers[k].size);
			for (size_t x = 0; x < memoryInfoBuffers[k].size / sizeof(uint64_t); x++) {
				if (buffer_u[x] == string_address) {
					pointer_address = memoryInfoBuffers[k].addr + x*8 - 8;
					break;
				}
			}
			delete[] buffer_u;
			if (pointer_address) {
				size_t l = 0;
				while (l < mappings_count) {
					if ((memoryInfoBuffers[l].perm & Perm_Rw) == Perm_Rw && (memoryInfoBuffers[l].type == MemType_CodeMutable || memoryInfoBuffers[l].type == MemType_CodeWritable)) {
						if (memoryInfoBuffers[l].size > 200'000'000) {
							continue;
						}
						buffer_u = new uint64_t[memoryInfoBuffers[l].size / sizeof(uint64_t)];
						dmntchtReadCheatProcessMemory(memoryInfoBuffers[l].addr, (void*)buffer_u, memoryInfoBuffers[l].size);
						uint64_t pointer2_address = 0;
						for (size_t x = 0; x < memoryInfoBuffers[l].size / sizeof(uint64_t); x++) {
							if (buffer_u[x] == pointer_address) {
								if (x+1 != memoryInfoBuffers[l].size / sizeof(uint64_t) 
									&& (int64_t)(buffer_u[x+1]) - buffer_u[x] > 0 
									&& (int64_t)(buffer_u[x+1]) - buffer_u[x] <= 0x98)
								{
									pointer2_address = memoryInfoBuffers[l].addr + (x+1)*8;
									Console::Printf("*主偏移: 0x%lX, 命令: %s ", pointer2_address - cheatMetadata.main_nso_extents.base, commandName);
									uint64_t pointer = 0;
									dmntchtReadCheatProcessMemory(pointer2_address, (void*)&pointer, 8);
									uint32_t main_offset = pointer2_address - cheatMetadata.main_nso_extents.base;
									if (pointer) {
										if (type == 1) {
											int data = 0;
											dmntchtReadCheatProcessMemory(pointer, (void*)&data, 4);
											Console::Printf("整数: %d\n", data);
											ue4_vector.push_back({commandName, false, data, 0.0, main_offset, 0});
										}
										else if (type == 2) {
											float data = 0;
											dmntchtReadCheatProcessMemory(pointer, (void*)&data, 4);
											Console::Printf("浮点数: %.4f\n", data);
											ue4_vector.push_back({commandName, true, 0, data, main_offset, 0});
										}
										else {
											Console::Printf("未知类型: %d\n", type);
										}
									}
									delete[] buffer_u;
									return true;
								}
							}	
						}
						delete[] buffer_u;
						if (pointer2_address) {
							k = mappings_count;
							l = mappings_count;
						}									
					}
					l++;
				}
			}
		}
		k++;
	}
	return false;
}

char* findStringInBuffer(char* buffer_c, size_t buffer_size, const char* description) {
	char* result = 0;
	if (utf_encoding == 8) {
		result = (char*)searchString(buffer_c, (char*)description, buffer_size);
	}
	else if (utf_encoding == 16) {
		size_t size = utf8_to_utf16(nullptr, (const uint8_t*)description, 0);
		char16_t* utf16_string = new char16_t[size+1]();
		utf8_to_utf16((uint16_t*)utf16_string, (const uint8_t*)description, size+1);
		result = (char*)searchString(buffer_c, utf16_string, buffer_size);
		delete[] utf16_string;
	}
	else {
		size_t size = utf8_to_utf32(nullptr, (const uint8_t*)description, 0);
		char32_t* utf32_string = new char32_t[size+1]();
		utf8_to_utf32((uint32_t*)utf32_string, (const uint8_t*)description, size+1);
		result = (char*)searchString(buffer_c, utf32_string, buffer_size);
		delete[] utf32_string;
	}
	return result;
}

void SearchFramerate() {
	uint32_t offset = 0;
	uint32_t offset2 = 0;
	for (size_t i = 0; i < mappings_count; i++) {
		if (memoryInfoBuffers[i].addr < cheatMetadata.main_nso_extents.base) {
			continue;
		}
		if (memoryInfoBuffers[i].addr >= cheatMetadata.main_nso_extents.base + cheatMetadata.main_nso_extents.size) {
			continue;
		}
		char* FFR_result = 0;
		char* FFR2_result = 0;
		char* CTS_result = 0;
		uint64_t address = 0;
		if ((memoryInfoBuffers[i].perm & Perm_R) == Perm_R && (memoryInfoBuffers[i].perm & Perm_Rx) != Perm_Rx && (memoryInfoBuffers[i].type == MemType_CodeStatic || memoryInfoBuffers[i].type == MemType_CodeReadOnly)) {
			if (memoryInfoBuffers[i].size > 200'000'000) {
				continue;
			}
			char* buffer_c = new char[memoryInfoBuffers[i].size];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
			FFR_result = (char*)searchString(buffer_c, "FixedFrameRate", memoryInfoBuffers[i].size, true, true);
			if (!FFR_result) {
				FFR_result = (char*)searchString(buffer_c, "bUseFixedFrameRate", memoryInfoBuffers[i].size, true, true);
				if (FFR_result) FFR_result = &FFR_result[4]; 
				FFR2_result = (char*)searchString(buffer_c, "bFixedFrameRate", memoryInfoBuffers[i].size, true, true);
				if (FFR2_result) FFR2_result = &FFR2_result[1]; 				
			}
			CTS_result = (char*)searchString(buffer_c, "CustomTimeStep", memoryInfoBuffers[i].size, true, true);
			address = (uint64_t)buffer_c;
			delete[] buffer_c;
		}
		else continue;
		if (!FFR_result) continue;

		ptrdiff_t FFR_diff = (uint64_t)FFR_result - address;
		uint64_t FFR_final_address = memoryInfoBuffers[i].addr + FFR_diff;
		ptrdiff_t FFR2_diff = (uint64_t)FFR2_result - address;
		uint64_t FFR2_final_address = memoryInfoBuffers[i].addr + FFR2_diff;

		ptrdiff_t CTS_diff = 0;
		uint64_t CTS_final_address = 0;
		if (CTS_result) {
			CTS_diff = (uint64_t)CTS_result - address;
			CTS_final_address = memoryInfoBuffers[i].addr + CTS_diff;
		}
		else {
			Console::Printf("未找到CustomTimeStep!\n");
		}
		for (size_t x = 0; x < mappings_count; x++) {
			if ((memoryInfoBuffers[x].perm & Perm_Rx) != Perm_Rx && (memoryInfoBuffers[x].type == MemType_CodeMutable || memoryInfoBuffers[x].type == MemType_CodeWritable)) {
				if (memoryInfoBuffers[x].addr < cheatMetadata.main_nso_extents.base) {
					continue;
				}
				if (memoryInfoBuffers[x].addr >= cheatMetadata.main_nso_extents.base + cheatMetadata.main_nso_extents.size) {
					continue;
				}
				if (memoryInfoBuffers[x].size > 200'000'000) {
					continue;
				}
				uint64_t* buffer = new uint64_t[memoryInfoBuffers[x].size / sizeof(uint64_t)];
				dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr, (void*)buffer, memoryInfoBuffers[x].size);
				size_t itr = 0;
				while (itr < (memoryInfoBuffers[x].size / sizeof(uint64_t))) {
					if (buffer[itr] == FFR_final_address || buffer[itr] == FFR2_final_address) {
						uint32_t offset_temp = 0;
						dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + (isUE5 ? 0x38 : 0x24), (void*)&offset_temp, 4);
						if (!offset_temp && !isUE5) dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + 0x28, (void*)&offset_temp, 4);
						if (offset_temp < 0x600 || offset_temp > 0x1000) {
							if (isUE5) {
								offset_temp = 0;
								dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + 0x32, (void*)&offset_temp, 2);
								if (offset_temp < 0x600 || offset_temp > 0x1000) {
									itr++;
									continue;
								}
							}
							else {
								itr++;
								continue;
							}
						}
						offset = offset_temp;
						break;
					}
					itr++;
				}
				if (!offset) {
					delete[] buffer;
					continue;
				}

				static bool printed = false;
				if (!printed) {
					Console::Printf("FixedFrameRate的偏移: 0x%x\n", offset);
					printed = true;
				}
				if (CTS_final_address) {
					while (itr < (memoryInfoBuffers[x].size / sizeof(uint64_t))) {
						if (buffer[itr] == CTS_final_address) {
							uint32_t offset_temp = 0;
							dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + (isUE5 ? 0x38 : 0x24), (void*)&offset_temp, 4);
							if (!offset_temp && !isUE5) dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + 0x28, (void*)&offset_temp, 4);
							if (offset_temp < 0x600 || offset_temp > 0x1000) {
								if (isUE5) {
									offset_temp = 0;
									dmntchtReadCheatProcessMemory(memoryInfoBuffers[x].addr + itr*8 + 0x32, (void*)&offset_temp, 2);
									if (offset_temp < 0x600 || offset_temp > 0x1000) {
										itr++;
										continue;
									}
								}
								else {
									itr++;
									continue;
								}
							}
							offset2 = offset_temp;
							break;
						}
						itr++;
					}
				}
				delete[] buffer;
				if (offset2) {
					Console::Printf("CustomTimeStep的偏移: 0x%x\n", offset2);
					break;
				}
			}
		}
	}
	if (!offset) {
		Console::Printf("未找到FixedFrameRate字符串!");
		if (!isUE5) Console::Printf(" 在较旧的Unreal Engine 4游戏中需要不同的方法。");
		Console::Printf("\n");
	}
	for (size_t y = 0; y < mappings_count; y++) {
		if (memoryInfoBuffers[y].addr != cheatMetadata.main_nso_extents.base) {
			continue;
		}
		uint8_t* buffer_two = new uint8_t[memoryInfoBuffers[y].size];
		dmntchtReadCheatProcessMemory(memoryInfoBuffers[y].addr, (void*)buffer_two, memoryInfoBuffers[y].size);
		while(true) {
			uint8_t pattern_number = 1;
			// A8 99 99 52 88 B9 A7 72 01 10 2C 1E 00 01 27 1E 60 01 80 52
			uint8_t pattern[] = {	0xA8, 0x99, 0x99, 0x52,		//mov  w8, #0xcccd
									0x88, 0xB9, 0xA7, 0x72, 	//movk w8, #0x3dcc, lsl #16
									0x01, 0x10, 0x2C, 0x1E, 	//fmov s1, #0.5
									0x00, 0x01, 0x27, 0x1E, 	//fmov s0, w8
									0x60, 0x01, 0x80, 0x52};	//mov  w0, #0xb
			// F7 37 68 22 40 39
			uint8_t pattern_2[] = {	/**/  /**/  0xF7, 0x37, 	//tbnz w9, #0x1E, *
									0x68, 0x22, 0x40, 0x39};	//ldrb w8, [x19, #8]
			// 08 20 40 39 08 01 20 37
			uint8_t pattern_3[] = {	0x08, 0x20, 0x40, 0x39, 	//ldrb w8, [x0, #8]
									0x08, 0x01, 0x20, 0x37};	//tbnz w8, #4, #PC+0x20
			// 68 0A 40 B9 88 03 20 37
			uint8_t pattern_4[] = {	0x68, 0x0A, 0x40, 0xB9, 	//ldr  w8, [x19, #8]
									0x88, 0x03, 0x20, 0x37};	//tbnz w8, #4, #PC+0x70
			// 29 02 F0 36 09 00 A8 52
			uint8_t pattern_5[] = {	0x29, 0x02, 0xF0, 0x36, 	//tbz w9, #0x1e, #0x44
									0x09, 0x00, 0xA8, 0x52};	//MOV W9, #0x40000000

			// 09 09 00 B9 68 22 40 39 08 01 20 37
			uint8_t pattern_6[] = {	0x09, 0x09, 0x00, 0xB9,     //str w9, [x8, #8]
									0x68, 0x22, 0x40, 0x39, 	//ldrb w8, [x19, #8]
									0x08, 0x01, 0x20, 0x37};	//tbnz w8, #4, #PC+0x20
			
			static bool skip_pattern[7] = {0};

			auto it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern, &pattern[sizeof(pattern)]); //Default constructor pattern
			if (it == &buffer_two[memoryInfoBuffers[y].size] && !skip_pattern[2]) {
				it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern_2, &pattern_2[sizeof(pattern_2)]); //Deconstructor pattern
				pattern_number = 2;
			}
			if (it == &buffer_two[memoryInfoBuffers[y].size] && !skip_pattern[3]) {
				it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern_3, &pattern_3[sizeof(pattern_3)]); //Deconstructor pattern 2
				pattern_number = 3;
			}
			if (it == &buffer_two[memoryInfoBuffers[y].size] && !skip_pattern[4]) {
				it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern_4, &pattern_4[sizeof(pattern_4)]); //Deconstructor pattern 3
				pattern_number = 4;
			}
			if (it == &buffer_two[memoryInfoBuffers[y].size] && !skip_pattern[5]) {
				it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern_5, &pattern_5[sizeof(pattern_5)]); //Deconstructor pattern 3
				pattern_number = 5;
			}
			if (it == &buffer_two[memoryInfoBuffers[y].size] && !skip_pattern[6]) {
				it = std::search(buffer_two, &buffer_two[memoryInfoBuffers[y].size], pattern_6, &pattern_6[sizeof(pattern_6)]); //Deconstructor pattern 3
				pattern_number = 6;
			}
			if (it != &buffer_two[memoryInfoBuffers[y].size]) {
				auto distance = std::distance(buffer_two, it);
				uint32_t first_instruction = *(uint32_t*)&buffer_two[distance-(8 * 4)];
				uint32_t second_instruction = *(uint32_t*)&buffer_two[distance-(7 * 4)];
				uint32_t second_alt_instruction = 0;
				switch(pattern_number) {
					case 1:
						distance = distance-(8 * 4);
						break;
					case 2:
						first_instruction = *(uint32_t*)&buffer_two[(distance-2) + (3 * 4)];
						second_instruction = *(uint32_t*)&buffer_two[(distance-2) + (5 * 4)];
						second_alt_instruction = *(uint32_t*)&buffer_two[(distance-2) + (4 * 4)];
						distance = (distance-2) + (3 * 4);
						break;
					case 3:
						first_instruction = *(uint32_t*)&buffer_two[distance + (2 * 4)];
						second_instruction = *(uint32_t*)&buffer_two[distance + (3 * 4)];
						distance += 2 * 4;
						break;
					case 4:
						first_instruction = *(uint32_t*)&buffer_two[distance + (2 * 4)];
						second_instruction = *(uint32_t*)&buffer_two[distance + (4 * 4)];
						distance += 2 * 4;
						break;
					case 5:
						first_instruction = *(uint32_t*)&buffer_two[distance + (6 * 4)];
						second_instruction = *(uint32_t*)&buffer_two[distance + (8 * 4)];
						distance += 6 * 4;
						break;
					case 6:
						first_instruction = *(uint32_t*)&buffer_two[distance + (3 * 4)];
						second_instruction = *(uint32_t*)&buffer_two[distance + (4 * 4)];
						distance += 3 * 4;
						break;
				}
				ad_insn *insn = NULL;
				uint32_t main_offset = 0;
				ArmadilloDisassemble(first_instruction, distance, &insn);
				if (insn -> instr_id == AD_INSTR_ADRP) {
					main_offset = insn -> operands[1].op_imm.bits;
					ArmadilloDone(&insn);
					ArmadilloDisassemble(second_instruction, distance * 4, &insn);
					if (insn -> num_operands == 2) {
						ArmadilloDone(&insn);
						ArmadilloDisassemble(second_alt_instruction, distance * 4, &insn);
					}
					if ((insn -> instr_id == AD_INSTR_LDR || insn -> instr_id == AD_INSTR_STR) && insn -> num_operands == 3 && insn -> operands[2].type == AD_OP_IMM) {
						main_offset += insn -> operands[2].op_imm.bits;
						ArmadilloDone(&insn);
						uint64_t GameEngine_ptr = 0;
						switch(pattern_number) {
							case 1:
								dmntchtReadCheatProcessMemory(cheatMetadata.main_nso_extents.base + main_offset, (void*)&GameEngine_ptr, 8);
								break;
							case 2:
							case 3:
							case 4:
							case 5:
							case 6:
								GameEngine_ptr = cheatMetadata.main_nso_extents.base + main_offset;
								break;
						}
						Console::Printf("GameEngine指针的主偏移: 0x%lX\n", GameEngine_ptr - cheatMetadata.main_nso_extents.base);
						uint64_t GameEngine = 0;
						dmntchtReadCheatProcessMemory(GameEngine_ptr, (void*)&GameEngine, 8);
						if (offset) {
							uint32_t bitflags = 0;
							dmntchtReadCheatProcessMemory(GameEngine + (offset - 4), (void*)&bitflags, 4);
							Console::Printf("位标志: 0x%x\n", bitflags);
							Console::Printf("bUseFixedFrameRate布尔值: %x\n", (bool)(bitflags & 0x40));
							Console::Printf("bSmoothFrameRate布尔值: %x\n", (bool)(bitflags & 0x20));
							float FixedFrameRate = 0;
							dmntchtReadCheatProcessMemory(GameEngine + offset, (void*)&FixedFrameRate, 4);
							Console::Printf("FixedFrameRate: %.4f\n", FixedFrameRate);
							ue4_vector.push_back({"FixedFrameRate", true, (int)bitflags, FixedFrameRate, (uint32_t)(GameEngine_ptr - cheatMetadata.main_nso_extents.base), offset - 4});
						}
						if (offset2) {
							int CustomTimeStep = 0;
							dmntchtReadCheatProcessMemory(GameEngine + offset2, (void*)&CustomTimeStep, 4);
							Console::Printf("CustomTimeStep: 0x%x\n", CustomTimeStep);
							ue4_vector.push_back({"CustomTimeStep", false, CustomTimeStep, 0, (uint32_t)(GameEngine_ptr - cheatMetadata.main_nso_extents.base), offset2});
						}
					}
					else {
						Console::Printf("第二条指令不是预期的LDR或STR! %s\n", insn -> decoded);
						ArmadilloDone(&insn);
						skip_pattern[pattern_number] = true;
						continue;
					}
				}
				else {
					Console::Printf("第一条指令不是ADRP! %s\n", insn -> decoded);
					ArmadilloDone(&insn);
					skip_pattern[pattern_number] = true;
					continue;
				}
			}
			else Console::Printf("未找到GameEngine结构的模式!\n");
			delete[] buffer_two;
			return;
		}
	}
}

void searchDescriptionsInRAM() {
	size_t i = 0;
	bool* UE4checkedList = new bool[UE4settingsArray.size()]();
	size_t ue4checkedCount = 0;
	Console::Printf("映射 %ld / %ld\r", i+1, mappings_count);
	while (i < mappings_count) {
		if (ue4checkedCount == UE4settingsArray.size()) {
			return;
		}
		if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
			if (memoryInfoBuffers[i].size > 200'000'000) {
				i++;
				continue;
			}
			char* buffer_c = new char[memoryInfoBuffers[i].size];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
			char* result = 0;
			for (size_t itr = 0; itr < UE4settingsArray.size(); itr++) {
				if (UE4checkedList[itr]) {
					continue;
				}
				result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE4settingsArray[itr].description);
				if (result) {
					ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
					uint64_t string_address = memoryInfoBuffers[i].addr + diff;
					if (searchPointerInMappings(string_address, UE4settingsArray[itr].commandName, UE4settingsArray[itr].type, itr)) {
						Console::Printf("映射 %ld / %ld\r", i+1, mappings_count);
						ue4checkedCount += 1;
						UE4checkedList[itr] = true;
					}
				}
			}
			delete[] buffer_c;
		}
		i++;
	}
	Console::Printf("                                                \n");
	for (size_t x = 0; x < UE4settingsArray.size(); x++) {
		if (!UE4checkedList[x]) {
			if (isUE5) {
				if (UE5DeprecatedUE4Settings.contains(UE4settingsArray[x].commandName)) {
					continue;
				}
			}
			if (UE4alternativeDescriptions1.contains(UE4settingsArray[x].commandName)) {
				i = 0;
				while (i < mappings_count) {
					if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
						if (memoryInfoBuffers[i].size > 200'000'000) {
							i++;
							continue;
						}
						char* buffer_c = new char[memoryInfoBuffers[i].size];
						dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
						char* result = 0;
						result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE4alternativeDescriptions1[UE4settingsArray[x].commandName].c_str());
						if (result) {
							ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
							uint64_t string_address = memoryInfoBuffers[i].addr + diff;
							if (searchPointerInMappings(string_address, UE4settingsArray[x].commandName, UE4settingsArray[x].type, x)) {
								UE4checkedList[x] = true;
								i = mappings_count;
							}
						}
						delete[] buffer_c;
					}
					i++;
				}
			}
		}
		if (isUE5 && !UE4checkedList[x]) {
			if (UE4toUE5alternativeDescriptions1.contains(UE4settingsArray[x].commandName)) {
				i = 0;
				while (i < mappings_count) {
					if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
						if (memoryInfoBuffers[i].size > 200'000'000) {
							i++;
							continue;
						}
						char* buffer_c = new char[memoryInfoBuffers[i].size];
						dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
						char* result = 0;
						result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE4toUE5alternativeDescriptions1[UE4settingsArray[x].commandName].c_str());
						if (result) {
							ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
							uint64_t string_address = memoryInfoBuffers[i].addr + diff;
							if (searchPointerInMappings(string_address, UE4settingsArray[x].commandName, UE4settingsArray[x].type, x)) {
								UE4checkedList[x] = true;
								i = mappings_count;
							}
						}
						delete[] buffer_c;
					}
					i++;
				}
			}
		}
		if (isUE5 && !UE4checkedList[x]) {
			if (UE4toUE5alternativeDescriptions2.contains(UE4settingsArray[x].commandName)) {
				i = 0;
				while (i < mappings_count) {
					if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
						if (memoryInfoBuffers[i].size > 200'000'000) {
							i++;
							continue;
						}
						char* buffer_c = new char[memoryInfoBuffers[i].size];
						dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
						char* result = 0;
						result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE4toUE5alternativeDescriptions2[UE4settingsArray[x].commandName].c_str());
						if (result) {
							ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
							uint64_t string_address = memoryInfoBuffers[i].addr + diff;
							if (searchPointerInMappings(string_address, UE4settingsArray[x].commandName, UE4settingsArray[x].type, x)) {
								UE4checkedList[x] = true;
								i = mappings_count;
							}
						}
						delete[] buffer_c;
					}
					i++;
				}
			}
		}
	}
	for (size_t x = 0; x < UE4settingsArray.size(); x++) {
		if (UE4checkedList[x])
			continue;
		if (UE4alternativeDescriptions1.contains(UE4settingsArray[x].commandName) && !UE4toUE5alternativeDescriptions1.contains(UE4settingsArray[x].commandName) && !UE4toUE5alternativeDescriptions2.contains(UE4settingsArray[x].commandName)) {
			Console::Printf("!: %s 即使使用UE4替代描述也未找到!\n", UE4settingsArray[x].commandName);
			continue;
		}
		if (isUE5 && UE4toUE5alternativeDescriptions1.contains(UE4settingsArray[x].commandName) && !UE4toUE5alternativeDescriptions2.contains(UE4settingsArray[x].commandName)) {
			Console::Printf("!: %s 即使使用UE5替代描述1也未找到!\n", UE4settingsArray[x].commandName);
			continue;
		}
		if (isUE5 && UE4toUE5alternativeDescriptions2.contains(UE4settingsArray[x].commandName)) {
			Console::Printf("!: %s 即使使用UE5替代描述2也未找到!\n", UE4settingsArray[x].commandName);
			continue;
		}
		if (isUE5 && UE5DeprecatedUE4Settings.contains(UE4settingsArray[x].commandName)) {
			Console::Printf("!: %s 在UE5中已弃用!", UE4settingsArray[x].commandName);
			if (UE5DeprecatedUE4Settings[UE4settingsArray[x].commandName].compare("")) {
				Console::Printf(" %s", UE5DeprecatedUE4Settings[UE4settingsArray[x].commandName].c_str());
			}
			Console::Printf("\n");
			continue;
		}
		Console::Printf("!: %s 未找到!\n", UE4settingsArray[x].commandName);
	}
	delete[] UE4checkedList;
}

void searchDescriptionsInRAM_UE5() {
	size_t i = 0;
	bool* UE5checkedList = new bool[UE5settingsArray.size()]();
	size_t ue5checkedCount = 0;
	Console::Printf("\n---\n查找UE5特定设置...\n---\n");
	Console::Printf("映射 %ld / %ld\r", i+1, mappings_count);
	while (i < mappings_count) {
		if (ue5checkedCount == UE5settingsArray.size()) {
			return;
		}
		if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
			if (memoryInfoBuffers[i].size > 200'000'000) {
				i++;
				continue;
			}
			char* buffer_c = new char[memoryInfoBuffers[i].size];
			dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
			char* result = 0;
			for (size_t itr = 0; itr < UE5settingsArray.size(); itr++) {
				if (UE5checkedList[itr]) {
					continue;
				}
				result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE5settingsArray[itr].description);
				if (result) {
					ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
					uint64_t string_address = memoryInfoBuffers[i].addr + diff;
					if (searchPointerInMappings(string_address, UE5settingsArray[itr].commandName, UE5settingsArray[itr].type, itr)) {
						Console::Printf("映射 %ld / %ld\r", i+1, mappings_count);
						ue5checkedCount += 1;
						UE5checkedList[itr] = true;
					}
				}
			}
			delete[] buffer_c;
		}
		i++;
	}
	Console::Printf("                                                \n");
	for (size_t x = 0; x < UE5settingsArray.size(); x++) {
		if (UE5alternativeDescriptions1.contains(UE5settingsArray[x].commandName)) {
			i = 0;
			while (i < mappings_count) {
				if ((memoryInfoBuffers[i].perm & Perm_Rw) == Perm_Rw && memoryInfoBuffers[i].type == MemType_Heap) {
					if (memoryInfoBuffers[i].size > 200'000'000) {
						i++;
						continue;
					}
					char* buffer_c = new char[memoryInfoBuffers[i].size];
					dmntchtReadCheatProcessMemory(memoryInfoBuffers[i].addr, (void*)buffer_c, memoryInfoBuffers[i].size);
					char* result = 0;
					result = findStringInBuffer(buffer_c, memoryInfoBuffers[i].size, UE5alternativeDescriptions1[UE5settingsArray[x].commandName].c_str());
					if (result) {
						ptrdiff_t diff = (uint64_t)result - (uint64_t)buffer_c;
						uint64_t string_address = memoryInfoBuffers[i].addr + diff;
						if (searchPointerInMappings(string_address, UE5settingsArray[x].commandName, UE5settingsArray[x].type, x)) {
							UE5checkedList[x] = true;
							i = mappings_count;
						}
					}
					delete[] buffer_c;
				}
				i++;
			}
		}
		else if (!UE5checkedList[x]) {
			Console::Printf("!: %s 未找到!\n", UE5settingsArray[x].commandName);
		}
	}
	for (size_t x = 0; x < UE5settingsArray.size(); x++) {
		if (UE5checkedList[x])
			continue;
		if (UE5alternativeDescriptions1.contains(UE5settingsArray[x].commandName)) {
			Console::Printf("!: %s 即使使用替代描述也未找到!\n", UE5settingsArray[x].commandName);
			continue;
		}
		Console::Printf("!: %s 未找到!\n", UE5settingsArray[x].commandName);
	}
	delete[] UE5checkedList;
}

void dumpAsCheats() {
	uint64_t BID = 0;
	memcpy(&BID, &(cheatMetadata.main_nso_build_id), 8);
	char path[128] = "";
	mkdir("sdmc:/switch/UE4cfgdumper/", 777);
	snprintf(path, sizeof(path), "sdmc:/switch/UE4cfgdumper/%016lX/", cheatMetadata.title_id);
	mkdir(path, 777);
	snprintf(path, sizeof(path), "sdmc:/switch/UE4cfgdumper/%016lX/%016lX.txt", cheatMetadata.title_id, __builtin_bswap64(BID));
	FILE* text_file = fopen(path, "w");
	if (!text_file) {
		Console::Printf("无法创建作弊文件!");
		return;
	}
	for (size_t i = 0; i < ue4_vector.size(); i++) {
		fwrite("[", 1, 1, text_file);
		fwrite(ue4_vector[i].iterator, strlen(ue4_vector[i].iterator), 1, text_file);
		fwrite("]\n", 2, 1, text_file);
		fwrite("580F0000 ", 9, 1, text_file);
		char temp[24] = "";
		snprintf(temp, sizeof(temp), "%08X\n", ue4_vector[i].offset);
		fwrite(temp, 9, 1, text_file);
		if (ue4_vector[i].add) {
			fwrite("780F0000 ", 9, 1, text_file);
			snprintf(temp, sizeof(temp), "%08X\n", ue4_vector[i].add);
			fwrite(temp, 9, 1, text_file);
		}
		if (!strcmp("CustomTimeStep", ue4_vector[i].iterator)) {
			fwrite("640F0000 00000000 ", 18, 1, text_file);
			snprintf(temp, sizeof(temp), "%08X\n\n", ue4_vector[i].default_value_int);
			fwrite(temp, 10, 1, text_file);
		}
		else {
			fwrite("680F0000 ", 9, 1, text_file);
			if (ue4_vector[i].isFloat) {
				if (!ue4_vector[i].default_value_int) {
					int temp_val = 0;
					memcpy(&temp_val, &ue4_vector[i].default_value_float, 4);
					snprintf(temp, sizeof(temp), "%08X %08X\n\n", temp_val, temp_val);
				}
				else {
					int temp_val = 0;
					memcpy(&temp_val, &ue4_vector[i].default_value_float, 4);
					snprintf(temp, sizeof(temp), "%08X %08X\n\n", temp_val, ue4_vector[i].default_value_int);
				}
			}
			else {
				snprintf(temp, sizeof(temp), "%08X %08X\n\n", ue4_vector[i].default_value_int, ue4_vector[i].default_value_int);
			}
			fwrite(temp, strlen(temp), 1, text_file);
		}
	}
	fclose(text_file);
	Console::Printf("作弊文件已导出到:\n");
	Console::Printf(path);
	Console::Printf("\n");
}

void dumpAsLog() {
	uint64_t BID = 0;
	memcpy(&BID, &(cheatMetadata.main_nso_build_id), 8);
	char path[128] = "";
	mkdir("sdmc:/switch/UE4cfgdumper/", 777);
	snprintf(path, sizeof(path), "sdmc:/switch/UE4cfgdumper/%016lX/", cheatMetadata.title_id);
	mkdir(path, 777);
	snprintf(path, sizeof(path), "sdmc:/switch/UE4cfgdumper/%016lX/%016lX.log", cheatMetadata.title_id, __builtin_bswap64(BID));	
	FILE* text_file = fopen(path, "w");
	if (!text_file) {
		Console::Printf("无法创建日志文件!");
		return;
	}
	fwrite(ue4_sdk.c_str(), ue4_sdk.size(), 1, text_file);
	fwrite("\n\n", 2, 1, text_file);
	for (size_t i = 0; i < ue4_vector.size(); i++) {
		fwrite(ue4_vector[i].iterator, strlen(ue4_vector[i].iterator), 1, text_file);
		char temp[128] = "";
		snprintf(temp, sizeof(temp), ", 主偏移: 0x%X + 0x%X, ", ue4_vector[i].offset, ue4_vector[i].add);
		fwrite(temp, strlen(temp), 1, text_file);
		if (!strcmp("FixedFrameRate", ue4_vector[i].iterator)) {
			snprintf(temp, sizeof(temp), "标志: 0x%x, bUseFixedFrameRate: %d, bSmoothFrameRate: %d, ", ue4_vector[i].default_value_int, (bool)(ue4_vector[i].default_value_int & 0x40), (bool)(ue4_vector[i].default_value_int & 0x20));
			fwrite(temp, strlen(temp), 1, text_file);
		}
		if (ue4_vector[i].isFloat) {
			int temp_val = 0;
			memcpy(&temp_val, &ue4_vector[i].default_value_float, 4);
			snprintf(temp, sizeof(temp), "类型: 浮点数 %.5f / 0x%X\n", ue4_vector[i].default_value_float, temp_val);
		}
		else {
			snprintf(temp, sizeof(temp), "类型: 整数 %d / 0x%X\n", ue4_vector[i].default_value_int, ue4_vector[i].default_value_int);
		}
		fwrite(temp, strlen(temp), 1, text_file);
	}
	fclose(text_file);
	Console::Printf("日志文件已导出到:\n");
	Console::Printf(path);	
	Console::Printf("\n");
}

int main(int argc, char* argv[])
{
    // 初始化SDL系统
    Logger::Initialize();

    if (!SDL::Initialize("UE4cfgdumper", 1280, 720)) {
        Logger::Log("初始化SDL错误: %s", SDL::GetErrorString());
        return 1;
    }

    if (!SDL::Text::Initialize()) {
        Logger::Log("初始化SDL::Text错误: %s", SDL::GetErrorString());
        SDL::Exit();
        return 1;
    }

    SDL::Color blueColor;
    blueColor.Raw = 0x00FFFFFF;
    SDL::Text::AddColorCharacter(L'^', blueColor); // 蓝色
    
    SDL::Color greenColor;
    greenColor.Raw = 0x00FF00FF;
    SDL::Text::AddColorCharacter(L'*', greenColor); // 绿色  
    
    SDL::Color redColor;
    redColor.Raw = 0xFF0000FF;
    SDL::Text::AddColorCharacter(L'>', redColor); // 红色
    
    SDL::Color yellowColor;
    yellowColor.Raw = 0xFFFF00FF;
    SDL::Text::AddColorCharacter(L'!', yellowColor); // 黄色

    Console::SetFontSize(20);
    Console::SetMaxLineCount(27);

	// Configure our supported input layout: a single player with standard controller styles
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);

	// Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
	PadState pad;
	padInitializeDefault(&pad);

	bool error = false;
	if (!isServiceRunning("dmnt:cht")) {
		Console::Printf(">未检测到DMNT:CHT!>\n");
		error = true;
	}
	pmdmntInitialize();
	uint64_t PID = 0;
	if (R_FAILED(pmdmntGetApplicationProcessId(&PID))) {
		Console::Printf(">游戏未初始化.>\n");
		error = true;
	}
	pmdmntExit();
	if (error) {
		Console::Printf("按 + 退出.");
		while (appletMainLoop()) {   
			// Scan the gamepad. This should be done once for each frame
			padUpdate(&pad);

			// padGetButtonsDown returns the set of buttons that have been
			// newly pressed in this frame compared to the previous one
			u64 kDown = padGetButtonsDown(&pad);

			if (kDown & HidNpadButton_Plus)
				break; // break in order to return to hbmenu

		}
	}
	else {
		pmdmntExit();
		size_t availableHeap = checkAvailableHeap();
		Console::Printf("可用堆: *%ld* MB\n", (availableHeap / (1024 * 1024)));
		dmntchtInitialize();
		bool hasCheatProcess = false;
		dmntchtHasCheatProcess(&hasCheatProcess);
		if (!hasCheatProcess) {
			dmntchtForceOpenCheatProcess();
		}

		Result res = dmntchtGetCheatProcessMetadata(&cheatMetadata);
		if (res)
			Console::Printf(">dmntchtGetCheatProcessMetadata 返回: 0x%x>\n", res);

		res = dmntchtGetCheatProcessMappingCount(&mappings_count);
		if (res)
			Console::Printf(">dmntchtGetCheatProcessMappingCount 返回: 0x%x>\n", res);
		else Console::Printf("映射数量: *%ld*\n", mappings_count);

		memoryInfoBuffers = new MemoryInfo[mappings_count];

		res = dmntchtGetCheatProcessMappings(memoryInfoBuffers, mappings_count, 0, &mappings_count);
		if (res)
			Console::Printf(">dmntchtGetCheatProcessMappings 返回: 0x%x>\n", res);

		//Test run

		if (checkIfUE4game() && (utf_encoding = testRUN())) {
			bool FullScan = true;
			Console::Printf("\n^----------^\n按 A 进行完整扫描\n");
			Console::Printf("按 X 进行基础扫描(排除FixedFrameRate和CustomTimeStep)\n");
			Console::Printf("按 + 退出程序\n\n");
			while (appletMainLoop()) {   
				padUpdate(&pad);

				u64 kDown = padGetButtonsDown(&pad);

				if (kDown & HidNpadButton_A)
					break;

				if (kDown & HidNpadButton_Plus) {
					dmntchtExit();
                    SDL::Text::Exit();
                    SDL::Exit();
					return 0;
				}
				
				if (kDown & HidNpadButton_X) {
					FullScan = false;
					break;
				}

			}
			Console::Printf("正在搜索RAM...\n\n");
			appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
			searchDescriptionsInRAM();
			if (isUE5) searchDescriptionsInRAM_UE5();
			Console::Printf("                                                \n");
			if (FullScan) SearchFramerate();
			Console::Printf("\n^---------------------------------------------^\n\n");
			Console::Printf("搜索完成!\n");
			dumpAsCheats();
			dumpAsLog();
			appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
		}
		
		delete[] memoryInfoBuffers;
		dmntchtExit();
		Console::Printf("按 + 退出.");
		while (appletMainLoop()) {   
			// Scan the gamepad. This should be done once for each frame
			padUpdate(&pad);

			// padGetButtonsDown returns the set of buttons that have been
			// newly pressed in this frame compared to the previous one
			u64 kDown = padGetButtonsDown(&pad);

			if (kDown & HidNpadButton_Plus)
				break; // break in order to return to hbmenu
		}
	}

	// Deinitialize and clean up resources
	ue4_vector.clear();
    SDL::Text::Exit();
    SDL::Exit();
	return 0;
}