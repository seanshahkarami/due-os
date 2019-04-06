// NOTE Currently uses a slightly modifed version of Due core. Only critical change is
// putting a weak alias to PendSV_Handler to allow directly handling the PendSV exception
// for context switching.

volatile uint32_t *ICSR = (uint32_t *)0xe000ed04;

struct Task {
  uint32_t *sp;
};

Task tasks[3];
int curtask = 0;
int maxtasks = 3;

static inline void yieldTask() {
  // Trigger PendSV interrupt which handles actual context switch.
  *ICSR = 1 << 28;
}

// Q - does returning from exception automatically restore the "hardware" stack? r0,r...
__attribute__((naked)) void PendSV_Handler() {
  // save context
  asm volatile(
      "mrs r0, msp\n"
      "stmfd r0!, {r4-r11}\n"
      "msr msp, r0\n"
  );

  // save sp for current task
  asm volatile(
    "mrs %0, msp" : "=r"(tasks[curtask].sp)
  );

  // select next task
  curtask = (curtask + 1) % maxtasks;

  // load sp for new task
  asm volatile(
    "msr msp, %0\n" : : "r"(tasks[curtask].sp)
  );

  // load context
  asm volatile(
      "mrs r0, msp\n"
      "ldmfd r0!, {r4-r11}\n"
      "msr msp, r0\n"
  );

  // return in thread mode using state at msp
  asm volatile(
      "mov lr, #0xfffffff9\n"
      "bx lr\n"
  );
}

void task1() {
  for (;;) {
    SerialUSB.println("hello from task1");
    delay(1000);
    yieldTask();
  }
}

void task2() {
  for (int i = 0; i < 3; i++) {
    SerialUSB.println("hello from task2");
    delay(1000);
    yieldTask();
  }
}

void taskdone() {
  SerialUSB.println("task done!");
  // should cleanup and remove task from list!

  for (;;) {
    yieldTask();
  }
}

// NOTE Should be able to implement in just the standard pendSV handler???
// We basically store the state of the inner call anyway, so it gets fixed

void setupTask(Task *task, uint32_t *sp, void (*func)()) {
  // hardware / exception frame
  *(--sp) = 0x21000000;          // psr init state
  *(--sp) = (uint32_t)func;      // pc
  *(--sp) = (uint32_t)taskdone;  // lr
  *(--sp) = 12;                  // r12
  *(--sp) = 3;
  *(--sp) = 2;
  *(--sp) = 1;
  *(--sp) = 0;

  // software / user frame
  *(--sp) = 11;
  *(--sp) = 10;
  *(--sp) = 9;
  *(--sp) = 8;
  *(--sp) = 7;
  *(--sp) = 6;
  *(--sp) = 5;
  *(--sp) = 4;

  task->sp = sp;
}

uint32_t stack1[128];
uint32_t stack2[128];

void setup() {
  SerialUSB.begin(0);
  setupTask(&tasks[1], &stack1[128], task1);
  setupTask(&tasks[2], &stack2[128], task2);
}

void loop() {
  SerialUSB.println("hello from loop");
  delay(1000);
  yieldTask();
}
