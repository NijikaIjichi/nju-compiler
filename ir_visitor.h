#ifndef __IR_VISITOR_H__
#define __IR_VISITOR_H__

void ir_hole_opt();
int ir_dump(const char *file);
void build_program();
int ir_livevar(int final);
void ir_mips();
int ir_avexpr(int final);
int ir_constant();
int ir_arthprog(int final);

#endif
