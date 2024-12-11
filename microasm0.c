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

#define FALSE	0
#define TRUE	1

#define MAX_LINE_LENGTH		256		// 一行代码最多可以有多少个字符
#define MAX_LINE_COUNT		1024	// 源代码文件最多可以有多少行

#define MAX_SYMBOL_LENGTH	64		// 一个符号最多可以包含多少个字符
#define MAX_SYMBOL_COUNT	256		// 源代码文件中最多可以包含多少个符号

#ifndef MAX_PATH
#define MAX_PATH 256				// 文件路径最多可以包含多少了字符
#endif

// 代码行数据库。记录源代码文件中所有代码行的信息（包括注释行、空行）
struct LINE_RECORD
{
	char line_string[MAX_LINE_LENGTH];	// 代码行的内容
	unsigned long line_num;				// 行号
	unsigned long address;				// 此行代码转换的机器码在映像文件中的地址（偏移）。注意，对于 DM1000 来说，微指令的一个地址对应 4 个字节。
	int machine_code_count;				// 此行代码转换的机器码的字节数量
	unsigned long flag;					// 代码行标志位，32位
};
struct LINE_RECORD line_database[MAX_LINE_COUNT] = {0};
int line_count = 0;
int machine_code_line_count = 0;	// 记录产生了机器码的代码行的数量

// 在此定义所有的代码行标志位。注意，代码行标志位是按位或的关系。
#define LF_INSTRUCTION		0x00000001		// 代码行标志位的最低位是1，表示此行是一条指令，否则表示此行是数据
 
//
// 在下面定义所有的关键字
//
// 寄存器名称
const char* rx_register_keyword = "rx";
const char* mar_register_keyword = "mar";
const char* rin_register_keyword = "rin";
const char* rout_register_keyword = "rout";
const char* sp_register_keyword = "sp";
const char* ia_register_keyword = "ia";
const char* ir_register_keyword = "ir";
const char* flag_register_keyword = "flag";
const char* pc_register_keyword = "pc";
const char* a_register_keyword = "a";
const char* w_register_keyword = "w";
const char* asr_register_keyword = "asr";
const char* upc_register_keyword = "upc";

// 堆栈计数器
const char* csp_counter_keyword = "csp";

//
// alu 操作码
const char* alu_add_keyword = "alu_add";
const char* alu_adc_keyword = "alu_adc";
const char* alu_sub_keyword = "alu_sub";
const char* alu_sbb_keyword = "alu_sbb";
const char* alu_and_keyword = "alu_and";
const char* alu_or_keyword = "alu_or";

const char* alu_aout_keyword = "alu_aout";
const char* alu_shr_keyword = "alu_shr";
const char* alu_shl_keyword = "alu_shl";
const char* alu_rcr_keyword = "alu_rcr";
const char* alu_rcl_keyword = "alu_rcl";
const char* alu_not_keyword = "alu_not";


const char* sp_inc_keyword = "sp_inc";
const char* sp_dec_keyword = "sp_dec";

// 访问主存或外设的操作数
const char* pc_main_memory_keyword = "[pc]";
const char* mar_main_memory_keyword = "[mar]";


const char* delimit_char = "\n\t\r ";			// 需要忽略的空白字符
const char* delimit_char_comma = "\n\t\r, ";	// 需要忽略的空白字符，包括英文逗号


// 单操作数微指令
struct ONE_OPERAND_INSTRUCTION_ENTRY
{
	const char** op;
	unsigned long micro_machine_code;
};

struct ONE_OPERAND_INSTRUCTION_ENTRY one_operand_table[] =
{
	{	NULL,						0x0			}	// 未用

	,{	&pc_register_keyword,		0xffffffff	} // inc pc
	,{	&upc_register_keyword,		0xffffffcf  } // reset upc
};


// 查表，操作数是否匹配。返回0，操作数不匹配；返回非0，操作数匹配，并且返回值就是操作数在表中的下标。
int match_one_operand(const char* op)
{
	int i;

	for(i=1; i<sizeof(one_operand_table)/sizeof(one_operand_table[0]); i++)
	{
		if((stricmp(op, *one_operand_table[i].op) == 0))
		{
			return i;
		}
	}

	return 0;
}


// 双操作数微指令
struct PATH_INSTRUCTION_OPERAND_ENTRY
{
	const char** op1;
	const char** op2;
	unsigned long micro_machine_code;
};


//////////////////////////////////////////////////////////////////////////
//
struct PATH_INSTRUCTION_OPERAND_ENTRY path_operand_table[] =
{
	{	NULL,							NULL,							0x0			}	// 未用

	// 注意：在本文档中，所有微指令编码都是从低字节到高字节的顺序编码的。
	// 以取指微指令 path [pc], ir 为例，它的32位编码，从低字节到高字节依次为：[7:0]=ef，[15:8]=3f，[23:16]=f9, [31:24]=ff；
	// 而在DM1000中，按照阅读习惯，将高位字节放在前面，低位字节放在后面。
	// 因此，在源代码窗口和存储器窗口中显示的是“ef 3f f9 ff”，即微指令编码[31:0]=fff93fef。
	,{	&pc_main_memory_keyword,		&ir_register_keyword,			0xfff93fef	} // path [pc], ir		

	,{	&alu_add_keyword,				&a_register_keyword,			0x99e4ffef  } // path alu_add, a	
	,{	&alu_sub_keyword,				&a_register_keyword,			0x86e4ffef  } // path alu_sub, a	
	,{	&alu_or_keyword,				&a_register_keyword,			0xbee4ffef	} // path alu_or, a
	,{	&alu_and_keyword,				&a_register_keyword,			0xbbe4ffef  } // path alu_and, a
	,{	&alu_adc_keyword,				&a_register_keyword,			0x89e4ffef  } // path alu_adc, a
	,{	&alu_sbb_keyword,				&a_register_keyword,			0x96e4ffef  } // path alu_sbb, a
	,{	&alu_shr_keyword,				&a_register_keyword,			0x90d5ffef  } // path alu_shr, a	 
	,{	&alu_shl_keyword,				&a_register_keyword,			0x90d6ffef  } // path alu_shl, a
	,{	&alu_rcr_keyword,				&a_register_keyword,			0x90e5ffef  } // path alu_rcr, a
	,{	&alu_rcl_keyword,				&a_register_keyword,			0x90e6ffef  } // path alu_rcl, a
	,{	&alu_not_keyword,				&a_register_keyword,			0xb0f4ffef  } // path alu_not, a

	,{	&rx_register_keyword,			&w_register_keyword,			0x7ffaffef	} // path rx, w
	,{	&rx_register_keyword,			&mar_register_keyword,			0xfffaf7ef	} // path rx, mar
	,{	&mar_main_memory_keyword,		&w_register_keyword,			0x7ff9fbef	} // path [mar], w
	,{	&pc_main_memory_keyword,		&mar_register_keyword,			0xfff977ef	} // path [pc], mar
	,{	&pc_main_memory_keyword,		&w_register_keyword,			0x7ff97fef	} // path [pc], w
	,{	&rx_register_keyword,			&a_register_keyword,			0xbffaffef	} // path rx, a
	,{	&mar_main_memory_keyword,		&a_register_keyword,			0xbff9fbef  } // path [mar], a   从地址寄存器指定的内存单元读数据到a寄存器
	,{	&pc_main_memory_keyword,		&a_register_keyword,			0xbff97fef	} // path [pc], a
	,{	&a_register_keyword,			&rx_register_keyword,			0xd0b4ffef	} // path a, rx
	,{	&a_register_keyword,			&mar_main_memory_keyword,		0xd0f4fbed	} // path a, [mar]  将a寄存器数据写入地址寄存器指向的内存
	,{	&pc_main_memory_keyword,		&rx_register_keyword,			0xffb97fef	} // path [pc], rx
	
	,{	&alu_aout_keyword,				&mar_main_memory_keyword,		0xd0f4fbec	} // path alu_aout, [mar]
	,{	&pc_main_memory_keyword,		&pc_register_keyword,			0xfff96fff	} // path [pc], pc

	,{	&pc_register_keyword,			&sp_register_keyword,			0xfff3feef	} // path pc, sp
	,{	&ia_register_keyword,			&mar_register_keyword,			0xfff1f7ef	} // path ia, mar
	,{	&pc_register_keyword,			&mar_register_keyword,			0xfff3f7ef	} // path pc, mar
	,{	&rin_register_keyword,			&a_register_keyword,			0xbff0ffef	} // path rin, a
	,{	&a_register_keyword,			&rout_register_keyword,			0xd0f4ffee	} // path a, rout
	,{	&mar_main_memory_keyword,		&pc_register_keyword,			0xfff9ebef  } // path [mar],pc 从地址寄存器指定的内存单元读数据到程序计数器pc
	,{	&pc_main_memory_keyword,		&sp_register_keyword,			0xfff97eef	} // path [pc], sp 
	

	,{	&sp_register_keyword,			&mar_register_keyword,			0xfff2f7ef	} // path sp, mar
	,{	&sp_register_keyword,			&csp_counter_keyword,			0xff72ffef	} // path sp, csp	
	,{	&pc_main_memory_keyword,		&ia_register_keyword,			0xfff97feb	} // path [pc], ia	
	,{	&pc_main_memory_keyword,		&asr_register_keyword,			0xfff97def	} // path [pc], asr	

	,{	&csp_counter_keyword,			&mar_register_keyword,			0xfff8f7ef	} // path csp, mar  
	,{	&csp_counter_keyword,			&sp_register_keyword,			0xfff8feef	} // path csp, sp

	,{	&sp_inc_keyword,				&csp_counter_keyword,			0xffffffe7	} // path sp_inc, csp
	,{	&sp_dec_keyword,				&csp_counter_keyword,			0xffffffef	} // path sp_dec, csp

	,{	&pc_register_keyword,			&mar_main_memory_keyword,		0xfff3fbed	} // path pc, [mar] 将pc值写入mar指向的存储单元
	,{	&asr_register_keyword,			&pc_register_keyword,			0xfff7efef	} // path asr, pc 
	,{	&sp_register_keyword,			&asr_register_keyword,			0xfff2fdef	} // path sp, asr 

};


// 查表，判断两个操作数是否匹配。返回0，操作数不完全匹配；返回非0，操作数完全匹配，并且返回值就是操作数在表中的下标。
int match_ops(const char* op1, const char* op2)
{
	int i;

	for(i=1; i<sizeof(path_operand_table)/sizeof(path_operand_table[0]); i++)
	{
		if((stricmp(op1, *path_operand_table[i].op1) == 0)
			&& (stricmp(op2, *path_operand_table[i].op2) == 0))
		{
			return i;
		}
	}

	return 0;
}

// 汇编产生的机器码
#define MAX_MACHINE_CODE 1024
BYTE machine_code[MAX_MACHINE_CODE];
unsigned long machine_code_address = 0;		// 一个地址对应一个字节
unsigned long machine_code_old_address = 0;


const char* micro_file_name = NULL;				// 微指令文件路径
const char* target_file_name = NULL;			// 目标文件路径
const char* list_file_name = NULL;				// 列表文件路径
const char* dbg_file_name = NULL;				// 调试信息文件路径

const unsigned long dbg_file_magic = 58;
const unsigned long dbg_file_version = 1;


// 输出汇编过程中发现的语法错误信息
void error_msg(const char* error_msg, int line_num)
{
	if(line_num >= 1)
	{
		printf("%s:%d: error: %s\n", micro_file_name, line_num, error_msg);
	}
	else
	{
		printf("%s: error: %s\n", micro_file_name, error_msg);
	}

	exit(1);
}



char formated_msg[1024];	// 将格式化后的错误信息放在此字符串中。
void error_msg_miss_op(const char* instruction_name, int line_num)
{
	sprintf(formated_msg, "%s 指令缺少操作数。", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_op(const char* instruction_name, int line_num)
{
	sprintf(formated_msg, "%s 指令不支持这样的操作数。", instruction_name);
	error_msg(formated_msg, line_num);
}


// 定义关键字和解析函数的对应关系
typedef void (*PARSE_FUNCTION)(int line_num);
struct KEYWORD_FUNCTION_ENTRY
{
	const char** keyword;
	PARSE_FUNCTION parse_function;
};

// 指令名称
const char* dup_instruction_keyword = "dup";
const char* null_instruction_keyword = "null";
const char* path_instruction_keyword = "path";

const char* inc_instruction_keyword = "inc"; // pc+1
const char* reset_instruction_keyword = "reset"; // 复位

// 判断是否是立即数。不支持负数。
int is_immediate(const char* token)
{
	return isdigit(token[0]);
}

// 根据立即数得到值。不支持负数。
unsigned long get_value_from_immediate(const char* immediate)
{
	char* end;
	int base = (immediate[0] == '0' && (immediate[1] == 'x' || immediate[1] == 'X')) ? 16 : 10;
	return strtoul(immediate, &end, base);
}

// dup n, null
void parse_dup(int line_num)
{
	char *op1, *op2;
	int i;
	unsigned long micro_instruction_count;	// 微指令个数
	unsigned long micro_code;				// 微指令编码

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if(NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(dup_instruction_keyword, line_num);
	}

	// dup微指令的第一个操作数必须是立即数(十进制或十六进制)
	if (!is_immediate(op1))
	{
		error_msg_wrong_op(dup_instruction_keyword, line_num);
	}

	// 得到微指令个数
	micro_instruction_count = get_value_from_immediate(op1);

	// 解析第二个操作数
	if (is_immediate(op2))
	{
		micro_code = get_value_from_immediate(op2);
	}
	else if(0 == stricmp(op2, null_instruction_keyword))
	{
		micro_code = 0xffffffff;
	}
	else
	{
		error_msg_wrong_op(dup_instruction_keyword, line_num);
	}

	for (i = 0; i < (int)micro_instruction_count; i++)
	{
		memcpy(&machine_code[machine_code_address], &micro_code, 4);
		machine_code_address += 4;
	}
}


// null
void parse_null(int line_num)
{
	unsigned long ul = 0xffffffff;
	memcpy(&machine_code[machine_code_address], &ul, 4);
	machine_code_address += 4;
}


// path op1, op2
void parse_path(int line_num)
{
	char *op1, *op2;
	int index;

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if(NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(path_instruction_keyword, line_num);
	}

	index = match_ops(op1, op2);
	if (0 == index)
	{
		error_msg_wrong_op(path_instruction_keyword, line_num);
	}

	memcpy(&machine_code[machine_code_address], &path_operand_table[index].micro_machine_code, 4);
	machine_code_address += 4;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}

// inc 
void parse_inc(int line_num)
{
	char *op;
	int index;

	op = strtok(NULL, delimit_char_comma);
	if(NULL == op)
	{
		error_msg_miss_op(inc_instruction_keyword, line_num);
	}

	index = match_one_operand(op);
	if (0 == index)
	{
		error_msg_wrong_op(inc_instruction_keyword, line_num);
	}

	memcpy(&machine_code[machine_code_address], &one_operand_table[index].micro_machine_code, 4);
	machine_code_address += 4;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}


// reset 
void parse_reset(int line_num)
{
	char *op;
	int index;

	op = strtok(NULL, delimit_char_comma);
	if(NULL == op)
	{
		error_msg_miss_op(reset_instruction_keyword, line_num);
	}

	index = match_one_operand(op);
	if (0 == index)
	{
		error_msg_wrong_op(reset_instruction_keyword, line_num);
	}

	memcpy(&machine_code[machine_code_address], &one_operand_table[index].micro_machine_code, 4);
	machine_code_address += 4;

	//
	// 在代码行数据库中，标记此行是一个指令行
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}


//
// 必须将关键字及其解析函数放在下面的表中。从而可以使用“表驱动”的编程模式。
//
struct KEYWORD_FUNCTION_ENTRY keyword_function_table[] =
{
	 {	NULL,								NULL						}	// 未用

	,{	&dup_instruction_keyword,			parse_dup					}
	,{	&null_instruction_keyword,			parse_null					}
	,{	&path_instruction_keyword,			parse_path					}
	,{	&inc_instruction_keyword,			parse_inc					}
	,{	&reset_instruction_keyword,			parse_reset					}
};



// 判断是否是一个关键字。返回0，不是关键字；返回非0，是关键字，并且返回值就是关键字在表中的下标。
int match_keyword(const char* token)
{
	int i;

	for(i=1; i<sizeof(keyword_function_table)/sizeof(keyword_function_table[0]); i++)
	{
		if(stricmp(token, *keyword_function_table[i].keyword) == 0)
		{
			return i;
		}
	}

	return 0;
}


// 输出版本信息
void version_msg()
{
	printf(
		"Engintime DM1000 8位模型机微指令汇编器 [版本 2.0]\n"
		);
}

// 输出帮助信息
void help_msg()
{
	printf(
		"Engintime DM1000 8位模型机微指令汇编器。\n\n"
		"用法:\n\n"
		"  microasm.exe micro_file_name [options]\n\n"
		"选项:\n\n"
		"  -g debug_file_name\t指定生成的调试信息文件路径。\n"
		"  -h\t\t\t打印此帮助信息。\n"
		"  -l list_file_name\t指定生成的列表文件路径。\n"
		"  -o target_file_name\t指定生成的目标文件路径。若未指定，默认生成 micro.obj 文件。\n"
		"  -v\t\t\t打印版本信息。\n"
		);

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
void process_argument(int argc, char* argv[])
{
	int i;

	// argv[0] 是 "easm.exe"，所以可以忽略。
	for(i=1; i<argc; i++)
	{
		if(strcmp(argv[i], "-v") == 0)
		{
			version_msg();
			exit(1);
		}
		else if(strcmp(argv[i], "-h") == 0)
		{
			help_msg();
		}
		else if(strcmp(argv[i], "-o") == 0)
		{
			if(i + 1 < argc)
			{
				i++;
				target_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if(strcmp(argv[i], "-l") == 0)
		{
			if(i + 1 < argc)
			{
				i++;
				list_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if(strcmp(argv[i], "-g") == 0)
		{
			if(i + 1 < argc)
			{
				i++;
				dbg_file_name = argv[i];
			}
			else
			{
				argument_error_msg();
			}
		}
		else if(NULL == micro_file_name && argv[i][0] != '-')
		{
			// 输入的汇编源代码文件路径。
			micro_file_name = argv[i];
		}
		else
		{
			argument_error_msg();
		}
	}

	//
	// 如果命令行参数中没有指定输入的汇编文件，就打印错误信息后退出
	//
	if(NULL == micro_file_name)
	{
		printf("命令行参数错误。没有指定微指令源代码文件的路径。\n");
		help_msg();
	}
}

// 将一个字符串写入二进制文件中。先将字符串长度写入文件，然后将字符串中的
// 每个字符依次写入文件，不包括字符串末尾的 0.
void write_string_to_binary_file(const char* str, FILE* fp)
{
	int str_length;

	if(str != NULL)
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


int main(int argc, char* argv[])
{   
	FILE* fp;
	char* token;
	char line[MAX_LINE_LENGTH];
	int i, j;	
	int line_num = 1;	// 行号从第一行开始计数
	int keyword_index;
	unsigned long micro_code;

	//
	// 处理命令行参数
	//
	process_argument(argc, argv);

	//
	// 打开汇编源代码文件
	//
	fp = fopen(micro_file_name, "r");
	if(NULL == fp)
	{
		printf("无法打开微指令源代码文件 %s\n", micro_file_name);
		return 1;
	}

	////////////////////////////////////////////////////////////////////////////
	//// 扫描

	version_msg();
	printf("\n正在汇编 %s...\n", micro_file_name);

	//
	// 一次读取一行文本，同时记录所有的代码行信息
	//
	while(fgets(line, sizeof(line), fp) != NULL)
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
		if(NULL == token)
		{
			// 如果是空行，不做任何处理
		}
		else if((keyword_index = match_keyword(token)) != 0)
		{
			// 根据关键字进行相应的处理
			keyword_function_table[keyword_index].parse_function(line_num);
		}
		else if(is_immediate(token))
		{
			// 将立即数直接作为微指令
			micro_code = get_value_from_immediate(token);

			memcpy(&machine_code[machine_code_address], &micro_code, 4);
			machine_code_address += 4;
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
		if(line_database[line_count].machine_code_count != 0)
		{
			machine_code_line_count++;
		}

		//
		// 增加行号
		//
		line_count++;
		line_num++;

		if(line_count == MAX_LINE_COUNT)
		{
			sprintf(formated_msg, "微指令文件中的代码行过多，最多只能有 %d 行代码。", MAX_LINE_COUNT);
			error_msg(formated_msg, -1);
		}
	}

	fclose(fp);

	////////////////////////////////////////////////////////////////////////////
	//// 产生各种输出文件

	//
	// 将机器码写入二进制文件
	//
	if(NULL == target_file_name)
	{
		target_file_name = "micro.obj";
	}

	fp = fopen(target_file_name, "wb");
	if(NULL == fp)
	{
		printf("无法打开目标文件 %s\n", target_file_name);
		return 1;
	}

	fwrite(machine_code, 1, machine_code_address, fp);

	fclose(fp);

	printf("\n生成目标文件 %s\n", target_file_name);

	//
	// 将代码行数据库中的信息写入列表文件
	//
	if(list_file_name != NULL)
	{
		fp = fopen(list_file_name, "w");
		if(NULL == fp)
		{
			printf("无法打开列表文件 %s\n", list_file_name);
			return 1;
		}

		for(i=0; i<line_count; i++)
		{
			// 行号
			fprintf(fp, "%04d    ", line_database[i].line_num);

			// 地址和机器码
			if(line_database[i].machine_code_count > 0)
			{
				fprintf(fp, "%02X    ", line_database[i].address);

				for(j=0; j<4; j++)
				{
					fprintf(fp, "%02X ", machine_code[line_database[i].address + j]);
				}

				fprintf(fp, "  ");
			}
			else
			{
				fprintf(fp, "                    ");
			}

			// 源代码
			fprintf(fp, line_database[i].line_string);
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

	if(dbg_file_name != NULL)
	{
		fp = fopen(dbg_file_name, "wb");
		if(NULL == fp)
		{
			printf("无法打开调试信息文件 %s\n", dbg_file_name);
			return 1;
		}

		// 魔数
		fwrite(&dbg_file_magic, 1, sizeof(dbg_file_magic), fp);

		// 版本号
		fwrite(&dbg_file_version, 1, sizeof(dbg_file_version), fp);

		// 源代码文件路径
		write_string_to_binary_file(micro_file_name, fp);

		// 列表文件路径
		write_string_to_binary_file(list_file_name, fp);

		// 代码行数据库中的所有元素
		fwrite(&machine_code_line_count, 1, sizeof(machine_code_line_count), fp);
		for(i=0; i<line_count; i++)
		{
			// 跳过没有产生机器码的代码行
			if(0 == line_database[i].machine_code_count)
			{
				continue;
			}

			fwrite(&line_database[i].line_num, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&line_database[i].flag, 1, sizeof(unsigned long), fp);
		}

		// 没有符号表。但是为了确保调试信息的格式能够被正确读取，必须写入符号表数量为 0
		i = 0;
		fwrite(&i, 1, sizeof(i), fp);
		fclose(fp);

		printf("生成调试信息文件 %s\n", dbg_file_name);
	}

	return 0;
}
