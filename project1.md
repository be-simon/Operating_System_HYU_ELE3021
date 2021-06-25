# 스케줄러 디자인

- MLFQ + Stride Scheduling
- 1tick = 10ms (timer interrupts)
- 10000장의 티켓을 두 스케줄러의 비율만큼 배분한다.
    - 모든 프로세스는 첫 실행시 MLFQ에 위치한다.
    - Stride Scheduler에 프로세스가 없으면 티켓은 모두 MLFQ에게 있다.
    - 프로세스가 set_cpu_share() 시스템콜을 호출하여 cpu를 요구하면 Stride Scheduler로 넘어가고, Stride Scheduler는 안에 있는 프로세스들이 점유하는 cpu %의 합만큼 티켓을 갖는다.

### Scenario

- 전체 티켓이 10000장이고 프로세스 A, B가 MLFQ에 존재한다.

    → MLFQ가 티켓 1000장을 모두 가지고, stride는 0장

- 프로세스 C, D가 들어와서 각각 cpu 40%, 10%를 요구한다
    - MLFQ가 5000장, stride가 5000장
    - Stride가 가진 5000장 중 C는 4000장을, D는 1000장을 가진다.
- 스케줄러는 먼저 스케줄러 티켓 비율에 따라 MLFQ와 Stride를 정한다.
    - MLFQ 스케줄러가 할당 받은 경우, MLFQ는 자체 스케줄링 정책에 따라 다음 프로세스를 실행한다.
    - Stride 스케줄러가 할당 받은 경우, 다시 내부 티켓의 비율에 따라 다음 프로세스를 정한다.
- MLFQ에 있는 프로세스가 cpu 점유를 요청하면 전체에서 해당 %만큼의 티켓을 MLFQ → stride로 옮긴다.
    - 예를 들어 프로세스 E가 새로 들어와 cpu 30%를 요구하는 경우 MLFQ가 가진 티켓 3000장을 Stride에 넘기고, E가 그 3000장의 티켓을 소유한다.
- stride에 처음 들어온 프로세스는 pass value를 가장 낮은 프로세스에 맞춰서 우선순위를 높여준다.
- 만약 어떤 프로세스가 cpu 점유를 요청했을 때, Stride Scheduler에 있는 모든 프로세스의 점유율 합이 80%를 넘게 되면 요청에 실패한다.

# 구현

### 프로세스와 스케줄러

- 프로세스 구조체에 mlfq를 위한 q-level, 해당 레벨에서 사용한 tick, stride 를 위한 tickets, stride, pass를 선언한다.
- mlfq와 stride 스케줄러 모델로 스케줄러 구조체를 만든다.
    - 각 스케줄러는 각자의 티켓을 가지고 있어서 그 비율에 따라 선택된다.
    - 각 스케줄러에 있는 프로세스들은 heap으로 관리된다.

## MLFQ

- 3-level queue
- 최소한 20%의 CPU 점유를 보장한다.

### time quantum of queue

- highest - 1tick
- middle - 2ticks
- lowest - 4ticks

### time allotment of queue

- highest - 5tick
- middle - 10ticks

### priority boosting

- every 100ticks

### 동작

- trap.c 함수에서 타이머 인터럽트가 걸릴 때마다 각 프로세스의 사용량을 체크해준다.
    - mlfq에 있는 프로세스는 큐 레벨에 따라 time quantum과 allotment를 체크해준다.
    - mlfq에 있는 프로세스가 할당될 때 마다 mlfqtick를 올려주어 priority boost 때 사용한다.
    - time quantum이나 time allotment를 다 썼을 때 yield()를 호출해주기 때문에 그 전에는 스케줄러로 분기하지 않고 해당 프로세스가 계속해서 점유한다.
    - mlfq 스케줄러 전체의 pass도 증가해준다.
- scheduler.c 파일에 mlfq_enqueue(), mlfq_dequeue() 함수를 구현한다.
    - mlfqsched의 heap은 들어있는 프로세스들의 q-level을 비교하는 min_heap이므로 가장 위에 큐에 있는 프로세스를 가져올 수 있다.

## Stride

- 최대로 가능한 CPU 사용 - 80%
    - 80% 이상이 될 경우, cpu 점유 요청 실패

### 동작

- trap.c 함수에서 타이머 인터럽트가 걸리면 cpu를 내어놓으면서 pass를 증가한다.
    - stride 스케줄러 전체의 pass도 증가해준다.
- scheduler.c 파일에 stride_enqueue(), stride_dequeue() 함수를 구현한다.
    - stridesched의 heap은 들어있는 프로세스들의 pass를 비교하는 min_heap이므로 pass가 가장 적은 프로세스를 가져올 수 있다.

## 스케줄러 함수

- 먼저 mlfq와 stride 스케줄러 중 pass가 적은 스케줄러를 선택한다.
    - 기본적으로는 Mlfq 스케줄러를 선택한다.
- 각 스케줄러에서는 다음 RUNNABLE한 프로세스를 선정하고 컨텍스트 스위치를 한다.
- 다시 스케줄러로 스위치 되어 오면 이전 프로세스를 다시 스케줄러에 넣어준다.
- mlfq→stride 큐로 간 프로세스, RUNNABLE이 아닌 프로세스들은 큐에서 꺼낸 다음에 예외처리를 해준다.

### 구현할 System call

- yield
- getlev
    - myproc()의 qlev을 리턴한다.
- set_cpu_share
    - 인자로 받은 share만큼의 티켓을 mlfq에서 stride 스케줄러로 이동해준다.
    - 바뀐 티켓에 맞춰 stride를 계산한다.
    - 호출한 프로세스의 tickets와 stride를 계산해주고 stride 스케줄러에 enqueue한다.
    - 요구한 share로 인해 stride 스케줄러가 MAXSHARE를 넘게되면 -1을 리턴한다.

## Test Code Result
![스크린샷_2021-04-24_오후_10.40.55](uploads/64895caacc1cbb708925942350cd634b/스크린샷_2021-04-24_오후_10.40.55.png)
![_2021-04-24__10.45.22](uploads/5ff4a5104c1f5f9ce3e69b89528d485f/_2021-04-24__10.45.22.png)
- 테스트 코드는 동일 시간 내에 스케줄이 많이 될 수록 카운트가 높아진다.
- stride 스케줄러는 share가 높은 프로세스가 더 높다.
- mlfq 스케줄러는 높은 큐일 수록 time allotment가 짧고 그만큼 체류시간이 짧기 때문에 아래 큐일 수록 카운트가 높다.
