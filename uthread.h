#ifndef MY_UTHREAD_H
#define MY_UTHREAD_H

// 在苹果系统上启用X/Open标准功能，确保ucontext.h可用
#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif 

#include <ucontext.h>
#include <vector>

#define DEFAULT_STACK_SIZE (1024*128)    //默认栈空间大小 128KB
#define MAX_UTHREAD_SIZE   1024          //最大线程数量

//协程状态空间
enum ThreadState{
    FREE,        //空闲状态，协程槽位未被使用
    RUNNABLE,    //可运行状态，协程已创建正在等待执行
    RUNNING,     //运行状态，协程正在执行
    SUSPEND      //挂起状态，协程主动让出 CPU
};

// 前向声明调度器结构体，因为uthread_t和schedule_t相互引用
struct schedule_t;

// 协程函数指针类型定义：所有协程函数必须符合此签名
typedef void (*Fun)(void *arg);

// 单个协程的结构体定义
typedef struct uthread_t
{
    ucontext_t ctx;                    // 协程的上下文信息，保存寄存器状态、栈指针等
    Fun func;                          // 协程要执行的目标函数
    void *arg;                         // 传递给目标函数的参数
    enum ThreadState state;            // 协程当前的生命周期状态
    char stack[DEFAULT_STACK_SIZE];    // 协程独立的栈空间（每个协程128KB）
}uthread_t;

// 调度器结构体定义：管理所有协程的创建、调度和销毁
typedef struct schedule_t
{
    ucontext_t main;           // 主线程（调度器）的上下文，协程执行完后会回到这里
    int running_thread;        // 当前正在运行的协程ID（-1表示没有协程在运行）
    uthread_t *threads;        // 协程数组指针，预分配MAX_UTHREAD_SIZE个协程槽位
    int max_index;             // 曾经使用到的最大的index + 1

    // 构造函数：初始化调度器
    schedule_t():running_thread(-1), max_index(0) {
        // 动态分配协程数组内存
        threads = new uthread_t[MAX_UTHREAD_SIZE];
        // 初始化所有协程槽位为空闲状态
        for (int i = 0; i < MAX_UTHREAD_SIZE; i++) {
            threads[i].state = FREE;
        }
    }

    // 析构函数：清理调度器占用的资源
    ~schedule_t() {
        delete [] threads;
    }
}schedule_t;

/* 
 * 协程入口包装函数（内部使用，不对外暴露）
 * 功能：作为所有协程的实际入口点，负责调用用户函数并处理协程结束逻辑
 * 参数：ps - 指向调度器的指针
 */
static void uthread_body(schedule_t *ps);

/*
 * 创建协程函数
 * 功能：在调度器中创建一个新的协程
 * 参数：
 *   schedule - 调度器引用
 *   func     - 协程要执行的函数指针
 *   arg      - 传递给协程函数的参数
 * 返回值：新创建协程的ID（索引），创建失败返回-1（虽然当前实现不会失败）
 */
int  uthread_create(schedule_t &schedule,Fun func,void *arg);
/*
 * 协程主动让出CPU
 * 功能：当前运行的协程主动暂停执行，切换回调度器主线程
 * 参数：schedule - 调度器引用
 * 注意：这是协作式调度的关键，协程必须主动调用此函数才能切换
 */
void uthread_yield(schedule_t &schedule);
/*
 * 恢复协程执行
 * 功能：唤醒指定ID的协程，使其从上次暂停的位置继续执行
 * 参数：
 *   schedule - 调度器引用
 *   id       - 要恢复的协程ID
 * 注意：只能恢复处于SUSPEND或RUNNABLE状态的协程
 */
void uthread_resume(schedule_t &schedule,int id);
/*
 * 检查调度是否完成
 * 功能：判断调度器中所有协程是否都已执行完毕
 * 参数：schedule - 调度器常量引用
 * 返回值：1-所有协程都已执行完成，0-还有协程未完成
 * 判断逻辑：
 *   - 如果有协程正在运行(running_thread != -1)，返回0
 *   - 如果任何协程状态不是FREE，返回0
 *   - 否则返回1
 */
int  schedule_finished(const schedule_t &schedule);

#endif
