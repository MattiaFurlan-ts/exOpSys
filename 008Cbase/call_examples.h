#ifndef CALL_EXAMPLES_H_
#define CALL_EXAMPLES_H_

#define INNER_ARRAY_LEN 80

struct INNER_ARRAY {
	char data[INNER_ARRAY_LEN];
};


struct INNER_ARRAY return_a_struct();

void pass_a_struct(struct INNER_ARRAY param);


void pass_a_struct_ptr(struct INNER_ARRAY * param);


void function_call_examples(void);

#endif /* CALL_EXAMPLES_H_ */
