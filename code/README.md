Real-Time Fish Classification System

This project simulates a real-time fish size and weight classification pipeline. The system uses a multithreaded producer-consumer architecture to model sensor input, classification, actuator routing, deadline monitoring, and event logging.

Technologies:
- C
- pthread
- Ubuntu
- Makefile
- POSIX real-time scheduling (`SCHED_FIFO` with graceful fallback)

File Structure:
- `main.c`: main pipeline simulation with Sensor, Classification, and Actuator threads
- `fish_system.h` / `fish_system.c`: core data model, validation, and fish classification logic
- `queue.h` / `queue.c`: thread-safe shared queue implementation using mutex and condition variables
- `logger.h` / `logger.c`: terminal and file logging utilities
- `Makefile`: build rules for the project

System Flow:
Sensor Thread -> Input Queue -> Classification Thread -> Output Queue -> Actuator Thread

Run Modes:
- `DEMO_MODE`: readable 5-fish scenario with detailed terminal output
- `STRESS_MODE`: batch simulation with at least 100 fish, pseudo-random data, intentional invalid samples, and controlled deadline misses
- Mode selection is done at compile time in `main.c` with simple macros:
  `#define DEMO_MODE 1`
  `#define STRESS_MODE 0`
- In `STRESS_MODE`, terminal output stays compact while `fish_system.log` keeps detailed event records

Deadline:
- Deadline is `3 ms`
- Latency is measured with `clock_gettime(CLOCK_MONOTONIC)`
- End-to-end latency is calculated from sensor production time to actuator routing time

Scheduling:
- The program attempts to start threads with `SCHED_FIFO`
- Suggested priorities:
  Sensor Thread = high
  Classification Thread = medium
  Actuator Thread = high/medium
- If the system does not allow real-time scheduling without root privileges, the program prints a warning and continues with the default scheduler

Error Conditions:
- `ERR_INVALID_DATA`
- `ERR_DEADLINE_MISS`

Log File:
- `fish_system.log`

Performance Summary:
- Printed at the end of the program
- Includes:
  `total_fish`, `deadline_ok_count`, `deadline_miss_count`, `invalid_data_count`, `average_latency_ms`, `max_latency_ms`
- Calculated in the Actuator Thread after final routing and deadline evaluation

Build:
```sh
make clean
make
```

Run:
```sh
./main
```

Expected Example Behavior:
- The system processes multiple fish samples through the pipeline
- `fish_id=5` triggers a controlled deadline miss test scenario
- When the deadline is missed, the result is logged with `status=DEADLINE_MISS`
- A missed deadline is routed to `destination=LATE_REJECT`
