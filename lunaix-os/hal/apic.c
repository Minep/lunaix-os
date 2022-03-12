/**
 * @file apic.c
 * @author Lunaixsky
 * @brief Abstraction for Advanced Programmable Interrupts Controller (APIC)
 * @version 0.1
 * @date 2022-03-06
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <hal/apic.h>
#include <hal/cpu.h>
#include <hal/pic.h>
#include <hal/rtc.h>

#include <arch/x86/interrupts.h>

#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("APIC")

void
apic_setup_timer();

void
apic_setup_lvts();

void
init_apic()
{
    // ensure that external interrupt is disabled
    cpu_disable_interrupt();

    // Make sure the APIC is there
    //  FUTURE: Use 8259 as fallback
    assert_msg(cpu_has_apic(), "No APIC detected!");

    // As we are going to use APIC, disable the old 8259 PIC
    pic_disable();

    // Hardware enable the APIC
    // By setting bit 11 of IA32_APIC_BASE register
    // Note: After this point, you can't disable then re-enable it until a reset (i.e., reboot)
    asm volatile (
        "movl %0, %%ecx\n"
        "rdmsr\n"
        "orl %1, %%eax\n"
        "wrmsr\n"
        ::"i"(IA32_APIC_BASE_MSR), "i"(IA32_APIC_ENABLE)
        : "eax", "ecx", "edx"
    );

    // Print the basic information of our current local APIC
    uint32_t apic_id = apic_read_reg(APIC_IDR) >> 24;
    uint32_t apic_ver = apic_read_reg(APIC_VER);

    kprintf(KINFO "ID: %x, Version: %x, Max LVT: %u\n",
           apic_id,
           apic_ver & 0xff,
           (apic_ver >> 16) & 0xff);

    // initialize the local vector table (LVT)
    apic_setup_lvts();

    // initialize priority registers
    
    // set the task priority to the lowest possible, so all external interrupts are acceptable
    //   Note, the lowest possible priority class is 2, not 0, 1, as they are reserved for
    //   internal interrupts (vector 0-31, and each p-class resposible for 16 vectors). 
    //   See Intel Manual Vol. 3A, 10-29
    apic_write_reg(APIC_TPR, APIC_PRIORITY(2, 0));

    // enable APIC
    uint32_t spiv = apic_read_reg(APIC_SPIVR);

    // install our handler for spurious interrupt.
    spiv = (spiv & ~0xff) | APIC_SPIV_APIC_ENABLE  | APIC_SPIV_IV;
    apic_write_reg(APIC_SPIVR, spiv);

    // setup timer and performing calibrations
    apic_setup_timer();
}

#define LVT_ENTRY_LINT0(vector)           (LVT_DELIVERY_FIXED | vector)

// Pin LINT#1 is configured for relaying NMI, but we masked it here as I think
//  it is too early for that
// LINT#1 *must* be edge trigged (Intel manual vol3. 10-14)
#define LVT_ENTRY_LINT1                   (LVT_DELIVERY_NMI | LVT_MASKED | LVT_TRIGGER_EDGE)
#define LVT_ENTRY_ERROR(vector)           (LVT_DELIVERY_FIXED | vector)
#define LVT_ENTRY_TIMER(vector, mode)     (LVT_DELIVERY_FIXED | mode | vector)

void
apic_setup_lvts()
{
    apic_write_reg(APIC_LVT_LINT0, LVT_ENTRY_LINT0(APIC_LINT0_IV));
    apic_write_reg(APIC_LVT_LINT1, LVT_ENTRY_LINT1);
    apic_write_reg(APIC_LVT_ERROR, LVT_ENTRY_ERROR(APIC_ERROR_IV));
}

void
temp_intr_routine_rtc_tick(const isr_param* param);

void
temp_intr_routine_apic_timer(const isr_param* param);

void
test_timer(const isr_param* param);

uint32_t apic_timer_base_freq = 0;

// Don't optimize them! Took me an half hour to figure that out...

volatile uint32_t rtc_counter = 0;
volatile uint8_t apic_timer_done = 0;

#define APIC_CALIBRATION_CONST  0x100000

void
apic_setup_timer()
{
    cpu_disable_interrupt();
    
    // Setup APIC timer

    // Setup a one-shot timer, we will use this to measure the bus speed. So we can
    //   then calibrate apic timer to work at *nearly* accurate hz
    apic_write_reg(APIC_TIMER_LVT,  LVT_ENTRY_TIMER(APIC_TIMER_IV, LVT_TIMER_ONESHOT));
    
    // Set divider to 64
    apic_write_reg(APIC_TIMER_DCR, APIC_TIMER_DIV64);

    /*
        Timer calibration process - measure the APIC timer base frequency

         step 1: setup a temporary isr for RTC timer which trigger at each tick (1024Hz)
         step 2: setup a temporary isr for #APIC_TIMER_IV
         step 3: setup the divider, APIC_TIMER_DCR
         step 4: Startup RTC timer
         step 5: Write a large value, v, to APIC_TIMER_ICR to start APIC timer 
                 (this must be followed immediately after step 4)
         step 6: issue a write to EOI and clean up.

        When the APIC ICR counting down to 0 #APIC_TIMER_IV triggered, save the rtc timer's 
        counter, k, and disable RTC timer immediately (although the RTC interrupts should be 
        blocked by local APIC as we are currently busy on handling #APIC_TIMER_IV)

        So the apic timer frequency F_apic in Hz can be calculate as
            v / F_apic = k / 1024
            =>  F_apic = v / k * 1024

    */

    apic_timer_base_freq = 0;
    rtc_counter = 0;
    apic_timer_done = 0;

    intr_subscribe(APIC_TIMER_IV, temp_intr_routine_apic_timer);
    intr_subscribe(RTC_TIMER_IV, temp_intr_routine_rtc_tick);

        
    rtc_enable_timer();                                         // start RTC timer
    apic_write_reg(APIC_TIMER_ICR, APIC_CALIBRATION_CONST);     // start APIC timer
    
    // enable interrupt, just for our RTC start ticking!
    cpu_enable_interrupt();

    wait_until(apic_timer_done);
    
    // cpu_disable_interrupt();

    assert_msg(apic_timer_base_freq, "Fail to initialize timer");

    kprintf(KINFO "Timer base frequency: %u Hz\n", apic_timer_base_freq);

    // cleanup
    intr_unsubscribe(APIC_TIMER_IV, temp_intr_routine_apic_timer);
    intr_unsubscribe(RTC_TIMER_IV, temp_intr_routine_rtc_tick);

    // TODO: now setup timer with our custom frequency which we can derived from the base frequency
    //          we measured

    apic_write_reg(APIC_TIMER_LVT,  LVT_ENTRY_TIMER(APIC_TIMER_IV, LVT_TIMER_PERIODIC));
    intr_subscribe(APIC_TIMER_IV, test_timer);
    
    apic_write_reg(APIC_TIMER_ICR, apic_timer_base_freq);
}

volatile rtc_datetime datetime;

void
test_timer(const isr_param* param) {

    rtc_get_datetime(&datetime);

    kprintf(KWARN "%u/%02u/%02u %02u:%02u:%02u\r",
           datetime.year,
           datetime.month,
           datetime.day,
           datetime.hour,
           datetime.minute,
           datetime.second);
}

void
temp_intr_routine_rtc_tick(const isr_param* param) {
    rtc_counter++;

    // dummy read on register C so RTC can send anther interrupt
    //  This strange behaviour observed in virtual box & bochs 
    (void) rtc_read_reg(RTC_REG_C);
}

void
temp_intr_routine_apic_timer(const isr_param* param) {
    apic_timer_base_freq = APIC_CALIBRATION_CONST / rtc_counter * RTC_TIMER_BASE_FREQUENCY;
    apic_timer_done = 1;

    rtc_disable_timer();
}