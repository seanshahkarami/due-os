// NOTE Currently uses a slightly modifed version of Due core. Only critical change is
// putting a weak alias to PendSV_Handler to allow directly handling the PendSV exception
// for context switching.

volatile uint32_t *ICSR = (uint32_t *)0xe000ed04;

struct Task {
  uint32_t state;
  uint32_t *sp;
};

Task tasks[3];
int curtask = 0;
int maxtasks = 3;

static inline void yieldTask() {
  // Trigger PendSV interrupt which handles actual context switch.
  *ICSR = 1 << 28;
}

void sleep(uint32_t duration) {
  uint32_t start = millis();

  while (millis() - start < duration) {
    yieldTask();
  }
}

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

  // select next task - will run forever if no task available
  for (;;) {
    curtask = (curtask + 1) % maxtasks;

    if (tasks[curtask].state == 1) {
      break;
    }
  }

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
    sleep(10000);
  }
}

void task2() {
  pinMode(13, OUTPUT);

  for (int i = 0; i < 1000; i++) {
    digitalWrite(13, HIGH);
    sleep(100);
    digitalWrite(13, LOW);
    sleep(100);
  }
}

void cleanupTask() {
  tasks[curtask].state = 0;
  yieldTask();
}

// NOTE Should be able to implement in just the standard pendSV handler???
// We basically store the state of the inner call anyway, so it gets fixed

void setupTask(Task *task, uint32_t *sp, void (*func)()) {
  // hardware / exception frame
  *(--sp) = 0x21000000;             // psr init state
  *(--sp) = (uint32_t)func;         // pc
  *(--sp) = (uint32_t)cleanupTask;  // lr
  *(--sp) = 12;                     // r12
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
  task->state = 1;
}

uint32_t stack1[128];
uint32_t stack2[128];

void setup() {
  SerialUSB.begin(0);
  
  // task 0 is the entry task - already correctly setup.
  tasks[0].state = 1;

  setupTask(&tasks[1], &stack1[128], task1);
  setupTask(&tasks[2], &stack2[128], task2);
}

void readInput(const char *prompt, char *buf, int maxlen) {
  int len = 0;

  SerialUSB.print(prompt);

  for (;;) {
    while (SerialUSB.available() == 0) {
      yieldTask();
    }

    int c = SerialUSB.read();

    // echo character
    SerialUSB.print((char)c);

    if (c == '\n' || c == '\r') {
      SerialUSB.println();
      break;
    }

    if (len < maxlen-1) {
      buf[len++] = c;
    }
  }

  buf[len] = 0;
}

void showProcesses() {
  for (int i = 0; i < maxtasks; i++) {
    SerialUSB.print("task ");
    SerialUSB.print(i);
    SerialUSB.print(" ");

    if (tasks[i].state == 1) {
      SerialUSB.print("running");
    } else {
      SerialUSB.print("stopped");
    }

    SerialUSB.println();
  }
}

void showUptime() {
  SerialUSB.print(millis());
  SerialUSB.println(" ms");
}

struct Command {
  const char *name;
  void (*func)();
};

Command commands[] = {
    {"ps", showProcesses},
    {"uptime", showUptime},
};

const int numcommands = sizeof(commands) / sizeof(Command);

void processCommand(const char *buf) {
  if (strlen(buf) == 0) {
    return;
  }

  for (int i = 0; i < numcommands; i++) {
    if (strcmp(buf, commands[i].name) == 0) {
      commands[i].func();
      return;
    }
  }
  
  SerialUSB.print("? ");
  SerialUSB.println(buf);
}

void loop() {
  char buf[256];
  readInput("$ ", buf, 256);
  processCommand(buf);
}
