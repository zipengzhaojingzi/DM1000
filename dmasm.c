/*******************************************************************************



*******************************************************************************/

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned char BYTE;
typedef long BOOL;

#define FALSE 0
#define TRUE 1

#define MAX_LINE_LENGTH 256 // 一行代码最多可以有多少个字符
#define MAX_LINE_COUNT 1024 // 源代码文件最多可以有多少行

#define MAX_SYMBOL_LENGTH 64 // 一个符号最多可以包含多少个字符
#define MAX_SYMBOL_COUNT 256 // 源代码文件中最多可以包含多少个符号

#ifndef MAX_PATH
#define MAX_PATH 256 // 文件路径最多可以包含多少个字符
#endif

// 代码行数据库。记录源代码文件中所有代码行的信息（包括注释行、空行）
struct LINE_RECORD
{
	char line_string[MAX_LINE_LENGTH]; // 代码行的内容
	unsigned long line_num;			   // 行号
	unsigned long address;			   // 此行代码转换的机器码在映像文件中的地址（偏移）
	int machine_code_count;			   // 此行代码转换的机器码的字节数量
	unsigned long flag;				   // 代码行标志位，32位
};
struct LINE_RECORD line_database[MAX_LINE_COUNT] = {0};
int line_count = 0;
int machine_code_line_count = 0; // 记录产生了机器码的代码行的数量

// 在此定义所有的代码行标志位。注意，代码行标志位是按位或的关系。
#define LF_INSTRUCTION 0x00000001 // 代码行标志位的最低位是1，表示此行是一条指令，否则表示此行是数据

// 重定位表。如果指令中使用了标号、变量名等符号，在第一次扫描时无法确定他们的地址，需要
// 在第二次扫描时进行重定位。
struct REALLOCATE
{
	unsigned long address;				 // 第一次扫描时，在这里记录了需要重定位的机器码的地址（偏移），
										 // 第二次扫描时，根据符号的地址进行重定位。
	char symbol_name[MAX_SYMBOL_LENGTH]; // 需要重定位的符号名称。
	int line_num;						 // 行号。
};
struct REALLOCATE reallocate_table[MAX_LINE_COUNT] = {0};
int reallocate_count = 0;

// 符号表。符号包括源代码中的标号、变量名。
struct SYMBOL
{
	char name[MAX_SYMBOL_LENGTH]; // 符号名称
	unsigned long address;		  // 符号表示的地址。重定位时需要用到。
	int machine_code_count;		  // 符号生成的机器码的数量（以字节为单位）。
	int line_num;				  // 行号
	int ref_count;				  // 引用计数
};
struct SYMBOL symbol_table[MAX_SYMBOL_COUNT] = {0};
int symbol_count = 0;

// 汇编过程的状态
enum
{
	AS_BEGIN // 起始状态。在遇到代码段名称前，处于此状态
	,
	AS_TEXT // 正在处理代码段。遇到代码段名称后，遇到数据的名称前，处于此状态
	,
	AS_DATA // 正在处理数据段。遇到数据段名称后，处于此状态
};
unsigned long assembler_state = AS_BEGIN;

//
// 在下面定义所有的关键字
//

// 段名称
const char *code_section_keyword = ".text"; // 代码段标志
char const *data_section_keyword = ".data"; // 数据段标志

// 指令名称
const char *mov_instruction_keyword = "mov";
const char *jmp_instruction_keyword = "jmp";
const char *add_instruction_keyword = "add";
const char *adc_instruction_keyword = "adc";
const char *sub_instruction_keyword = "sub";
const char *sbb_instruction_keyword = "sbb";
const char *and_instruction_keyword = "and";
const char *or_instruction_keyword = "or";
const char *read_instruction_keyword = "read";
const char *write_instruction_keyword = "write";
const char *jc_instruction_keyword = "jc";
const char *jz_instruction_keyword = "jz";
const char *call_instruction_keyword = "call";
const char *in_instruction_keyword = "in";
const char *out_instruction_keyword = "out";
const char *ret_instruction_keyword = "ret";
const char *shr_instruction_keyword = "shr";
const char *shl_instruction_keyword = "shl";
const char *rcr_instruction_keyword = "rcr";
const char *rcl_instruction_keyword = "rcl";
const char *not_instruction_keyword = "not";
const char *iret_instruction_keyword = "iret";
const char *nop_instruction_keyword = "nop";
const char *int_instruction_keyword = "int";
const char *lea_instruction_keyword = "lea";
const char *shlnum_instruction_keyword = "shlnum";
const char *shrnum_instruction_keyword = "shrnum";
const char *rcrnum_instruction_keyword = "rcrnum";
const char *rclnum_instruction_keyword = "rclnum";

// 通用寄存器名称
const char *r0_register_keyword = "r0";
const char *r1_register_keyword = "r1";
const char *r2_register_keyword = "r2";
const char *r3_register_keyword = "r3";
const char *r0_register_indirect_keyword = "[r0]";
const char *r1_register_indirect_keyword = "[r1]";
const char *r2_register_indirect_keyword = "[r2]";
const char *r3_register_indirect_keyword = "[r3]";
const char *a_register_keyword = "a";
const char *sp_register_keyword = "sp";

const char *delimit_char = "\n\t\r ";		 // 需要忽略的空白字符
const char *delimit_char_comma = "\n\t\r, "; // 需要忽略的空白字符，包括英文逗号

// 汇编产生的机器码
#define MAX_MACHINE_CODE 1024
BYTE machine_code[MAX_MACHINE_CODE];
unsigned long machine_code_address = 0;
unsigned long machine_code_old_address = 0;

const char *assembly_file_name = NULL; // 汇编文件路径
const char *target_file_name = NULL;   // 目标文件路径
const char *list_file_name = NULL;	   // 列表文件路径
const char *dbg_file_name = NULL;	   // 调试信息文件路径

const unsigned long dbg_file_magic = 58;
const unsigned long dbg_file_version = 1;

// 输出汇编过程中发现的语法错误信息
void error_msg(const char *error_msg, int line_num)
{
	if (line_num >= 1)
	{
		printf("%s:%d: error: %s\n", assembly_file_name, line_num, error_msg);
	}
	else
	{
		printf("%s: error: %s\n", assembly_file_name, error_msg);
	}

	exit(1);
}

char formated_msg[1024]; // 将格式化后的错误信息放在此字符串中。
void error_msg_miss_op(const char *instruction_name, int line_num)
{
	sprintf(formated_msg, "%s 指令缺少操作数。", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_op(const char *instruction_name, int line_num)
{
	sprintf(formated_msg, "%s 指令不支持这样的操作数。", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_same_symbol(const char *symbol, int line_num, int ref_line_num)
{
	sprintf(formated_msg, "符号 %s 重复定义。参见第 %d 行。", symbol, ref_line_num);
	error_msg(formated_msg, line_num);
}

void error_msg_keyword_symbol(const char *symbol, int line_num)
{
	sprintf(formated_msg, "不能使用保留的关键字 %s 作为符号名称。", symbol);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_data(const char *data, int line_num)
{
	sprintf(formated_msg, "%s 不是有效的数据。", data);
	error_msg(formated_msg, line_num);
}

// 输出汇编过程中发现的警告信息
void warning_msg(const char *warning_msg, int line_num)
{
	if (line_num >= 1)
	{
		printf("%s:%d: warning: %s\n", assembly_file_name, line_num, warning_msg);
	}
	else
	{
		printf("%s: warning: %s\n", assembly_file_name, warning_msg);
	}
}

void warning_msg_invalid_line(int line_num)
{
	warning_msg("忽略无效的代码行。", line_num);
}

void warning_msg_unref_symbol(const char *symbol, int line_num)
{
	sprintf(formated_msg, "符号 %s 未被引用。", symbol);
	warning_msg(formated_msg, line_num);
}

// 判断是否是立即数。如果是数字开头，或者是负号开头的，就认为是立即数。
int is_immediate(const char *token)
{
	return (isdigit(token[0]) || '-' == token[0]) ? 1 : 0;
}

// 判断是否是主存数据
int is_main_memory(const char *token)
{
	return (isdigit(token[0]) || '@' == token[0]) ? 1 : 0;
}

// 指令操作数类型
enum
{
	OT_REGISTER_A // 累加器
	,
	OT_REGISTER_R0 // r0
	,
	OT_REGISTER_R1 // r1
	,
	OT_REGISTER_R2 // r2
	,
	OT_REGISTER_R3 // r3
	,
	OT_REGISTER_R0_INDIRECT // [r0]
	,
	OT_REGISTER_R1_INDIRECT // [r1]
	,
	OT_REGISTER_R2_INDIRECT // [r2]
	,
	OT_REGISTER_R3_INDIRECT // [r3]
	,
	OT_IMMEDIATE // 立即数
	,
	OT_SYMBOL // 符号
	,
	OT_REGISTER_SP // 堆栈指针寄存器
};

// 得到指令操作数的类型
unsigned long get_operand_type(const char *op)
{
	unsigned long op_type;

	if (stricmp(op, a_register_keyword) == 0)
	{
		op_type = OT_REGISTER_A;
	}
	else if (stricmp(op, r0_register_keyword) == 0)
	{
		op_type = OT_REGISTER_R0;
	}
	else if (stricmp(op, r1_register_keyword) == 0)
	{
		op_type = OT_REGISTER_R1;
	}
	else if (stricmp(op, r2_register_keyword) == 0)
	{
		op_type = OT_REGISTER_R2;
	}
	else if (stricmp(op, r3_register_keyword) == 0)
	{
		op_type = OT_REGISTER_R3;
	}
	else if (stricmp(op, r0_register_indirect_keyword) == 0)
	{
		op_type = OT_REGISTER_R0_INDIRECT;
	}
	else if (stricmp(op, r1_register_indirect_keyword) == 0)
	{
		op_type = OT_REGISTER_R1_INDIRECT;
	}
	else if (stricmp(op, r2_register_indirect_keyword) == 0)
	{
		op_type = OT_REGISTER_R2_INDIRECT;
	}
	else if (stricmp(op, r3_register_indirect_keyword) == 0)
	{
		op_type = OT_REGISTER_R3_INDIRECT;
	}
	else if (stricmp(op, sp_register_keyword) == 0)
	{
		op_type = OT_REGISTER_SP;
	}
	else if (is_immediate(op))
	{
		op_type = OT_IMMEDIATE;
	}
	else
	{
		op_type = OT_SYMBOL;
	}

	return op_type;
}

// 根据 r? 寄存器和机器码的基础值，得到一个机器码
char get_machine_code_from_r(unsigned long op_type, char code_base)
{
	return code_base + (char)(op_type - OT_REGISTER_R0);
}

// 根据 [r?] 寄存器和机器码的基础值，得到一个机器码
char get_machine_code_from_r_indirect(unsigned long op_type, char code_base)
{
	return code_base + (char)(op_type - OT_REGISTER_R0_INDIRECT);
}

// 根据立即数得到一个机器码。注意，允许使用负数，所以返回值是带符号的 8 位 char。
char get_machine_code_from_immediate(const char *immediate)
{
	char *end;
	int start_index = (immediate[0] == '-' ? 1 : 0);

	int base = (immediate[start_index] == '0' && (immediate[start_index + 1] == 'x' || immediate[start_index + 1] == 'X')) ? 16 : 10;
	return (char)strtol(immediate, &end, base);
}

// 向重定位表中添加一个重定位信息
void add_reallocate(const char *symbol, int line_num)
{
	reallocate_table[reallocate_count].address = machine_code_address;
	strcpy(reallocate_table[reallocate_count].symbol_name, symbol);
	reallocate_table[reallocate_count].line_num = line_num;
	reallocate_count++;
}

// mov op1, op2
void parse_mov(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(mov_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// mov a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x70);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// mov a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x74);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// mov a, symbol
		machine_code[machine_code_address] = 0x78;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// mov a, immediate
		machine_code[machine_code_address] = 0x7c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else if ((op1_type >= OT_REGISTER_R0 && op1_type <= OT_REGISTER_R3) && OT_REGISTER_A == op2_type)
	{
		// mov r?, a
		machine_code[machine_code_address] = get_machine_code_from_r(op1_type, 0x80);
		machine_code_address++;
	}
	else if ((op1_type >= OT_REGISTER_R0_INDIRECT && op1_type <= OT_REGISTER_R3_INDIRECT) && OT_REGISTER_A == op2_type)
	{
		// mov [r?], a
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op1_type, 0x84);
		machine_code_address++;
	}
	else if (OT_SYMBOL == op1_type && OT_REGISTER_A == op2_type)
	{
		// mov symbol, a
		machine_code[machine_code_address] = 0x88;
		machine_code_address++;

		// 重定位
		add_reallocate(op1, line_num);
		machine_code_address++;
	}
	else if ((op1_type >= OT_REGISTER_R0 && op1_type <= OT_REGISTER_R3) && OT_IMMEDIATE == op2_type)
	{
		// mov r?, immediate
		machine_code[machine_code_address] = get_machine_code_from_r(op1_type, 0x8c);
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else if (OT_REGISTER_SP == op1_type && OT_IMMEDIATE == op2_type)
	{
		// mov sp, immediate
		machine_code[machine_code_address] = 0x9c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(mov_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// jmp symbol
void parse_jmp(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(jmp_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_SYMBOL == op_type)
	{
		// jmp symbol
		machine_code[machine_code_address] = 0xAC;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jmp_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// add op1, op2
void parse_add(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(add_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// add a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x10);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// add a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x14);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// add a, symbol
		machine_code[machine_code_address] = 0x18;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// add a, immediate
		machine_code[machine_code_address] = 0x1c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(add_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// adc op1, op2
void parse_adc(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(adc_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// adc a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x20);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// adc a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x24);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// adc a, symbol
		machine_code[machine_code_address] = 0x28;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// adc a, immediate
		machine_code[machine_code_address] = 0x2c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(adc_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// sub op1, op2
void parse_sub(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(sub_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// sub a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x30);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// sub a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x34);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// sub a, symbol
		machine_code[machine_code_address] = 0x38;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// sub a, immediate
		machine_code[machine_code_address] = 0x3c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(sub_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// sbb op1, op2
void parse_sbb(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(sbb_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// sbb a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x40);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// sbb a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x44);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// sbb a, symbol
		machine_code[machine_code_address] = 0x48;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// sbb a, immediate
		machine_code[machine_code_address] = 0x4c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(sbb_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// and op1, op2
void parse_and(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(and_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// and a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x50);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// and a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x54);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// and a, symbol
		machine_code[machine_code_address] = 0x58;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// and a, immediate
		machine_code[machine_code_address] = 0x5c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(and_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// or op1, op2
void parse_or(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(or_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0 && op2_type <= OT_REGISTER_R3))
	{
		// or a, r?
		machine_code[machine_code_address] = get_machine_code_from_r(op2_type, 0x60);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && (op2_type >= OT_REGISTER_R0_INDIRECT && op2_type <= OT_REGISTER_R3_INDIRECT))
	{
		// or a, [r?]
		machine_code[machine_code_address] = get_machine_code_from_r_indirect(op2_type, 0x64);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// or a, symbol
		machine_code[machine_code_address] = 0x68;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// or a, immediate
		machine_code[machine_code_address] = 0x6c;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(or_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// lea a, symbol
void parse_lea(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(lea_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && OT_SYMBOL == op2_type)
	{
		// lea a, symbol
		machine_code[machine_code_address] = 0x98;
		machine_code_address++;

		// 重定位
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(lea_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// read symbol
void parse_read(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);
	if (NULL == op)
	{
		error_msg_miss_op(read_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);
	if (OT_SYMBOL == op_type)
	{
		// read symbol
		machine_code[machine_code_address] = 0x90;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(read_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// write symbol
void parse_write(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);
	if (NULL == op)
	{
		error_msg_miss_op(write_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);
	if (OT_SYMBOL == op_type)
	{
		// write symbol
		machine_code[machine_code_address] = 0x94;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(write_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// jc symbol
void parse_jc(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(jc_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_SYMBOL == op_type)
	{
		// jc symbol
		machine_code[machine_code_address] = 0xA0;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jc_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// jz symbol
void parse_jz(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(jz_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_SYMBOL == op_type)
	{
		// jz symbol
		machine_code[machine_code_address] = 0xA4;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jz_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// call symbol
void parse_call(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(call_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_SYMBOL == op_type)
	{
		// call symbol
		machine_code[machine_code_address] = 0xE8;
		machine_code_address++;

		// 重定位
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(call_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// int immediate
void parse_int(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(int_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_IMMEDIATE == op_type)
	{
		// int immediate
		machine_code[machine_code_address] = 0xB8;
		machine_code_address++;

		machine_code[machine_code_address] = get_machine_code_from_immediate(op);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(int_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// in
void parse_in(int line_num)
{
	machine_code[machine_code_address] = 0xB0;
	machine_code_address++;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// out
void parse_out(int line_num)
{
	machine_code[machine_code_address] = 0xB4;
	machine_code_address++;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// ret
void parse_ret(int line_num)
{
	machine_code[machine_code_address] = 0xC8;
	machine_code_address++;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// shr a
void parse_shr(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(shr_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_REGISTER_A == op_type)
	{
		machine_code[machine_code_address] = 0xD0;
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(shr_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// shl a
void parse_shl(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(shl_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_REGISTER_A == op_type)
	{
		machine_code[machine_code_address] = 0xD4;
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(shl_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// shlnum a, num
void parse_shlnum(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;
	unsigned long shift_amount;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);
	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(shlnum_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// shlnum a, immediate
		machine_code[machine_code_address] = 0xcc;
		machine_code_address++;
		if (shift_amount > 7)
		{
			error_msg(" nope", line_num);
		}

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	// else if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	// {
	// 	// add a, immediate
	// 	machine_code[machine_code_address] = 0x1c;
	// 	machine_code_address++;

	// 	machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
	// 	machine_code_address++;
	// }
	else
	{
		error_msg_wrong_op(shlnum_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// shrnum a, num
void parse_shrnum(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;
	unsigned long shift_amount;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(shrnum_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// shrnum a, immediate
		machine_code[machine_code_address] = 0xa8;
		machine_code_address++;
		if (shift_amount > 7)
		{
			error_msg(" nope", line_num);
		}

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(shrnum_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}
// rcrnum a, num
void parse_rcrnum(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;
	unsigned long shift_amount;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(rcrnum_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// rcrnum a, immediate
		machine_code[machine_code_address] = 0x90;
		machine_code_address++;
		if (shift_amount > 7)
		{
			error_msg(" nope", line_num);
		}

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(rcrnum_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// rclnum a, num
void parse_rclnum(int line_num)
{
	char *op1, *op2;
	unsigned long op1_type, op2_type;
	unsigned long shift_amount;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if (NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(rclnum_instruction_keyword, line_num);
	}

	op1_type = get_operand_type(op1);
	op2_type = get_operand_type(op2);

	if (OT_REGISTER_A == op1_type && OT_IMMEDIATE == op2_type)
	{
		// rclnum a, immediate
		machine_code[machine_code_address] = 0x94;
		machine_code_address++;
		if (shift_amount > 7)
		{
			error_msg(" nope", line_num);
		}

		machine_code[machine_code_address] = get_machine_code_from_immediate(op2);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(rclnum_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// rcr a
void parse_rcr(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(rcr_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_REGISTER_A == op_type)
	{
		machine_code[machine_code_address] = 0xD8;
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(rcr_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// rcl a
void parse_rcl(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(rcl_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_REGISTER_A == op_type)
	{
		machine_code[machine_code_address] = 0xDC;
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(rcl_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// not a
void parse_not(int line_num)
{
	char *op;
	unsigned long op_type;

	if (assembler_state != AS_TEXT)
	{
		warning_msg_invalid_line(line_num);
		return;
	}

	op = strtok(NULL, delimit_char);

	if (NULL == op)
	{
		error_msg_miss_op(not_instruction_keyword, line_num);
	}

	op_type = get_operand_type(op);

	if (OT_REGISTER_A == op_type)
	{
		machine_code[machine_code_address] = 0xE4;
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(not_instruction_keyword, line_num);
	}

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// iret
void parse_iret(int line_num)
{
	machine_code[machine_code_address] = 0xF8;
	machine_code_address++;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// nop
void parse_nop(int line_num)
{
	machine_code[machine_code_address] = 0xE0;
	machine_code_address++;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// 向符号表中添加一个符号
void add_symbol(const char *symbol, int line_num)
{
	int i;

	// 符号名称不能重复
	for (i = 0; i < symbol_count; i++)
	{
		if (stricmp(symbol, symbol_table[i].name) == 0)
		{
			error_msg_same_symbol(symbol, line_num, symbol_table[i].line_num);
		}
	}

	// 符号数量有限
	if (symbol_count == MAX_SYMBOL_COUNT)
	{
		sprintf(formated_msg, "定义了太多的符号。最多可以定义 %d 个符号。", MAX_SYMBOL_COUNT);
		error_msg(formated_msg, line_num);
	}

	// 添加符号
	strcpy(symbol_table[symbol_count].name, symbol);
	symbol_table[symbol_count].address = machine_code_address;
	symbol_table[symbol_count].line_num = line_num;
	symbol_count++;
}

// 解析符号
void parse_symbol(const char *symbol_with_colon, int line_num)
{
	char symbol[MAX_SYMBOL_LENGTH];
	char *token;

	// 删除符号末尾的冒号
	strcpy(symbol, symbol_with_colon);
	symbol[strlen(symbol) - 1] = 0;

	if (AS_TEXT == assembler_state)
	{
		// 代码段中的标号作为符号。记录到符号表中，不产生机器码。
		add_symbol(symbol, line_num);

		// 标号必须单独占一行，否则报错
		if (strtok(NULL, delimit_char) != NULL)
		{
			error_msg("标号必须单独占用一行。", line_num);
		}
	}
	else if (AS_DATA == assembler_state)
	{
		// 数据段中的变量作为符号。一个变量可以包含多个字节数据。记录到符号表中，产生机器码。
		add_symbol(symbol, line_num);

		while ((token = strtok(NULL, delimit_char)) != NULL)
		{
			if (!is_immediate(token))
			{
				error_msg_wrong_data(token, line_num);
			}

			machine_code[machine_code_address] = get_machine_code_from_immediate(token);
			machine_code_address++;
			symbol_table[symbol_count - 1].machine_code_count++;
		}
	}
	else
	{
		warning_msg_invalid_line(line_num);
	}
}

// 解析代码段开始标志
void parse_code_section_keyword(int line_num)
{
	// 代码段标志
	if (AS_DATA == assembler_state)
	{
		error_msg("代码段不能定义在数据段的后面。", line_num);
	}
	else if (AS_TEXT == assembler_state)
	{
		error_msg("定义了重复的代码段。", line_num);
	}

	assembler_state = AS_TEXT;
}

// 解析数据段开始标志
void parse_data_section_keyword(int line_num)
{
	// 数据段标志
	if (AS_DATA == assembler_state)
	{
		error_msg("定义了重复的数据段。", line_num);
	}
	else if (AS_BEGIN == assembler_state)
	{
		error_msg("数据段不能定义在代码段的前面。", line_num);
	}

	assembler_state = AS_DATA;
}

// 定义关键字和解析函数的对应关系
typedef void (*PARSE_FUNCTION)(int line_num);
struct KEYWORD_FUNCTION_ENTRY
{
	const char **keyword;
	PARSE_FUNCTION parse_function;
};

//
// 必须将关键字及其解析函数放在下面的表中。从而可以使用“表驱动”的编程模式。
//
struct KEYWORD_FUNCTION_ENTRY keyword_function_table[] =
	{
		{NULL, NULL} // 未用

		,
		{&code_section_keyword, parse_code_section_keyword},
		{&data_section_keyword, parse_data_section_keyword}

		,
		{&mov_instruction_keyword, parse_mov},
		{&jmp_instruction_keyword, parse_jmp},
		{&add_instruction_keyword, parse_add},
		{&adc_instruction_keyword, parse_adc},
		{&sub_instruction_keyword, parse_sub},
		{&sbb_instruction_keyword, parse_sbb},
		{&and_instruction_keyword, parse_and},
		{&or_instruction_keyword, parse_or},
		{&read_instruction_keyword, parse_read},
		{&write_instruction_keyword, parse_write},
		{&jc_instruction_keyword, parse_jc},
		{&jz_instruction_keyword, parse_jz},
		{&call_instruction_keyword, parse_call},
		{&in_instruction_keyword, parse_in},
		{&out_instruction_keyword, parse_out},
		{&ret_instruction_keyword, parse_ret},
		{&shr_instruction_keyword, parse_shr},
		{&shl_instruction_keyword, parse_shl},
		{&rcr_instruction_keyword, parse_rcr},
		{&rcl_instruction_keyword, parse_rcl},
		{&not_instruction_keyword, parse_not},
		{&iret_instruction_keyword, parse_iret},
		{&nop_instruction_keyword, parse_nop},
		{&int_instruction_keyword, parse_int},
		{&lea_instruction_keyword, parse_lea},
		{&shlnum_instruction_keyword, parse_shlnum},
		{&shrnum_instruction_keyword, parse_shrnum},
		{&rcrnum_instruction_keyword, parse_rcrnum},
		{&rclnum_instruction_keyword, parse_rclnum},

		{&r0_register_keyword, NULL},
		{&r1_register_keyword, NULL},
		{&r2_register_keyword, NULL},
		{&r3_register_keyword, NULL},
		{&r0_register_indirect_keyword, NULL},
		{&r1_register_indirect_keyword, NULL},
		{&r2_register_indirect_keyword, NULL},
		{&r3_register_indirect_keyword, NULL},
		{&a_register_keyword, NULL},
		{&sp_register_keyword, NULL}};

// 判断是否是一个关键字。返回0，不是关键字；返回非0，是关键字，并且返回值就是关键字在表中的下标。
int match_keyword(const char *token)
{
	int i;

	for (i = 1; i < sizeof(keyword_function_table) / sizeof(keyword_function_table[0]); i++)
	{
		if (stricmp(token, *keyword_function_table[i].keyword) == 0)
		{
			return i;
		}
	}

	return 0;
}

// 判断是否是一个符号
int is_symbol(const char *token, int line_num)
{
	int i;
	char symbol_name[MAX_SYMBOL_LENGTH];

	// 最后一个字符必须是冒号
	if (token[strlen(token) - 1] != ':')
	{
		return 0;
	}

	// 第一个字符必须是字母或者下划线
	if (token[0] != '_' && !isalpha(token[0]))
	{
		return 0;
	}

	// 后面的字符必须是字母、数字、下划线
	for (i = 0; i < (int)strlen(token) - 2; i++)
	{
		if (token[i] != '_' && !isalpha(token[i]) && !isdigit(token[i]))
		{
			return 0;
		}
	}

	// 不能与关键字相同
	strcpy(symbol_name, token);
	symbol_name[strlen(symbol_name) - 1] = 0;
	if (match_keyword(symbol_name) != 0)
	{
		error_msg_keyword_symbol(symbol_name, line_num);
	}

	return 1;
}

// 输出版本信息
void version_msg()
{
	printf(
		"Engintime DM1000 8位模型机汇编器 [版本 2.0]\n");
}

// 输出帮助信息
void help_msg()
{
	printf(
		"Engintime DM1000 8位模型机汇编器。\n\n"
		"用法:\n\n"
		"  dmasm.exe assembly_file_name [options]\n\n"
		"选项:\n\n"
		"  -g debug_file_name\t指定生成的调试信息文件路径。\n"
		"  -h\t\t\t打印此帮助信息。\n"
		"  -l list_file_name\t指定生成的列表文件路径。\n"
		"  -o target_file_name\t指定生成的目标文件路径。若未指定，默认生成 a.obj 文件。\n"
		"  -v\t\t\t打印版本信息。\n");

	printf("\n");

	version_msg();

	exit(1);
}

void argument_error_msg()
{
	printf("命令行参数错误。\n\n");
	help_msg();
}

// 处理器用户输入的命令行参数
void process_argument(int argc, char *argv[])
{
	int i;

	// argv[0] 是 "easm.exe"，所以可以忽略。
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-v") == 0)
		{
			version_msg();
			exit(1);
		}
		else if (strcmp(argv[i], "-h") == 0)
		{
			help_msg();
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				target_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if (strcmp(argv[i], "-l") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				list_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if (strcmp(argv[i], "-g") == 0)
		{
			if (i + 1 < argc)
			{
				i++;
				dbg_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if (NULL == assembly_file_name && argv[i][0] != '-')
		{
			// 输入的汇编源代码文件路径。
			assembly_file_name = argv[i];
		}
		else
		{
			argument_error_msg();
		}
	}

	//
	// 如果命令行参数中没有指定输入的汇编文件，就打印错误信息后退出
	//
	if (NULL == assembly_file_name)
	{
		printf("命令行参数错误。没有指定汇编源代码文件的路径。\n");
		help_msg();
	}
}

// 将一个字符串写入二进制文件中。先将字符串长度写入文件，然后将字符串中的
// 每个字符依次写入文件，不包括字符串末尾的 0.
void write_string_to_binary_file(const char *str, FILE *fp)
{
	int str_length;

	if (str != NULL)
	{
		str_length = strlen(str);
		fwrite(&str_length, 1, sizeof(str_length), fp);
		fwrite(str, 1, str_length, fp);
	}
	else
	{
		str_length = 0;
		fwrite(&str_length, 1, sizeof(str_length), fp);
	}
}

int main(int argc, char *argv[])
{
	FILE *fp;
	char *token;
	char line[MAX_LINE_LENGTH];
	int i, j;
	int line_num = 1; // 行号从第一行开始计数
	int keyword_index;

	//
	// 处理命令行参数
	//
	process_argument(argc, argv);

	//
	// 打开汇编源代码文件
	//
	fp = fopen(assembly_file_name, "r");
	if (NULL == fp)
	{
		printf("无法打开源代码文件 %s\n", assembly_file_name);
		return 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// 第一遍扫描

	version_msg();
	printf("\n正在汇编 %s...\n", assembly_file_name);

	//
	// 一次读取一行文本，同时记录所有的代码行信息
	//
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		//
		// 将一行代码的信息记录到代码行数据库中
		//
		strcpy(line_database[line_count].line_string, line);
		line_database[line_count].line_num = line_num;
		line_database[line_count].address = machine_code_address;

		//
		// 将代码行中的注释剥离
		//
		line[strcspn(line, ";")] = 0;

		//
		// 开始解析代码行
		//
		token = strtok(line, delimit_char);
		if (NULL == token)
		{
			// 如果是空行，不做任何处理
		}
		else if ((keyword_index = match_keyword(token)) != 0 && keyword_function_table[keyword_index].parse_function != NULL)
		{
			// 根据关键字进行相应的处理
			keyword_function_table[keyword_index].parse_function(line_num);
		}
		else if (is_symbol(token, line_num))
		{
			// 处理符号
			parse_symbol(token, line_num);
		}
		else
		{
			error_msg("无法识别的代码行。", line_num);
		}

		//
		// 补充代码行数据库信息
		//
		line_database[line_count].machine_code_count = machine_code_address - machine_code_old_address;
		machine_code_old_address = machine_code_address;

		//
		// 记录产生了机器码的代码行数量
		//
		if (line_database[line_count].machine_code_count != 0)
		{
			machine_code_line_count++;
		}

		//
		// 增加行号
		//
		line_count++;
		line_num++;

		if (line_count == MAX_LINE_COUNT)
		{
			sprintf(formated_msg, "汇编文件中的代码行过多，最多只能有 %d 行代码。", MAX_LINE_COUNT);
			error_msg(formated_msg, -1);
		}
	}

	fclose(fp);

	//////////////////////////////////////////////////////////////////////////
	// 第二遍扫描

	//
	// 根据符号表更新重定位表
	//
	for (i = 0; i < reallocate_count; i++)
	{
		for (j = 0; j < symbol_count; j++)
		{
			if (stricmp(symbol_table[j].name, reallocate_table[i].symbol_name) == 0)
			{
				machine_code[reallocate_table[i].address] = (BYTE)symbol_table[j].address;
				symbol_table[j].ref_count++;

				break;
			}
		}

		if (j == symbol_count)
		{
			// 重定位表中使用的符号在符号表中未定义，报错
			sprintf(formated_msg, "使用了未定义的符号 %s。", reallocate_table[i].symbol_name);
			error_msg(formated_msg, reallocate_table[i].line_num);
		}
	}

	//
	// 如果存在未引用的符号，输出警告信息。
	//
	for (i = 0; i < symbol_count; i++)
	{
		if (0 == symbol_table[i].ref_count)
		{
			warning_msg_unref_symbol(symbol_table[i].name, symbol_table[i].line_num);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 产生各种输出文件

	//
	// 将机器码写入二进制文件
	//
	if (NULL == target_file_name)
	{
		target_file_name = "a.obj";
	}

	fp = fopen(target_file_name, "wb");
	if (NULL == fp)
	{
		printf("无法打开输出文件 %s\n", target_file_name);
		return 1;
	}

	fwrite(machine_code, 1, machine_code_address, fp);

	fclose(fp);

	printf("\n生成目标文件 %s\n", target_file_name);

	//
	// 将代码行数据库中的信息写入列表文件
	//
	if (list_file_name != NULL)
	{
		fp = fopen(list_file_name, "w");
		if (NULL == fp)
		{
			printf("无法打开列表文件 %s\n", list_file_name);
			return 1;
		}

		for (i = 0; i < line_count; i++)
		{
			// 行号
			fprintf(fp, "%04d    ", line_database[i].line_num);

			// 地址和机器码
			if (line_database[i].machine_code_count > 0)
			{
				fprintf(fp, "%02X    ", line_database[i].address);

				for (j = 0; j < line_database[i].machine_code_count; j++)
				{
					// 确保每行最多写两个字节的机器码
					if (j != 0 && j % 2 == 0)
					{
						if (2 == j)
						{
							fprintf(fp, "              ");
						}
						else
						{
							fprintf(fp, "\n        ");
						}
					}

					fprintf(fp, "%02X ", machine_code[line_database[i].address + j]);

					if (1 == j)
					{
						fprintf(fp, "  ");
						fprintf(fp, line_database[i].line_string);
					}
				}

				if (1 == j)
				{
					fprintf(fp, "     ");
					fprintf(fp, line_database[i].line_string);
				}
				else if (j > 2)
				{
					fprintf(fp, "\n");
				}
			}
			else
			{
				fprintf(fp, "              ");

				// 源代码
				fprintf(fp, line_database[i].line_string);
			}
		}

		fclose(fp);

		printf("生成列表文件 %s\n", list_file_name);
	}

	//
	// 将代码行数据库中的信息写入二进制的调试信息文件。
	//
	// 调试信息的基本格式为：
	// 魔数（4字节），固定不可改变
	// 版本号（4字节），固定不可改变
	// 源代码文件绝对路径字符串长度（4字节）
	// 源代码文件绝对路径字符串（不包括字符串结尾的0）
	// 列表文件绝对路径字符串长度（4字节）
	// 列表文件绝对路径字符串（不包括字符串结尾的0）
	//
	// 代码行数据库中元素的数量（4字节）
	// 代码行数据库中的所有元素
	//
	// 符号表元素的数量（4字节）
	// 符号表中的所有元素
	//

	if (dbg_file_name != NULL)
	{
		fp = fopen(dbg_file_name, "wb");
		if (NULL == fp)
		{
			printf("无法打开调试信息文件 %s\n", dbg_file_name);
			return 1;
		}

		// 魔数
		fwrite(&dbg_file_magic, 1, sizeof(dbg_file_magic), fp);

		// 版本号
		fwrite(&dbg_file_version, 1, sizeof(dbg_file_version), fp);

		// 源代码文件路径
		write_string_to_binary_file(assembly_file_name, fp);

		// 列表文件路径
		write_string_to_binary_file(list_file_name, fp);

		// 代码行数据库中的所有元素
		fwrite(&machine_code_line_count, 1, sizeof(machine_code_line_count), fp);
		for (i = 0; i < line_count; i++)
		{
			// 跳过没有产生机器码的代码行
			if (0 == line_database[i].machine_code_count)
			{
				continue;
			}

			fwrite(&line_database[i].line_num, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&line_database[i].flag, 1, sizeof(unsigned long), fp);
		}

		// 符号表中的所有元素
		fwrite(&symbol_count, 1, sizeof(symbol_count), fp);
		for (i = 0; i < symbol_count; i++)
		{
			write_string_to_binary_file(symbol_table[i].name, fp);
			fwrite(&symbol_table[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&symbol_table[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&symbol_table[i].line_num, 1, sizeof(int), fp);
		}

		fclose(fp);

		printf("生成调试信息文件 %s\n", dbg_file_name);
	}

	return 0;
}
