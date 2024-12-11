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

#define MAX_LINE_LENGTH 256 // ??§Õ??????????§Ø???????
#define MAX_LINE_COUNT 1024 // ???????????????§Ø?????

#define MAX_SYMBOL_LENGTH 64 // ????????????????????????
#define MAX_SYMBOL_COUNT 256 // ????????????????????????????

#ifndef MAX_PATH
#define MAX_PATH 256 // ???¡¤???????????????????
#endif

// ???????????????????????????§Õ????§Ö??????????????§³????§µ?
struct LINE_RECORD
{
	char line_string[MAX_LINE_LENGTH]; // ?????§Ö?????
	unsigned long line_num;			   // ?§Ü?
	unsigned long address;			   // ???§Õ??????????????????????§Ö?????????
	int machine_code_count;			   // ???§Õ?????????????????????
	unsigned long flag;				   // ?????§Ò??¦Ë??32¦Ë
};
struct LINE_RECORD line_database[MAX_LINE_COUNT] = {0};
int line_count = 0;
int machine_code_line_count = 0; // ???????????????????§Ö?????

// ?????????§Ö?????§Ò??¦Ë??????????§Ò??¦Ë???¦Ë???????
#define LF_INSTRUCTION 0x00000001 // ?????§Ò??¦Ë?????¦Ë??1?????????????????????????????????

// ???¦Ë????????????????????????????????????????????????????????????
// ?????????????????¦Ë??
struct REALLOCATE
{
	unsigned long address;				 // ???????????????????????????¦Ë??????????????????
										 // ????????????????????????????¦Ë??
	char symbol_name[MAX_SYMBOL_LENGTH]; // ??????¦Ë??????????
	int line_num;						 // ?§Ü??
};
struct REALLOCATE reallocate_table[MAX_LINE_COUNT] = {0};
int reallocate_count = 0;

// ????????????????????§Ö????????????
struct SYMBOL
{
	char name[MAX_SYMBOL_LENGTH]; // ????????
	unsigned long address;		  // ???????????????¦Ë?????????
	int machine_code_count;		  // ?????????????????????????????¦Ë????
	int line_num;				  // ?§Ü?
	int ref_count;				  // ???¨¹???
};
struct SYMBOL symbol_table[MAX_SYMBOL_COUNT] = {0};
int symbol_count = 0;

// ?????????
enum
{
	AS_BEGIN // ????????????????????????????????
	,
	AS_TEXT // ???????????¦±??????????????????????????????????????
	,
	AS_DATA // ???????????¦±?????????????????????
};
unsigned long assembler_state = AS_BEGIN;

//
// ?????ÕÇ?????§Ö?????
//

// ??????
const char *code_section_keyword = ".text"; // ????¦Á??
char const *data_section_keyword = ".data"; // ????¦Á??

// ???????
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

// ??¨¹????????
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

const char *delimit_char = "\n\t\r ";		 // ?????????????
const char *delimit_char_comma = "\n\t\r, "; // ?????????????????????????

// ?????????????
#define MAX_MACHINE_CODE 1024
BYTE machine_code[MAX_MACHINE_CODE];
unsigned long machine_code_address = 0;
unsigned long machine_code_old_address = 0;

const char *assembly_file_name = NULL; // ??????¡¤??
const char *target_file_name = NULL;   // ??????¡¤??
const char *list_file_name = NULL;	   // ?§Ò????¡¤??
const char *dbg_file_name = NULL;	   // ??????????¡¤??

const unsigned long dbg_file_magic = 58;
const unsigned long dbg_file_version = 1;

// ??????????§Ù?????????????
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

char formated_msg[1024]; // ???????????????????????????§³?
void error_msg_miss_op(const char *instruction_name, int line_num)
{
	sprintf(formated_msg, "%s ?????????????", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_op(const char *instruction_name, int line_num)
{
	sprintf(formated_msg, "%s ???????????????????", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_same_symbol(const char *symbol, int line_num, int ref_line_num)
{
	sprintf(formated_msg, "???? %s ??????‰^?¦Ì??? %d ?§³?", symbol, ref_line_num);
	error_msg(formated_msg, line_num);
}

void error_msg_keyword_symbol(const char *symbol, int line_num)
{
	sprintf(formated_msg, "???????????????? %s ????????????", symbol);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_data(const char *data, int line_num)
{
	sprintf(formated_msg, "%s ??????§¹???????", data);
	error_msg(formated_msg, line_num);
}

// ??????????§Ù??????????
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
	warning_msg("??????§¹??????§³?", line_num);
}

void warning_msg_unref_symbol(const char *symbol, int line_num)
{
	sprintf(formated_msg, "???? %s ¦Ä?????¨¢?", symbol);
	warning_msg(formated_msg, line_num);
}

// ?§Ø????????????????????????????????????????????????????????
int is_immediate(const char *token)
{
	return (isdigit(token[0]) || '-' == token[0]) ? 1 : 0;
}

// ?§Ø??????????????
int is_main_memory(const char *token)
{
	return (isdigit(token[0]) || '@' == token[0]) ? 1 : 0;
}

// ????????????
enum
{
	OT_REGISTER_A // ?????
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
	OT_IMMEDIATE // ??????
	,
	OT_SYMBOL // ????
	,
	OT_REGISTER_SP // ??????????
};

// ?????????????????
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

// ???? r? ???????????????????????????????
char get_machine_code_from_r(unsigned long op_type, char code_base)
{
	return code_base + (char)(op_type - OT_REGISTER_R0);
}

// ???? [r?] ???????????????????????????????
char get_machine_code_from_r_indirect(unsigned long op_type, char code_base)
{
	return code_base + (char)(op_type - OT_REGISTER_R0_INDIRECT);
}

// ????????????????????????????????????????????????????? 8 ¦Ë char??
char get_machine_code_from_immediate(const char *immediate)
{
	char *end;
	int start_index = (immediate[0] == '-' ? 1 : 0);

	int base = (immediate[start_index] == '0' && (immediate[start_index + 1] == 'x' || immediate[start_index + 1] == 'X')) ? 16 : 10;
	return (char)strtol(immediate, &end, base);
}

// ?????¦Ë??????????????¦Ë???
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

		// ???¦Ë
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jmp_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
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
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op2, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(lea_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(read_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(write_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jc_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(jz_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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

		// ???¦Ë
		add_reallocate(op, line_num);
		machine_code_address++;
	}
	else
	{
		error_msg_wrong_op(call_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// in
void parse_in(int line_num)
{
	machine_code[machine_code_address] = 0xB0;
	machine_code_address++;

	//
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// out
void parse_out(int line_num)
{
	machine_code[machine_code_address] = 0xB4;
	machine_code_address++;

	//
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// ret
void parse_ret(int line_num)
{
	machine_code[machine_code_address] = 0xC8;
	machine_code_address++;

	//
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
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
		if (shift_amount > 7) // ?????????¦Ë???7
		{
			error_msg("??¦Ë?????????7??", line_num);
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
		error_msg_wrong_op(shl_instruction_keyword, line_num);
	}

	//
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
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
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// iret
void parse_iret(int line_num)
{
	machine_code[machine_code_address] = 0xF8;
	machine_code_address++;

	//
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// nop
void parse_nop(int line_num)
{
	machine_code[machine_code_address] = 0xE0;
	machine_code_address++;

	//
	// ?????????????§µ?????????????????
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// ???????????????????
void add_symbol(const char *symbol, int line_num)
{
	int i;

	// ??????????????
	for (i = 0; i < symbol_count; i++)
	{
		if (stricmp(symbol, symbol_table[i].name) == 0)
		{
			error_msg_same_symbol(symbol, line_num, symbol_table[i].line_num);
		}
	}

	// ????????????
	if (symbol_count == MAX_SYMBOL_COUNT)
	{
		sprintf(formated_msg, "??????????????????????? %d ???????", MAX_SYMBOL_COUNT);
		error_msg(formated_msg, line_num);
	}

	// ???????
	strcpy(symbol_table[symbol_count].name, symbol);
	symbol_table[symbol_count].address = machine_code_address;
	symbol_table[symbol_count].line_num = line_num;
	symbol_count++;
}

// ????????
void parse_symbol(const char *symbol_with_colon, int line_num)
{
	char symbol[MAX_SYMBOL_LENGTH];
	char *token;

	// ????????¦Â?????
	strcpy(symbol, symbol_with_colon);
	symbol[strlen(symbol) - 1] = 0;

	if (AS_TEXT == assembler_state)
	{
		// ??????§Ö??????????????????????§µ?????????????
		add_symbol(symbol, line_num);

		// ???????????§µ??????
		if (strtok(NULL, delimit_char) != NULL)
		{
			error_msg("?????????????§³?", line_num);
		}
	}
	else if (AS_DATA == assembler_state)
	{
		// ??????§Ö????????????????????????????????????????????????§µ???????????
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

// ????????¦Ï?????
void parse_code_section_keyword(int line_num)
{
	// ????¦Á??
	if (AS_DATA == assembler_state)
	{
		error_msg("????¦Â????????????¦Å???µµ", line_num);
	}
	else if (AS_TEXT == assembler_state)
	{
		error_msg("??????????????¦±?", line_num);
	}

	assembler_state = AS_TEXT;
}

// ????????¦Ï?????
void parse_data_section_keyword(int line_num)
{
	// ????¦Á??
	if (AS_DATA == assembler_state)
	{
		error_msg("???????????????¦±?", line_num);
	}
	else if (AS_BEGIN == assembler_state)
	{
		error_msg("????¦Â???????????¦Å???µµ", line_num);
	}

	assembler_state = AS_DATA;
}

// ???????????????????????
typedef void (*PARSE_FUNCTION)(int line_num);
struct KEYWORD_FUNCTION_ENTRY
{
	const char **keyword;
	PARSE_FUNCTION parse_function;
};

//
// ??????????????????????????????§³??????????¨¢?????????????????
//
struct KEYWORD_FUNCTION_ENTRY keyword_function_table[] =
	{
		{NULL, NULL} // ¦Ä??

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
		{&shlnum_instruction_keyword, parse_shlnum},

		{&rcr_instruction_keyword, parse_rcr},
		{&rcl_instruction_keyword, parse_rcl},
		{&not_instruction_keyword, parse_not},
		{&iret_instruction_keyword, parse_iret},
		{&nop_instruction_keyword, parse_nop},
		{&int_instruction_keyword, parse_int},
		{&lea_instruction_keyword, parse_lea}

		,
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

// ?§Ø???????????????????0????????????????0?????????????????????????????§Ö??¡À?
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

// ?§Ø?????????????
int is_symbol(const char *token, int line_num)
{
	int i;
	char symbol_name[MAX_SYMBOL_LENGTH];

	// ??????????????????
	if (token[strlen(token) - 1] != ':')
	{
		return 0;
	}

	// ??????????????????????????
	if (token[0] != '_' && !isalpha(token[0]))
	{
		return 0;
	}

	// ?????????????????????????????
	for (i = 0; i < (int)strlen(token) - 2; i++)
	{
		if (token[i] != '_' && !isalpha(token[i]) && !isdigit(token[i]))
		{
			return 0;
		}
	}

	// ?????????????
	strcpy(symbol_name, token);
	symbol_name[strlen(symbol_name) - 1] = 0;
	if (match_keyword(symbol_name) != 0)
	{
		error_msg_keyword_symbol(symbol_name, line_num);
	}

	return 1;
}

// ????·Ú???
void version_msg()
{
	printf(
		"Engintime DM1000 8¦Ë????????? [?·Ú 2.0]\n");
}

// ??????????
void help_msg()
{
	printf(
		"Engintime DM1000 8¦Ë???????????\n\n"
		"?¡Â?:\n\n"
		"  dmasm.exe assembly_file_name [options]\n\n"
		"???:\n\n"
		"  -g debug_file_name\t?????????????????¡¤????\n"
		"  -h\t\t\t?????????????\n"
		"  -l list_file_name\t?????????§Ò????¡¤????\n"
		"  -o target_file_name\t??????????????¡¤??????¦Ä???????????? a.obj ?????\n"
		"  -v\t\t\t????·Ú?????\n");

	printf("\n");

	version_msg();

	exit(1);
}

void argument_error_msg()
{
	printf("?????§Ó???????\n\n");
	help_msg();
}

// ???????????????????§Ó???
void process_argument(int argc, char *argv[])
{
	int i;

	// argv[0] ?? "easm.exe"?????????????
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
			// ???????????????¡¤????
			assembly_file_name = argv[i];
		}
		else
		{
			argument_error_msg();
		}
	}

	//
	// ????????§Ó???????????????????????????????????????
	//
	if (NULL == assembly_file_name)
	{
		printf("?????§Ó??????????????????????????¡¤????\n");
		help_msg();
	}
}

// ??????????§Õ???????????§³?????????????§Õ????????????????§Ö?
// ??????????§Õ???????????????????¦Â?? 0.
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
	int line_num = 1; // ?§Ü?????§á??????
	int keyword_index;

	//
	// ?????????§Ó???
	//
	process_argument(argc, argv);

	//
	// ????????????
	//
	fp = fopen(assembly_file_name, "r");
	if (NULL == fp)
	{
		printf("????????????? %s\n", assembly_file_name);
		return 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// ????????

	version_msg();
	printf("\n?????? %s...\n", assembly_file_name);

	//
	// ??¦Æ??????????????????§Ö?????????
	//
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		//
		// ????§Õ?????????????????????????
		//
		strcpy(line_database[line_count].line_string, line);
		line_database[line_count].line_num = line_num;
		line_database[line_count].address = machine_code_address;

		//
		// ?????????§Ö???????
		//
		line[strcspn(line, ";")] = 0;

		//
		// ?????????????
		//
		token = strtok(line, delimit_char);
		if (NULL == token)
		{
			// ???????§µ??????¦Ê¦Ä???
		}
		else if ((keyword_index = match_keyword(token)) != 0 && keyword_function_table[keyword_index].parse_function != NULL)
		{
			// ???????????????????
			keyword_function_table[keyword_index].parse_function(line_num);
		}
		else if (is_symbol(token, line_num))
		{
			// ????????
			parse_symbol(token, line_num);
		}
		else
		{
			error_msg("???????????§³?", line_num);
		}

		//
		// ?????????????????
		//
		line_database[line_count].machine_code_count = machine_code_address - machine_code_old_address;
		machine_code_old_address = machine_code_address;

		//
		// ????????????????????????
		//
		if (line_database[line_count].machine_code_count != 0)
		{
			machine_code_line_count++;
		}

		//
		// ?????§Ü?
		//
		line_count++;
		line_num++;

		if (line_count == MAX_LINE_COUNT)
		{
			sprintf(formated_msg, "???????§Ö?????§Û??????????? %d ?§Õ???", MAX_LINE_COUNT);
			error_msg(formated_msg, -1);
		}
	}

	fclose(fp);

	//////////////////////////////////////////////////////////////////////////
	// ????????

	//
	// ???????????????¦Ë??
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
			// ???¦Ë???????????????????¦Ä???ÈÉ????
			sprintf(formated_msg, "?????¦Ä???????? %s??", reallocate_table[i].symbol_name);
			error_msg(formated_msg, reallocate_table[i].line_num);
		}
	}

	//
	// ???????¦Ä?????????????????????
	//
	for (i = 0; i < symbol_count; i++)
	{
		if (0 == symbol_table[i].ref_count)
		{
			warning_msg_unref_symbol(symbol_table[i].name, symbol_table[i].line_num);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// ??????????????

	//
	// ????????§Õ??????????
	//
	if (NULL == target_file_name)
	{
		target_file_name = "a.obj";
	}

	fp = fopen(target_file_name, "wb");
	if (NULL == fp)
	{
		printf("??????????? %s\n", target_file_name);
		return 1;
	}

	fwrite(machine_code, 1, machine_code_address, fp);

	fclose(fp);

	printf("\n?????????? %s\n", target_file_name);

	//
	// ??????????????§Ö????§Õ???§Ò????
	//
	if (list_file_name != NULL)
	{
		fp = fopen(list_file_name, "w");
		if (NULL == fp)
		{
			printf("??????§Ò???? %s\n", list_file_name);
			return 1;
		}

		for (i = 0; i < line_count; i++)
		{
			// ?§Ü?
			fprintf(fp, "%04d    ", line_database[i].line_num);

			// ??????????
			if (line_database[i].machine_code_count > 0)
			{
				fprintf(fp, "%02X    ", line_database[i].address);

				for (j = 0; j < line_database[i].machine_code_count; j++)
				{
					// ?????????§Õ?????????????
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

				// ?????
				fprintf(fp, line_database[i].line_string);
			}
		}

		fclose(fp);

		printf("?????§Ò???? %s\n", list_file_name);
	}

	//
	// ??????????????§Ö????§Õ???????????????????
	//
	// ??????????????????
	// ?????4???????????????
	// ?·Ú???4???????????????
	// ????????????¡¤????????????4????
	// ????????????¡¤??????????????????????¦Â??0??
	// ?§Ò????????¡¤????????????4????
	// ?§Ò????????¡¤??????????????????????¦Â??0??
	//
	// ???????????????????????4????
	// ????????????§Ö????????
	//
	// ???????????????4????
	// ??????§Ö????????
	//

	if (dbg_file_name != NULL)
	{
		fp = fopen(dbg_file_name, "wb");
		if (NULL == fp)
		{
			printf("?????????????? %s\n", dbg_file_name);
			return 1;
		}

		// ???
		fwrite(&dbg_file_magic, 1, sizeof(dbg_file_magic), fp);

		// ?·Ú??
		fwrite(&dbg_file_version, 1, sizeof(dbg_file_version), fp);

		// ????????¡¤??
		write_string_to_binary_file(assembly_file_name, fp);

		// ?§Ò????¡¤??
		write_string_to_binary_file(list_file_name, fp);

		// ????????????§Ö????????
		fwrite(&machine_code_line_count, 1, sizeof(machine_code_line_count), fp);
		for (i = 0; i < line_count; i++)
		{
			// ??????§Ó???????????????
			if (0 == line_database[i].machine_code_count)
			{
				continue;
			}

			fwrite(&line_database[i].line_num, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&line_database[i].flag, 1, sizeof(unsigned long), fp);
		}

		// ??????§Ö????????
		fwrite(&symbol_count, 1, sizeof(symbol_count), fp);
		for (i = 0; i < symbol_count; i++)
		{
			write_string_to_binary_file(symbol_table[i].name, fp);
			fwrite(&symbol_table[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&symbol_table[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&symbol_table[i].line_num, 1, sizeof(int), fp);
		}

		fclose(fp);

		printf("????????????? %s\n", dbg_file_name);
	}

	return 0;
}
