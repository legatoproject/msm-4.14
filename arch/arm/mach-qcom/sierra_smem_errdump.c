/* arch/arm/mach-msm/sierra_smem_errdump.c
 *
 * Sierra SMEM utility functions. These functions don't rely on Sierra SMEM driver,
 * and can be used in early kernel start (after paging_init)
 *
 * Copyright (c) 2012 Sierra Wireless, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/random.h>
#include <asm/stacktrace.h>

#include <mach/sierra_smem.h>

static DEFINE_MUTEX(errdump_lock);

static struct sER_DATA *sierra_smem_get_dump_buf(void)
{
        unsigned char *virtual_addr;

        virtual_addr = sierra_smem_base_addr_get();

        if (virtual_addr) {

                /*  APPL ER ABORT: ERR region, 2nd dump */
                virtual_addr += (BSMEM_ERR_OFFSET + BS_SMEM_ERR_DUMP_SIZE);
                return (struct sER_DATA *)virtual_addr;
        }
        else {
                return NULL;
        }
}

void sierra_smem_errdump_save_start(void)
{
        struct sER_DATA *errdatap = sierra_smem_get_dump_buf();

        if (!errdatap) {
                return;
        }

        if (mutex_trylock(&errdump_lock)) {
                /* note that the errdatap can only be accessible after
                 * paging_init at kernel start. If there is a panic
                 * before paging_init, the following line will likely
                 * cause another panic 
                 */
                memset((void *)errdatap, 0x00, sizeof(struct sER_DATA));
                mutex_unlock(&errdump_lock);
        }
        /* else, reentry, don't save */
}
EXPORT_SYMBOL(sierra_smem_errdump_save_start);

void sierra_smem_errdump_save_timestamp(uint32_t time_stamp)
{
        struct sER_DATA *errdatap = sierra_smem_get_dump_buf();

        if (!errdatap) {
                return;
        }

        if (mutex_trylock(&errdump_lock)) {
                if (errdatap->time_stamp == 0) {
                        errdatap->time_stamp = time_stamp;
                }
                /* else time_stamp has something, should not happen since
                 * it should be cleared at sierra_smem_errdump_save_start
                 */

                mutex_unlock(&errdump_lock);
        }
        /* else, reentry, don't save */
}
EXPORT_SYMBOL(sierra_smem_errdump_save_timestamp);

void sierra_smem_errdump_save_errstr(char *errstrp)
{
        struct sER_DATA *errdatap = sierra_smem_get_dump_buf();

        if (!errdatap) {
                return;
        }

        if (mutex_trylock(&errdump_lock)) {

                if (errdatap->error_string[0] == 0x00) {

                        errdatap->start_marker = ERROR_START_MARKER;
                        errdatap->error_source = ERROR_FATAL_ERROR;

                        strncpy(errdatap->error_string, errstrp, ERROR_STRING_LEN);
                        errdatap->error_string[ERROR_STRING_LEN - 1] = 0x00;

                        /* also write ID and proc type here */
                        get_random_bytes(&errdatap->error_id, sizeof(errdatap->error_id));
                        errdatap->proc_type = ERDUMP_PROC_TYPE_APPS;

                        errdatap->end_marker = ERROR_END_MARKER;
                }
                /* else error_string has something, should not happen since
                 * it should be cleared at sierra_smem_errdump_save_start
                 */
    
                mutex_unlock(&errdump_lock);
        }
        /* else, reentry, don't save */
}
EXPORT_SYMBOL(sierra_smem_errdump_save_errstr);

void sierra_smem_errdump_save_auxstr(char *errstrp)
{
        struct sER_DATA *errdatap = sierra_smem_get_dump_buf();

        if (!errdatap) {
                return;
        }

        if (mutex_trylock(&errdump_lock)) {

                if (errdatap->aux_string[0] == 0x00) {

                        strncpy(errdatap->aux_string, errstrp, ERROR_STRING_LEN);
                        errdatap->aux_string[ERROR_STRING_LEN - 1] = 0x00;
                }
                /* else format_string has something, should not happen since
                 * it should be cleared at sierra_smem_errdump_save_start
                 */

                mutex_unlock(&errdump_lock);
        }
        /* else, reentry, don't save */
}
EXPORT_SYMBOL(sierra_smem_errdump_save_auxstr);

void sierra_smem_errdump_save_frame(void *taskp, void *framedatap)
{
        struct sER_DATA *errdatap = sierra_smem_get_dump_buf();
        unsigned long *stackp, stack_index;
        struct stackframe *framep = (struct stackframe *)framedatap;

        if (!errdatap) {
                return;
        }

        if (mutex_trylock(&errdump_lock)) {

                /* kernel warning (unwind) will try to save frame without
                 * error_string and that can result a ghost gcdump
                 * Check valid error_string first before saving frame
                 */
                if (errdatap->error_string[0] &&
                    errdatap->program_counter == 0) {

                        errdatap->program_counter = framep->pc;
                        errdatap->registers[11] = framep->fp; 
                        errdatap->registers[13] = framep->sp; 
                        errdatap->registers[14] = framep->lr;

                        /* use frame pointer which is one step closer
                         * than stack pointer 
                         * taskp != 0: kernel space stack processing
                         */
                        if (taskp && framep->fp) {

                                stackp = (unsigned long *)framep->fp;
                                /* match mpss side pattern */
                                for (stack_index = 0; stack_index < MAX_STACK_DATA; stack_index ++) {
                                        errdatap->stack_data[MAX_STACK_DATA - stack_index - 1] = stackp[stack_index];
                                }
                        }
                        /* taskp == 0: user space stack processing: */
                        else if (taskp == 0 && access_ok(VERIFY_READ, (char __user *)framep->fp, MAX_STACK_DATA * sizeof(long))) {

                                /* match mpss side pattern */
                                for (stack_index = 0; stack_index < MAX_STACK_DATA; stack_index ++) {

                                        get_user(errdatap->stack_data[MAX_STACK_DATA - stack_index - 1],
                                                 (unsigned long __user *)(framep->fp + (sizeof(long) * stack_index)));
                                }
                        }

                        sprintf(errdatap->task_name, "%08X", (unsigned int)taskp);

                }
                /* else pc has something, should not happen since
                 * it should be cleared at sierra_smem_errdump_save_start
                 */

                mutex_unlock(&errdump_lock);
        }
        /* else, reentry, don't save */
}
EXPORT_SYMBOL(sierra_smem_errdump_save_frame);
