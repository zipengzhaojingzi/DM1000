; 示例程序

.text
mov r0, 16		;将立即数 16 传送到寄存器 r0
mov a, r0
add a, 250
;mov a, num     	;将标号 num 指定的存储单元内容复制到累加器 a 中
;add a, r0      	;将累加器 a 与寄存器 r0 相加，结果写回 a 中
;shl a, 3
;rcl a
rcrnum a,4
rclnum a,5
shrnum a,6
shlnum a,7


