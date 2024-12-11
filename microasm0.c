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

#define MAX_LINE_LENGTH		256		// һ�д����������ж��ٸ��ַ�
#define MAX_LINE_COUNT		1024	// Դ�����ļ��������ж�����

#define MAX_SYMBOL_LENGTH	64		// һ�����������԰������ٸ��ַ�
#define MAX_SYMBOL_COUNT	256		// Դ�����ļ��������԰������ٸ�����

#ifndef MAX_PATH
#define MAX_PATH 256				// �ļ�·�������԰����������ַ�
#endif

// ���������ݿ⡣��¼Դ�����ļ������д����е���Ϣ������ע���С����У�
struct LINE_RECORD
{
	char line_string[MAX_LINE_LENGTH];	// �����е�����
	unsigned long line_num;				// �к�
	unsigned long address;				// ���д���ת���Ļ�������ӳ���ļ��еĵ�ַ��ƫ�ƣ���ע�⣬���� DM1000 ��˵��΢ָ���һ����ַ��Ӧ 4 ���ֽڡ�
	int machine_code_count;				// ���д���ת���Ļ�������ֽ�����
	unsigned long flag;					// �����б�־λ��32λ
};
struct LINE_RECORD line_database[MAX_LINE_COUNT] = {0};
int line_count = 0;
int machine_code_line_count = 0;	// ��¼�����˻�����Ĵ����е�����

// �ڴ˶������еĴ����б�־λ��ע�⣬�����б�־λ�ǰ�λ��Ĺ�ϵ��
#define LF_INSTRUCTION		0x00000001		// �����б�־λ�����λ��1����ʾ������һ��ָ������ʾ����������
 
//
// �����涨�����еĹؼ���
//
// �Ĵ�������
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

// ��ջ������
const char* csp_counter_keyword = "csp";

//
// alu ������
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

// �������������Ĳ�����
const char* pc_main_memory_keyword = "[pc]";
const char* mar_main_memory_keyword = "[mar]";


const char* delimit_char = "\n\t\r ";			// ��Ҫ���ԵĿհ��ַ�
const char* delimit_char_comma = "\n\t\r, ";	// ��Ҫ���ԵĿհ��ַ�������Ӣ�Ķ���


// ��������΢ָ��
struct ONE_OPERAND_INSTRUCTION_ENTRY
{
	const char** op;
	unsigned long micro_machine_code;
};

struct ONE_OPERAND_INSTRUCTION_ENTRY one_operand_table[] =
{
	{	NULL,						0x0			}	// δ��

	,{	&pc_register_keyword,		0xffffffff	} // inc pc
	,{	&upc_register_keyword,		0xffffffcf  } // reset upc
};


// ����������Ƿ�ƥ�䡣����0����������ƥ�䣻���ط�0��������ƥ�䣬���ҷ���ֵ���ǲ������ڱ��е��±ꡣ
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


// ˫������΢ָ��
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
	{	NULL,							NULL,							0x0			}	// δ��

	// ע�⣺�ڱ��ĵ��У�����΢ָ����붼�Ǵӵ��ֽڵ����ֽڵ�˳�����ġ�
	// ��ȡָ΢ָ�� path [pc], ir Ϊ��������32λ���룬�ӵ��ֽڵ����ֽ�����Ϊ��[7:0]=ef��[15:8]=3f��[23:16]=f9, [31:24]=ff��
	// ����DM1000�У������Ķ�ϰ�ߣ�����λ�ֽڷ���ǰ�棬��λ�ֽڷ��ں��档
	// ��ˣ���Դ���봰�ںʹ洢����������ʾ���ǡ�ef 3f f9 ff������΢ָ�����[31:0]=fff93fef��
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
	,{	&mar_main_memory_keyword,		&a_register_keyword,			0xbff9fbef  } // path [mar], a   �ӵ�ַ�Ĵ���ָ�����ڴ浥Ԫ�����ݵ�a�Ĵ���
	,{	&pc_main_memory_keyword,		&a_register_keyword,			0xbff97fef	} // path [pc], a
	,{	&a_register_keyword,			&rx_register_keyword,			0xd0b4ffef	} // path a, rx
	,{	&a_register_keyword,			&mar_main_memory_keyword,		0xd0f4fbed	} // path a, [mar]  ��a�Ĵ�������д���ַ�Ĵ���ָ����ڴ�
	,{	&pc_main_memory_keyword,		&rx_register_keyword,			0xffb97fef	} // path [pc], rx
	
	,{	&alu_aout_keyword,				&mar_main_memory_keyword,		0xd0f4fbec	} // path alu_aout, [mar]
	,{	&pc_main_memory_keyword,		&pc_register_keyword,			0xfff96fff	} // path [pc], pc

	,{	&pc_register_keyword,			&sp_register_keyword,			0xfff3feef	} // path pc, sp
	,{	&ia_register_keyword,			&mar_register_keyword,			0xfff1f7ef	} // path ia, mar
	,{	&pc_register_keyword,			&mar_register_keyword,			0xfff3f7ef	} // path pc, mar
	,{	&rin_register_keyword,			&a_register_keyword,			0xbff0ffef	} // path rin, a
	,{	&a_register_keyword,			&rout_register_keyword,			0xd0f4ffee	} // path a, rout
	,{	&mar_main_memory_keyword,		&pc_register_keyword,			0xfff9ebef  } // path [mar],pc �ӵ�ַ�Ĵ���ָ�����ڴ浥Ԫ�����ݵ����������pc
	,{	&pc_main_memory_keyword,		&sp_register_keyword,			0xfff97eef	} // path [pc], sp 
	

	,{	&sp_register_keyword,			&mar_register_keyword,			0xfff2f7ef	} // path sp, mar
	,{	&sp_register_keyword,			&csp_counter_keyword,			0xff72ffef	} // path sp, csp	
	,{	&pc_main_memory_keyword,		&ia_register_keyword,			0xfff97feb	} // path [pc], ia	
	,{	&pc_main_memory_keyword,		&asr_register_keyword,			0xfff97def	} // path [pc], asr	

	,{	&csp_counter_keyword,			&mar_register_keyword,			0xfff8f7ef	} // path csp, mar  
	,{	&csp_counter_keyword,			&sp_register_keyword,			0xfff8feef	} // path csp, sp

	,{	&sp_inc_keyword,				&csp_counter_keyword,			0xffffffe7	} // path sp_inc, csp
	,{	&sp_dec_keyword,				&csp_counter_keyword,			0xffffffef	} // path sp_dec, csp

	,{	&pc_register_keyword,			&mar_main_memory_keyword,		0xfff3fbed	} // path pc, [mar] ��pcֵд��marָ��Ĵ洢��Ԫ
	,{	&asr_register_keyword,			&pc_register_keyword,			0xfff7efef	} // path asr, pc 
	,{	&sp_register_keyword,			&asr_register_keyword,			0xfff2fdef	} // path sp, asr 

};


// ����ж������������Ƿ�ƥ�䡣����0������������ȫƥ�䣻���ط�0����������ȫƥ�䣬���ҷ���ֵ���ǲ������ڱ��е��±ꡣ
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

// �������Ļ�����
#define MAX_MACHINE_CODE 1024
BYTE machine_code[MAX_MACHINE_CODE];
unsigned long machine_code_address = 0;		// һ����ַ��Ӧһ���ֽ�
unsigned long machine_code_old_address = 0;


const char* micro_file_name = NULL;				// ΢ָ���ļ�·��
const char* target_file_name = NULL;			// Ŀ���ļ�·��
const char* list_file_name = NULL;				// �б��ļ�·��
const char* dbg_file_name = NULL;				// ������Ϣ�ļ�·��

const unsigned long dbg_file_magic = 58;
const unsigned long dbg_file_version = 1;


// ����������з��ֵ��﷨������Ϣ
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



char formated_msg[1024];	// ����ʽ����Ĵ�����Ϣ���ڴ��ַ����С�
void error_msg_miss_op(const char* instruction_name, int line_num)
{
	sprintf(formated_msg, "%s ָ��ȱ�ٲ�������", instruction_name);
	error_msg(formated_msg, line_num);
}

void error_msg_wrong_op(const char* instruction_name, int line_num)
{
	sprintf(formated_msg, "%s ָ�֧�������Ĳ�������", instruction_name);
	error_msg(formated_msg, line_num);
}


// ����ؼ��ֺͽ��������Ķ�Ӧ��ϵ
typedef void (*PARSE_FUNCTION)(int line_num);
struct KEYWORD_FUNCTION_ENTRY
{
	const char** keyword;
	PARSE_FUNCTION parse_function;
};

// ָ������
const char* dup_instruction_keyword = "dup";
const char* null_instruction_keyword = "null";
const char* path_instruction_keyword = "path";

const char* inc_instruction_keyword = "inc"; // pc+1
const char* reset_instruction_keyword = "reset"; // ��λ

// �ж��Ƿ�������������֧�ָ�����
int is_immediate(const char* token)
{
	return isdigit(token[0]);
}

// �����������õ�ֵ����֧�ָ�����
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
	unsigned long micro_instruction_count;	// ΢ָ�����
	unsigned long micro_code;				// ΢ָ�����

	op1 = strtok(NULL, delimit_char_comma);
	op2 = strtok(NULL, delimit_char);

	if(NULL == op1 || NULL == op2)
	{
		error_msg_miss_op(dup_instruction_keyword, line_num);
	}

	// dup΢ָ��ĵ�һ��������������������(ʮ���ƻ�ʮ������)
	if (!is_immediate(op1))
	{
		error_msg_wrong_op(dup_instruction_keyword, line_num);
	}

	// �õ�΢ָ�����
	micro_instruction_count = get_value_from_immediate(op1);

	// �����ڶ���������
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
	// �ڴ��������ݿ��У���Ǵ�����һ��ָ����
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
	// �ڴ��������ݿ��У���Ǵ�����һ��ָ����
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
	// �ڴ��������ݿ��У���Ǵ�����һ��ָ����
	//
	line_database[line_count].flag |= LF_INSTRUCTION;
}


//
// ���뽫�ؼ��ּ������������������ı��С��Ӷ�����ʹ�á����������ı��ģʽ��
//
struct KEYWORD_FUNCTION_ENTRY keyword_function_table[] =
{
	 {	NULL,								NULL						}	// δ��

	,{	&dup_instruction_keyword,			parse_dup					}
	,{	&null_instruction_keyword,			parse_null					}
	,{	&path_instruction_keyword,			parse_path					}
	,{	&inc_instruction_keyword,			parse_inc					}
	,{	&reset_instruction_keyword,			parse_reset					}
};



// �ж��Ƿ���һ���ؼ��֡�����0�����ǹؼ��֣����ط�0���ǹؼ��֣����ҷ���ֵ���ǹؼ����ڱ��е��±ꡣ
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


// ����汾��Ϣ
void version_msg()
{
	printf(
		"Engintime DM1000 8λģ�ͻ�΢ָ������ [�汾 2.0]\n"
		);
}

// ���������Ϣ
void help_msg()
{
	printf(
		"Engintime DM1000 8λģ�ͻ�΢ָ��������\n\n"
		"�÷�:\n\n"
		"  microasm.exe micro_file_name [options]\n\n"
		"ѡ��:\n\n"
		"  -g debug_file_name\tָ�����ɵĵ�����Ϣ�ļ�·����\n"
		"  -h\t\t\t��ӡ�˰�����Ϣ��\n"
		"  -l list_file_name\tָ�����ɵ��б��ļ�·����\n"
		"  -o target_file_name\tָ�����ɵ�Ŀ���ļ�·������δָ����Ĭ������ micro.obj �ļ���\n"
		"  -v\t\t\t��ӡ�汾��Ϣ��\n"
		);

	printf("\n");

	version_msg();

	exit(1);
}

void argument_error_msg()
{
	printf("�����в�������\n\n");
	help_msg();
}

// �������û�����������в���
void process_argument(int argc, char* argv[])
{
	int i;

	// argv[0] �� "easm.exe"�����Կ��Ժ��ԡ�
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
			// ����Ļ��Դ�����ļ�·����
			micro_file_name = argv[i];
		}
		else
		{
			argument_error_msg();
		}
	}

	//
	// ��������в�����û��ָ������Ļ���ļ����ʹ�ӡ������Ϣ���˳�
	//
	if(NULL == micro_file_name)
	{
		printf("�����в�������û��ָ��΢ָ��Դ�����ļ���·����\n");
		help_msg();
	}
}

// ��һ���ַ���д��������ļ��С��Ƚ��ַ�������д���ļ���Ȼ���ַ����е�
// ÿ���ַ�����д���ļ����������ַ���ĩβ�� 0.
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
	int line_num = 1;	// �кŴӵ�һ�п�ʼ����
	int keyword_index;
	unsigned long micro_code;

	//
	// ���������в���
	//
	process_argument(argc, argv);

	//
	// �򿪻��Դ�����ļ�
	//
	fp = fopen(micro_file_name, "r");
	if(NULL == fp)
	{
		printf("�޷���΢ָ��Դ�����ļ� %s\n", micro_file_name);
		return 1;
	}

	////////////////////////////////////////////////////////////////////////////
	//// ɨ��

	version_msg();
	printf("\n���ڻ�� %s...\n", micro_file_name);

	//
	// һ�ζ�ȡһ���ı���ͬʱ��¼���еĴ�������Ϣ
	//
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		//
		// ��һ�д������Ϣ��¼�����������ݿ���
		//
		strcpy(line_database[line_count].line_string, line);
		line_database[line_count].line_num = line_num;
		line_database[line_count].address = machine_code_address;	

		//
		// ���������е�ע�Ͱ���
		//
		line[strcspn(line, ";")] = 0;

		//
		// ��ʼ����������
		//
		token = strtok(line, delimit_char);
		if(NULL == token)
		{
			// ����ǿ��У������κδ���
		}
		else if((keyword_index = match_keyword(token)) != 0)
		{
			// ���ݹؼ��ֽ�����Ӧ�Ĵ���
			keyword_function_table[keyword_index].parse_function(line_num);
		}
		else if(is_immediate(token))
		{
			// ��������ֱ����Ϊ΢ָ��
			micro_code = get_value_from_immediate(token);

			memcpy(&machine_code[machine_code_address], &micro_code, 4);
			machine_code_address += 4;
		}
		else
		{
			error_msg("�޷�ʶ��Ĵ����С�", line_num);
		}

		//
		// ������������ݿ���Ϣ
		//
		line_database[line_count].machine_code_count = machine_code_address - machine_code_old_address;
		machine_code_old_address = machine_code_address;

		//
		// ��¼�����˻�����Ĵ���������
		//
		if(line_database[line_count].machine_code_count != 0)
		{
			machine_code_line_count++;
		}

		//
		// �����к�
		//
		line_count++;
		line_num++;

		if(line_count == MAX_LINE_COUNT)
		{
			sprintf(formated_msg, "΢ָ���ļ��еĴ����й��࣬���ֻ���� %d �д��롣", MAX_LINE_COUNT);
			error_msg(formated_msg, -1);
		}
	}

	fclose(fp);

	////////////////////////////////////////////////////////////////////////////
	//// ������������ļ�

	//
	// ��������д��������ļ�
	//
	if(NULL == target_file_name)
	{
		target_file_name = "micro.obj";
	}

	fp = fopen(target_file_name, "wb");
	if(NULL == fp)
	{
		printf("�޷���Ŀ���ļ� %s\n", target_file_name);
		return 1;
	}

	fwrite(machine_code, 1, machine_code_address, fp);

	fclose(fp);

	printf("\n����Ŀ���ļ� %s\n", target_file_name);

	//
	// �����������ݿ��е���Ϣд���б��ļ�
	//
	if(list_file_name != NULL)
	{
		fp = fopen(list_file_name, "w");
		if(NULL == fp)
		{
			printf("�޷����б��ļ� %s\n", list_file_name);
			return 1;
		}

		for(i=0; i<line_count; i++)
		{
			// �к�
			fprintf(fp, "%04d    ", line_database[i].line_num);

			// ��ַ�ͻ�����
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

			// Դ����
			fprintf(fp, line_database[i].line_string);
		}

		fclose(fp);

		printf("�����б��ļ� %s\n", list_file_name);
	}

	//
	// �����������ݿ��е���Ϣд������Ƶĵ�����Ϣ�ļ���
	//
	// ������Ϣ�Ļ�����ʽΪ��
	// ħ����4�ֽڣ����̶����ɸı�
	// �汾�ţ�4�ֽڣ����̶����ɸı�
	// Դ�����ļ�����·���ַ������ȣ�4�ֽڣ�
	// Դ�����ļ�����·���ַ������������ַ�����β��0��
	// �б��ļ�����·���ַ������ȣ�4�ֽڣ�
	// �б��ļ�����·���ַ������������ַ�����β��0��
	//
	// ���������ݿ���Ԫ�ص�������4�ֽڣ�
	// ���������ݿ��е�����Ԫ��
	//
	// ���ű�Ԫ�ص�������4�ֽڣ�
	// ���ű��е�����Ԫ��
	//

	if(dbg_file_name != NULL)
	{
		fp = fopen(dbg_file_name, "wb");
		if(NULL == fp)
		{
			printf("�޷��򿪵�����Ϣ�ļ� %s\n", dbg_file_name);
			return 1;
		}

		// ħ��
		fwrite(&dbg_file_magic, 1, sizeof(dbg_file_magic), fp);

		// �汾��
		fwrite(&dbg_file_version, 1, sizeof(dbg_file_version), fp);

		// Դ�����ļ�·��
		write_string_to_binary_file(micro_file_name, fp);

		// �б��ļ�·��
		write_string_to_binary_file(list_file_name, fp);

		// ���������ݿ��е�����Ԫ��
		fwrite(&machine_code_line_count, 1, sizeof(machine_code_line_count), fp);
		for(i=0; i<line_count; i++)
		{
			// ����û�в���������Ĵ�����
			if(0 == line_database[i].machine_code_count)
			{
				continue;
			}

			fwrite(&line_database[i].line_num, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].address, 1, sizeof(unsigned long), fp);
			fwrite(&line_database[i].machine_code_count, 1, sizeof(int), fp);
			fwrite(&line_database[i].flag, 1, sizeof(unsigned long), fp);
		}

		// û�з��ű�����Ϊ��ȷ��������Ϣ�ĸ�ʽ�ܹ�����ȷ��ȡ������д����ű�����Ϊ 0
		i = 0;
		fwrite(&i, 1, sizeof(i), fp);
		fclose(fp);

		printf("���ɵ�����Ϣ�ļ� %s\n", dbg_file_name);
	}

	return 0;
}
