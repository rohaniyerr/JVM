#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "read_class.h"

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * case you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplcaseication, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid case `has_value` is true) */
    int32_t value;
} optional_value_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    /* You should remove these casts to void in your solution.
     * They are just here so the code compiles without warnings. */
    size_t pc = 0;
    int32_t *operand_stack = calloc(sizeof(int32_t), (method->code.max_stack));
    int32_t stack_count = 0;
    while (pc < method->code.code_length) {
        switch (method->code.code[pc]) {
            case i_bipush: {
                operand_stack[stack_count] = (int)((int8_t) method->code.code[pc + 1]);
                stack_count += 1;
                pc += 2;
                break;
            }
            case i_iadd: {
                int32_t pushed_val =
                    operand_stack[stack_count - 1] + operand_stack[stack_count - 2];
                operand_stack[stack_count - 2] = pushed_val;
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_return: {
                optional_value_t result = {.has_value = false};
                free(operand_stack);
                return result;
            }
            case i_getstatic: {
                pc += 3;
                break;
            }
            case i_invokevirtual: {
                printf("%i\n", operand_stack[stack_count - 1]);
                stack_count -= 1;
                pc += 3;
                break;
            }
            case i_iconst_m1:
            case i_iconst_0:
            case i_iconst_1:
            case i_iconst_2:
            case i_iconst_3:
            case i_iconst_4:
            case i_iconst_5:
                operand_stack[stack_count] = ((int32_t) method->code.code[pc]) - 3;
                stack_count += 1;
                pc += 1;
                break;
            case i_sipush: {
                signed short val =
                    (method->code.code[pc + 1] << 8) | method->code.code[pc + 2];
                operand_stack[stack_count] = val;
                stack_count += 1;
                pc += 3;
                break;
            }
            case i_isub: {
                int32_t top = (int32_t) operand_stack[stack_count - 1];
                int32_t sec = (int32_t) operand_stack[stack_count - 2];
                operand_stack[stack_count - 2] = sec - top;
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_imul: {
                operand_stack[stack_count - 2] =
                    (int32_t) operand_stack[stack_count - 2] *
                    (int32_t) operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_idiv: {
                assert((int32_t) operand_stack[stack_count - 1] != 0);
                operand_stack[stack_count - 2] =
                    (int32_t) operand_stack[stack_count - 2] /
                    (int32_t) operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_irem: {
                assert((int32_t) operand_stack[stack_count - 1] != 0);
                operand_stack[stack_count - 2] =
                    (int32_t) operand_stack[stack_count - 2] %
                    (int32_t) operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_ineg: {
                int32_t val = (int32_t) operand_stack[stack_count - 1];
                operand_stack[stack_count - 1] = -1 * val;
                pc += 1;
                break;
            }
            case i_ishl: {
                assert(operand_stack[stack_count - 1] >= 0);
                operand_stack[stack_count - 2] = operand_stack[stack_count - 2]
                                                 << operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_ishr: {
                assert(operand_stack[stack_count - 1] >= 0);
                operand_stack[stack_count - 2] =
                    operand_stack[stack_count - 2] >> operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_iushr: {
                assert(operand_stack[stack_count - 1] >= 0);
                u4 val = (u4) operand_stack[stack_count - 2];
                operand_stack[stack_count - 2] = val >> operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_iand: {
                operand_stack[stack_count - 2] =
                    operand_stack[stack_count - 2] & operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_ior: {
                operand_stack[stack_count - 2] =
                    operand_stack[stack_count - 2] | operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_ixor: {
                operand_stack[stack_count - 2] =
                    operand_stack[stack_count - 2] ^ operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_iload: {
                operand_stack[stack_count] = locals[method->code.code[pc + 1]];
                stack_count += 1;
                pc += 2;
                break;
            }
            case i_iload_0:
            case i_iload_1:
            case i_iload_2:
            case i_iload_3:
                operand_stack[stack_count] =
                    locals[((int32_t) method->code.code[pc]) - i_iload_0];
                stack_count += 1;
                pc += 1;
                break;
            case i_istore: {
                locals[method->code.code[pc + 1]] = operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 2;
                break;
            }
            case i_istore_0:
            case i_istore_1:
            case i_istore_2:
            case i_istore_3:
                locals[((int32_t) method->code.code[pc]) - i_istore_0] =
                    operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
            case i_iinc: {
                locals[method->code.code[pc + 1]] += (int8_t) method->code.code[pc + 2];
                pc += 3;
                break;
            }
            case i_ldc: {
                int32_t b = (int32_t) method->code.code[pc + 1];
                operand_stack[stack_count] =
                    ((CONSTANT_Integer_info *) class->constant_pool[b - 1].info)->bytes;
                stack_count += 1;
                pc += 2;
                break;
            }
            case i_ifeq: {
                if (operand_stack[stack_count - 1] == 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_ifne: {
                if (operand_stack[stack_count - 1] != 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_iflt: {
                if (operand_stack[stack_count - 1] < 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_ifge: {
                if (operand_stack[stack_count - 1] >= 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_ifgt: {
                if (operand_stack[stack_count - 1] > 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_ifle: {
                if (operand_stack[stack_count - 1] <= 0) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 1;
                break;
            }
            case i_if_icmpeq: {
                if (operand_stack[stack_count - 2] == operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_if_icmpne: {
                if (operand_stack[stack_count - 2] != operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_if_icmplt: {
                if (operand_stack[stack_count - 2] < operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_if_icmpge: {
                if (operand_stack[stack_count - 2] >= operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_if_icmpgt: {
                if (operand_stack[stack_count - 2] > operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_if_icmple: {
                if (operand_stack[stack_count - 2] <= operand_stack[stack_count - 1]) {
                    pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                    method->code.code[pc + 2]);
                }
                else {
                    pc += 3;
                }
                stack_count -= 2;
                break;
            }
            case i_goto: {
                pc += (int16_t)((method->code.code[pc + 1] << 8) |
                                method->code.code[pc + 2]);
                break;
            }
            case i_ireturn: {
                optional_value_t a;
                a.value = operand_stack[stack_count - 1];
                a.has_value = true;
                free(operand_stack);
                return a;
            }
            case i_invokestatic: {
                u1 b1 = method->code.code[pc + 1];
                u1 b2 = method->code.code[pc + 2];
                int16_t index = (int16_t)((b1 << 8) | b2);
                method_t *meth = find_method_from_index(index, class);
                uint16_t len = get_number_of_parameters(meth);
                int32_t *locals_queue = calloc(sizeof(int32_t), (meth->code.max_locals));
                for (int32_t i = len - 1; i >= 0; i--) {
                    locals_queue[i] = operand_stack[stack_count - 1];
                    stack_count -= 1;
                }
                optional_value_t res = execute(meth, locals_queue, class, heap);
                free(locals_queue);
                if (res.has_value) {
                    operand_stack[stack_count] = res.value;
                    stack_count += 1;
                }
                pc += 3;
                break;
            }
            case i_nop: {
                pc += 1;
                break;
            }
            case i_dup: {
                operand_stack[stack_count] = operand_stack[stack_count - 1];
                stack_count += 1;
                pc += 1;
                break;
            }
            case i_newarray: {
                int32_t count = operand_stack[stack_count - 1];
                assert(count + 1 > 0);
                int32_t *new_array = calloc(sizeof(int32_t), (count + 1));
                new_array[0] = count;
                for (int i = 1; i <= count; i++) {
                    new_array[i] = 0;
                }
                int32_t ref = heap_add(heap, new_array);
                operand_stack[stack_count - 1] = ref;
                pc += 2;
                break;
            }
            case i_arraylength: {
                int32_t ref = operand_stack[stack_count - 1];
                int32_t count = heap_get(heap, ref)[0];
                operand_stack[stack_count - 1] = count;
                pc += 1;
                break;
            }
            case i_areturn: {
                int32_t ref = operand_stack[stack_count - 1];
                optional_value_t ret;
                ret.value = ref;
                ret.has_value = true;
                free(operand_stack);
                return ret;
            }
            case i_iaload: {
                int32_t index = operand_stack[stack_count - 1];
                int32_t ref = operand_stack[stack_count - 2];
                int32_t *arr = heap_get(heap, ref);
                int32_t value = arr[index + 1];
                operand_stack[stack_count - 2] = value;
                stack_count -= 1;
                pc += 1;
                break;
            }
            case i_aload: {
                operand_stack[stack_count] = locals[method->code.code[pc + 1]];
                stack_count += 1;
                pc += 2;
                break;
            }
            case i_aload_0:
            case i_aload_1:
            case i_aload_2:
            case i_aload_3:
                operand_stack[stack_count] =
                    locals[((int32_t) method->code.code[pc]) - i_aload_0];
                stack_count += 1;
                pc += 1;
                break;
            case i_iastore: {
                int32_t value = operand_stack[stack_count - 1];
                int32_t index = operand_stack[stack_count - 2];
                int32_t ref = operand_stack[stack_count - 3];
                int32_t *arr = heap_get(heap, ref);
                arr[index + 1] = value;
                stack_count -= 3;
                pc += 1;
                break;
            }
            case i_astore: {
                locals[method->code.code[pc + 1]] = operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 2;
                break;
            }
            case i_astore_0:
            case i_astore_1:
            case i_astore_2:
            case i_astore_3:
                locals[((int32_t) method->code.code[pc]) - i_astore_0] =
                    operand_stack[stack_count - 1];
                stack_count -= 1;
                pc += 1;
                break;
        }
    }
    // Return void
    optional_value_t result = {.has_value = false};
    free(operand_stack);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
